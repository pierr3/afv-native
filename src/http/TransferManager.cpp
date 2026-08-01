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
#include <curl/curl.h>
#include <utility>

using namespace afv_native::http;

namespace {
    std::mutex gCurlGlobalMutex;
    unsigned   gCurlGlobalRefCount = 0;
} // namespace

TransferManager::CurlGlobalGuard::CurlGlobalGuard() {
    std::lock_guard<std::mutex> lock(gCurlGlobalMutex);
    if (gCurlGlobalRefCount++ == 0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
}

TransferManager::CurlGlobalGuard::~CurlGlobalGuard() {
    std::lock_guard<std::mutex> lock(gCurlGlobalMutex);
    if (--gCurlGlobalRefCount == 0) {
        curl_global_cleanup();
    }
}

TransferManager::TransferManager(unsigned workerCount) {
    if (workerCount == 0) {
        workerCount = 1;
    }
    mWorkers.reserve(workerCount);
    for (unsigned i = 0; i < workerCount; ++i) {
        mWorkers.emplace_back(&TransferManager::workerLoop, this);
    }
    mDispatcher = std::thread(&TransferManager::dispatchLoop, this);
}

TransferManager::~TransferManager() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRunning = false;
        // Abort anything already handed to a worker so shutdown does not have
        // to wait out a full transfer timeout.
        for (auto &entry: mLive) {
            if (entry.second) {
                entry.second->store(true);
            }
        }
        mQueue.clear();
        mCompleted.clear();
    }
    mWorkCv.notify_all();
    mDoneCv.notify_all();

    for (auto &worker: mWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    if (mDispatcher.joinable()) {
        mDispatcher.join();
    }
}

RequestHandle TransferManager::submit(Request req, CompletionCallback cb) {
    Job job;
    job.request  = std::move(req);
    job.callback = std::move(cb);
    job.cancel   = std::make_shared<std::atomic<bool>>(false);

    RequestHandle handle = 0;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning) {
            return 0;
        }
        handle        = mNextHandle++;
        job.handle    = handle;
        mLive[handle] = job.cancel;
        mQueue.push_back(std::move(job));
    }
    mWorkCv.notify_one();
    return handle;
}

void TransferManager::cancel(RequestHandle handle) {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mLive.find(handle);
    if (it != mLive.end() && it->second) {
        it->second->store(true);
    }

    // Drop it outright if no worker has picked it up yet.
    for (auto queued = mQueue.begin(); queued != mQueue.end(); ++queued) {
        if (queued->handle == handle) {
            mQueue.erase(queued);
            mLive.erase(handle);
            return;
        }
    }
}

Response TransferManager::performSync(const Request &req) {
    return curlPerform(req, nullptr);
}

void TransferManager::process() {
    // Completions are delivered by the dispatch thread; nothing to pump.
}

void TransferManager::workerLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mWorkCv.wait(lock, [this] { return !mRunning || !mQueue.empty(); });
            if (!mRunning) {
                return;
            }
            job = std::move(mQueue.front());
            mQueue.pop_front();
        }

        Response resp = curlPerform(job.request, job.cancel);

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mLive.erase(job.handle);
            // Cancelled requests, and anything outstanding at shutdown, report
            // nothing.
            const bool suppressed = resp.cancelled || !mRunning;
            if (!suppressed && job.callback) {
                mCompleted.emplace_back(std::move(job.callback), std::move(resp));
            }
        }
        mDoneCv.notify_one();
    }
}

void TransferManager::dispatchLoop() {
    for (;;) {
        std::pair<CompletionCallback, Response> item;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mDoneCv.wait(lock, [this] { return !mRunning || !mCompleted.empty(); });
            if (!mRunning && mCompleted.empty()) {
                return;
            }
            if (mCompleted.empty()) {
                continue;
            }
            item = std::move(mCompleted.front());
            mCompleted.pop_front();
        }
        // Invoked outside the lock: callbacks re-enter client code and may
        // submit or cancel further requests.
        if (item.first) {
            item.first(item.second);
        }
    }
}
