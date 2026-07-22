#include "afv-native/http/PollingTransferManager.h"
#include "afv-native/Log.h"
#include "afv-native/http/Request.h"
#include <chrono>
#include <curl/curl.h>

using namespace afv_native::http;

PollingTransferManager::PollingTransferManager():
    TransferManager() {
    mPollThread = std::thread(&PollingTransferManager::pollLoop, this);
}

PollingTransferManager::~PollingTransferManager() {
    mRunning.store(false);
    curl_multi_wakeup(mCurlMultiHandle.get());
    if (mPollThread.joinable()) {
        mPollThread.join();
    }
}

void PollingTransferManager::process() {
    // NOP — the polling thread handles everything.
}

void PollingTransferManager::pollLoop() {
    LOG("PollingTransferManager", "Poll thread started");
    while (mRunning.load()) {
        std::vector<std::pair<Request *, bool>> completed;
        {
            std::lock_guard<std::recursive_mutex> lock(mMutex);
            // Tombstones only protect the completed list collected below from
            // requests reset after collection; older ones are stale and must
            // not suppress a resubmitted request's completion.
            clearCancellationTombstones();
            int running = 0;
            curl_multi_perform(mCurlMultiHandle.get(), &running);
            completed = collectCompletedTransfers();
        }
        // Completion callbacks re-enter client code (session state machines,
        // PTT handling) and may submit or cancel requests — they must run
        // without the CURLM lock held. A callback may also reset a request
        // that is further down this list; consumeCancellation catches that.
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
        {
            std::lock_guard<std::recursive_mutex> lock(mMutex);
            // curl_multi_poll must be under the same lock as perform/add/remove —
            // concurrent access to a CURLM handle is undefined behavior.
            // Other threads use curl_multi_wakeup() to interrupt the poll
            // when new work is queued.
            int numfds = 0;
            curl_multi_poll(mCurlMultiHandle.get(), nullptr, 0, 50, &numfds);
        }
        // The mutex is not fair: relocking immediately after release starves
        // threads blocked in submitRequest/cancelRequest and freezes the whole
        // client. Sleeping outside the lock guarantees them a window.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    LOG("PollingTransferManager", "Poll thread stopped");
}
