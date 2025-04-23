/* http/EventTransferManager.cpp
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2019 Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "afv-native/http/EventTransferManager.h"
#include "afv-native/http/Request.h"
#include "afv-native/Log.h"
#include <iostream>

using namespace afv_native::http;
using namespace std;

EventTransferManager::EventTransferManager(struct event_base *evBase):
    TransferManager(),
    mEvBase(evBase),
    mWatchedSockets(),
    mSocketPointers(),
    mTimerEvent(nullptr),
    mIsShuttingDown(false) {

    curl_multi_setopt(mCurlMultiHandle, CURLMOPT_SOCKETFUNCTION, EventTransferManager::curlSocketCallback);
    curl_multi_setopt(mCurlMultiHandle, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(mCurlMultiHandle, CURLMOPT_TIMERFUNCTION, EventTransferManager::curlTimerCallback);
    curl_multi_setopt(mCurlMultiHandle, CURLMOPT_TIMERDATA, this);
}

EventTransferManager::~EventTransferManager() {
    LOG("EventTransferManager", "Destructor called");
    shutdown();
}

void EventTransferManager::shutdown() {

    mIsShuttingDown = true;

    clearPendingTransfers();

    if (mTimerEvent != nullptr) {
        event_del(mTimerEvent);
        event_free(mTimerEvent);
        mTimerEvent = nullptr;
    }

    std::lock_guard<std::mutex> lock(mSocketsMutex);

    mWatchedSockets.clear();
    mSocketPointers.clear();

    LOG("EventTransferManager", "Shutdown completed");
}

void EventTransferManager::clearPendingTransfers() {
    LOG("EventTransferManager", "Clearing pending transfers");

    for (auto& transfer : mPendingTransfers) {
        if (transfer.second) {
            curl_multi_remove_handle(mCurlMultiHandle, transfer.first);
            transfer.second->notifyTransferError();
        }
    }

    mPendingTransfers.clear();
}

void EventTransferManager::process() {
    // NOP right now. Processing in the libevent version is driven externally.
}

int EventTransferManager::curlSocketCallback(CURL *easy, curl_socket_t s, int what, void *userp, void *socketp) {
    auto *etp = reinterpret_cast<EventTransferManager *>(userp);
    return etp->socketCallback(easy, s, what, socketp);
}

int EventTransferManager::socketCallback(CURL *easy, curl_socket_t s, int what, void *socketp) {
    if (mIsShuttingDown) {
        LOG("EventTransferManager", "Ignoring socket callback during shutdown");
        return 0;
    }

    std::lock_guard<std::mutex> lock(mSocketsMutex);

    SocketInfo *si = reinterpret_cast<SocketInfo *>(socketp);

    if (what == CURL_POLL_REMOVE) {
        if (si != nullptr) {
            auto it = mSocketPointers.find(si);
            if (it != mSocketPointers.end()) {
                auto siPtr = it->second;

                if (siPtr->ev != nullptr) {
                    event_del(siPtr->ev);
                    event_free(siPtr->ev);
                    siPtr->ev = nullptr;
                }

                siPtr->isValid = false;

                mWatchedSockets.erase(siPtr);
                mSocketPointers.erase(it);

                curl_multi_assign(mCurlMultiHandle, s, nullptr);
            }
        }
        return 0;
    } else if (what == CURL_POLL_IN || what == CURL_POLL_INOUT || what == CURL_POLL_OUT) {
        std::shared_ptr<SocketInfo> siPtr;

        if (si == nullptr) {
            siPtr = std::make_shared<SocketInfo>();
            si = siPtr.get();

            mWatchedSockets.insert(siPtr);
            mSocketPointers[si] = siPtr;

            curl_multi_assign(mCurlMultiHandle, s, si);
        } else {
            auto it = mSocketPointers.find(si);
            if (it != mSocketPointers.end()) {
                siPtr = it->second;
            } else {
                return 0;
            }
        }

        if (!siPtr->isValid) {
            return 0;
        }

        short ev_what = EV_PERSIST;
        if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) {
            ev_what |= EV_READ;
        }
        if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
            ev_what |= EV_WRITE;
        }

        if (siPtr->ev == nullptr) {
            siPtr->ev = event_new(mEvBase, s, ev_what, EventTransferManager::evSocketCallback, this);
        } else {
            event_del(siPtr->ev);
            event_assign(siPtr->ev, mEvBase, s, ev_what, EventTransferManager::evSocketCallback, this);
        }

        event_add(siPtr->ev, nullptr);
        return 0;
    }

    return 0;
}

void EventTransferManager::evSocketCallback(evutil_socket_t fd, short events, void *arg) {
    auto *etm = reinterpret_cast<EventTransferManager *>(arg);

    if (etm->mIsShuttingDown) {
        return;
    }

    int running_handles = 0;
    int curl_evmask = 0;

    if (events & EV_READ) {
        curl_evmask |= CURL_CSELECT_IN;
    }
    if (events & EV_WRITE) {
        curl_evmask |= CURL_CSELECT_OUT;
    }

    curl_multi_socket_action(etm->getCurlMultiHandle(), fd, curl_evmask, &running_handles);
    etm->processPendingMultiEvents();
}

void EventTransferManager::evTimerCallback(evutil_socket_t fd, short events, void *arg) {
    auto *etm = reinterpret_cast<EventTransferManager *>(arg);

    if (etm->mIsShuttingDown) {
        return;
    }

    int running_handles = 0;

    curl_multi_socket_action(etm->mCurlMultiHandle, CURL_SOCKET_TIMEOUT, 0, &running_handles);
    etm->processPendingMultiEvents();
}

int EventTransferManager::timerCallback(CURLM *multi, long timeout_ms) {
    if (mIsShuttingDown) {
        return 0;
    }

    if (mTimerEvent != nullptr) {
        event_del(mTimerEvent);
    }

    if (timeout_ms < 0) {
        return 0;
    } else {
        if (mTimerEvent == nullptr) {
            mTimerEvent = evtimer_new(mEvBase, EventTransferManager::evTimerCallback, this);
        }

        struct timeval tv = {0, 0};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        event_add(mTimerEvent, &tv);
    }

    return 0;
}

int EventTransferManager::curlTimerCallback(CURLM *multi, long timeout_ms, void *userp) {
    auto *etm = reinterpret_cast<EventTransferManager *>(userp);
    return etm->timerCallback(multi, timeout_ms);
}