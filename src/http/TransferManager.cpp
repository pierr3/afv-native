/* http/TransferManager.cpp
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

#include "afv-native/http/TransferManager.h"
#include "afv-native/http/Request.h"
#include <curl/curl.h>

using namespace afv_native::http;

TransferManager::TransferManager():
    mCurlMultiHandle(curl_multi_init()),
    mCurlShareHandle(curl_share_init()),
    mPendingTransfers() {
    // Must be installed before anything is shared.
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_LOCKFUNC, &TransferManager::curlShareLock);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_UNLOCKFUNC, &TransferManager::curlShareUnlock);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_USERDATA, this);

    // share everything.
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(mCurlShareHandle.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);
}

void TransferManager::curlShareLock(CURL *, curl_lock_data data, curl_lock_access, void *userptr) {
    auto *tm = static_cast<TransferManager *>(userptr);
    if (tm == nullptr || data >= CURL_LOCK_DATA_LAST) {
        return;
    }
    // Exclusive for both SHARED and SINGLE access; these are short cache lookups.
    tm->mShareLocks[static_cast<size_t>(data)].lock();
}

void TransferManager::curlShareUnlock(CURL *, curl_lock_data data, void *userptr) {
    auto *tm = static_cast<TransferManager *>(userptr);
    if (tm == nullptr || data >= CURL_LOCK_DATA_LAST) {
        return;
    }
    tm->mShareLocks[static_cast<size_t>(data)].unlock();
}

TransferManager::~TransferManager() {
    // First, remove all easy handles from the multi handle
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    for (auto const &pair: mPendingTransfers) {
        curl_multi_remove_handle(mCurlMultiHandle.get(), pair.first);
    }

    // Clear pending transfers before unique_ptrs clean up the handles
    mPendingTransfers.clear();
}

void TransferManager::process() {
    std::vector<std::pair<Request *, bool>> completed;
    {
        std::lock_guard<std::recursive_mutex> lock(mMutex);
        int                                   running = 0;

        // Tombstones only protect the completed list collected below from
        // requests reset after collection; older ones are stale and must not
        // suppress a resubmitted request's completion.
        clearCancellationTombstones();

        curl_multi_perform(mCurlMultiHandle.get(), &running);

        completed = collectCompletedTransfers();
    }
    for (auto &[req, ok]: completed) {
        if (consumeCancellation(req)) {
            continue;
        }
        if (ok) {
            req->notifyTransferCompleted();
        } else {
            req->notifyTransferError();
        }
    }
}

std::vector<std::pair<Request *, bool>> TransferManager::collectCompletedTransfers() {
    std::vector<std::pair<Request *, bool>> completed;
    struct CURLMsg                         *cMsg        = nullptr;
    int                                     msgs_queued = 0;

    while (nullptr != (cMsg = curl_multi_info_read(mCurlMultiHandle.get(), &msgs_queued))) {
        // GAH.  STUPID STUPID CURL.  Never return pointers from stack or other transient memory.
        auto msgCopy = *cMsg;

        if (msgCopy.msg != CURLMSG_DONE) {
            continue;
        }
        // remove the easy handle from our management
        curl_multi_remove_handle(mCurlMultiHandle.get(), msgCopy.easy_handle);
        auto it = mPendingTransfers.find(msgCopy.easy_handle);
        if (it == mPendingTransfers.end()) {
            // request was cancelled while the completion was queued
            continue;
        }
        Request *req = it->second;
        mPendingTransfers.erase(it);
        // read the status code / content type while we still hold the lock,
        // so the callback can run lock-free later without touching the
        // easy handle concurrently with anyone resetting the request.
        req->captureResponseInfo();
        completed.emplace_back(req, msgCopy.data.result == CURLE_OK);
    }
    return completed;
}

void TransferManager::AddToSession(Request *req) const {
    if (req) {
        curl_easy_setopt(req->getCurlHandle(), CURLOPT_SHARE, mCurlShareHandle.get());
    }
}

void TransferManager::HandleRequest(Request *req) {
    submitRequest(req);
}

void TransferManager::submitRequest(Request *req) {
    if (req) {
        std::lock_guard<std::recursive_mutex> lock(mMutex);
        auto curlHandle               = req->getCurlHandle();
        mPendingTransfers[curlHandle] = req;
        curl_multi_add_handle(mCurlMultiHandle.get(), curlHandle);
        curl_multi_wakeup(mCurlMultiHandle.get());
    }
}

void TransferManager::cancelRequest(Request *req) {
    if (!req) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    auto h = req->getCurlHandle();
    if (h) {
        curl_multi_remove_handle(mCurlMultiHandle.get(), h);
        mPendingTransfers.erase(h);
    }
    // The request may already have been collected for completion dispatch and
    // be awaiting its callback outside the lock. Tombstone it so the dispatch
    // loop skips it instead of touching a reset (or destroyed) request.
    mCancelledDuringDispatch.insert(req);
}

bool TransferManager::consumeCancellation(Request *req) {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    return mCancelledDuringDispatch.erase(req) > 0;
}

void TransferManager::clearCancellationTombstones() {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    mCancelledDuringDispatch.clear();
}

CURLM *TransferManager::getCurlMultiHandle() const {
    return mCurlMultiHandle.get();
}

CURLSH *TransferManager::getCurlShareHandle() const {
    return mCurlShareHandle.get();
}

void TransferManager::registerForAsyncCallback(Request &req) {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    auto curlHandle = req.getCurlHandle();
    if (curlHandle != nullptr) {
        mPendingTransfers[curlHandle] = &req;
    }
}

void TransferManager::removeAsyncCallback(Request &req) {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    auto curlHandle = req.getCurlHandle();
    if (curlHandle != nullptr) {
        mPendingTransfers.erase(curlHandle);
    }
}
