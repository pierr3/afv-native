/* http/TransferManager.h
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

#ifndef AFV_NATIVE_TRANSFERMANAGER_H
#define AFV_NATIVE_TRANSFERMANAGER_H

#include "afv-native/http/CurlPerform.h"
#include "afv-native/http/Request.h"
#include "afv-native/http/Response.h"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace afv_native { namespace http {

    /** Identifies a submitted request for cancellation.  0 is never valid. */
    using RequestHandle = std::uint64_t;

    using CompletionCallback = std::function<void(const Response &)>;

    /** Runs HTTP requests on a pool of worker threads and reports each result
     * on a single dispatch thread.
     *
     * Ownership rule: a Request is moved into the manager at submit() and is
     * never visible to the caller again.  Each worker builds its own CURL*,
     * performs it, and destroys it without the handle ever leaving that stack
     * frame, so no curl state is shared between threads - which is why there
     * is no multi handle, no share handle, and no lock callbacks here.
     *
     * Completion callbacks are serialised with respect to each other.  They
     * are NOT serialised against the application thread: a callback can run
     * while the application is calling into the same object, exactly as
     * before this class was rewritten.
     */
    class TransferManager {
      public:
        explicit TransferManager(unsigned workerCount = 4);
        virtual ~TransferManager();

        TransferManager(const TransferManager &)            = delete;
        TransferManager &operator=(const TransferManager &) = delete;

        /** Queues req.  cb runs on the dispatch thread once it finishes.
         *
         * @return a handle usable with cancel(), or 0 if the manager is
         *      shutting down and the request was not accepted.
         */
        RequestHandle submit(Request req, CompletionCallback cb);

        /** Aborts the request if it is still queued or in flight, and
         * suppresses its callback.  Safe to call with an unknown or already
         * completed handle.
         */
        void cancel(RequestHandle handle);

        /** Runs req on the calling thread.  Does not use the pool. */
        Response performSync(const Request &req);

        /** Retained for source compatibility with the old pull-based API.
         * Completions are delivered by the dispatch thread, so this does
         * nothing.
         */
        virtual void process();

      protected:
        struct Job {
            RequestHandle      handle = 0;
            Request            request;
            CompletionCallback callback;
            CancelFlag         cancel;
        };

        void workerLoop();
        void dispatchLoop();

        /** Refcounted curl_global_init/curl_global_cleanup.
         *
         * libcurl initialises itself lazily from curl_easy_init if this is
         * never called, but that path is explicitly not thread safe and the
         * workers below race straight into it.  Declared first so it runs
         * before any worker starts and is torn down after they have all
         * joined.
         */
        class CurlGlobalGuard {
          public:
            CurlGlobalGuard();
            ~CurlGlobalGuard();
            CurlGlobalGuard(const CurlGlobalGuard &)            = delete;
            CurlGlobalGuard &operator=(const CurlGlobalGuard &) = delete;
        };

        CurlGlobalGuard mCurlGlobalGuard;

        std::mutex              mMutex;
        std::condition_variable mWorkCv;
        std::condition_variable mDoneCv;
        bool                    mRunning = true;

        RequestHandle   mNextHandle = 1;
        std::deque<Job> mQueue;

        std::deque<std::pair<CompletionCallback, Response>> mCompleted;

        /** Cancel flags for queued and in-flight jobs, keyed by handle. */
        std::unordered_map<RequestHandle, CancelFlag> mLive;

        std::vector<std::thread> mWorkers;
        std::thread              mDispatcher;
    };

    /** There is no polling any more - the worker pool replaced it.  Kept so
     * code that names the old type still compiles.
     */
    using PollingTransferManager = TransferManager;

}} // namespace afv_native::http

#endif // AFV_NATIVE_TRANSFERMANAGER_H
