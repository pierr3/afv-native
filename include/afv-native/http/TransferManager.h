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

#include <array>
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace afv_native { namespace http {

    struct CurlMultiDeleter {
        void operator()(CURLM *p) const { curl_multi_cleanup(p); }
    };
    struct CurlShareDeleter {
        void operator()(CURLSH *p) const { curl_share_cleanup(p); }
    };

    class Request;

    /** TransferManager manages all of the running HTTP/HTTPS transfers, making sure
     * completion notifications get fired, etc - and generally keeping things
     * running without blocking the thread.  It also keeps SSL session and
     * cookie data to share between requests.
     */
    class TransferManager {
      protected:
        /** Refcounted curl_global_init/curl_global_cleanup.
         *
         * libcurl will initialise itself lazily from curl_easy_init if this is
         * never called, but that path is explicitly not thread safe, and we
         * create requests from the poll thread, the session timer threads and
         * the caller's thread.  Doing it here covers every entry point without
         * adding an init call to the public API - and doing it from a member
         * declared first means it runs before curl_multi_init below and is
         * torn down after both handles are gone.
         */
        class CurlGlobalGuard {
          public:
            CurlGlobalGuard();
            ~CurlGlobalGuard();
            CurlGlobalGuard(const CurlGlobalGuard &)            = delete;
            CurlGlobalGuard &operator=(const CurlGlobalGuard &) = delete;
        };

        CurlGlobalGuard mCurlGlobalGuard;

        /** Per-datum locks for the share handle.  libcurl requires these
         * whenever a share is used from more than one thread, which it is here:
         * easy handles are created and destroyed on session timer threads while
         * the poll thread is inside curl_multi_perform.
         *
         * Must be declared before the handles below - curl_share_cleanup calls
         * back into these, and members die in reverse declaration order.
         */
        std::array<std::mutex, CURL_LOCK_DATA_LAST> mShareLocks;

        std::unique_ptr<CURLM, CurlMultiDeleter>  mCurlMultiHandle;
        std::unique_ptr<CURLSH, CurlShareDeleter> mCurlShareHandle;

        std::unordered_map<CURL *, Request *> mPendingTransfers;

        /** Requests cancelled via cancelRequest since the start of the current
         * dispatch cycle. A request can be reset/destroyed after it was
         * collected for completion dispatch but before its callback ran; the
         * dispatch loop checks this set (consumeCancellation) before invoking
         * a callback. The set is cleared (clearCancellationTombstones) at the
         * START of each cycle, atomically with collection, so stale tombstones
         * cannot suppress the completion of a request that was reset and then
         * resubmitted.
         */
        std::unordered_set<Request *> mCancelledDuringDispatch;

        std::recursive_mutex mMutex;

        static void curlShareLock(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr);
        static void curlShareUnlock(CURL *handle, curl_lock_data data, void *userptr);

        /** Returns true (and forgets the tombstone) if the request was
         * cancelled since the start of the current dispatch cycle. */
        bool consumeCancellation(Request *req);

        /** Drops all cancellation tombstones. Call while holding mMutex at the
         * start of a dispatch cycle, before collecting completed transfers. */
        void clearCancellationTombstones();

        /** collectCompletedTransfers reconciles any outstanding completion
         * notifications from curl, detaches the finished easy handles from the
         * multi handle, and returns the affected requests with their success
         * state.  The caller must hold mMutex.  Completion callbacks are NOT
         * invoked here — the caller must invoke notifyTransferCompleted /
         * notifyTransferError on the returned requests AFTER releasing mMutex,
         * as callbacks re-enter client code and may submit or cancel requests.
         */
        std::vector<std::pair<Request *, bool>> collectCompletedTransfers();

      public:
        TransferManager();

        /* no copy constructor - TransferManger must not be copied as it would break the internal states. */
        TransferManager(const TransferManager &cpysrc) = delete;

        /** Joins a Request to the shared state carried by this TransferManager.
         *
         * @param req the Request object to join to the shared state.
         */
        [[deprecated("Use the shareState method on Request instead")]] void AddToSession(Request *req) const;

        void registerForAsyncCallback(Request &req);
        void removeAsyncCallback(Request &req);

        /** Schedules a Request to be processed asynchronously by this
         * TransferManager.  Once you call this method on a Request object,
         * you must not invoke it's local doSync() method.
         *
         * @param req The Request object to process
         */
        [[deprecated("Use the doAsync method on Request instead")]] virtual void HandleRequest(Request *req);

        /** Thread-safe request submission. Acquires the CURLM lock, adds the
         * handle, registers the callback, and wakes the poll thread. */
        void submitRequest(Request *req);

        /** Thread-safe request cancellation. Acquires the CURLM lock, removes
         * the easy handle from the multi handle, and deregisters the callback.
         * Safe to call from any thread, including while the poll thread is
         * inside curl_multi_perform / curl_multi_poll. */
        void cancelRequest(Request *req);

        virtual ~TransferManager();

        /** Process any outstanding events without blocking. */
        virtual void process();

        /** Return the internal CURLM handle
         *
         * @note This is not guaranteed to be available in the future.
         *
         * @return the internal CURLM handle
         */
        CURLM *getCurlMultiHandle() const;
        CURLSH *getCurlShareHandle() const;
    };
}}     // namespace afv_native::http
#endif // AFV_NATIVE_TRANSFERMANAGER_H
