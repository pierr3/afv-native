#include "afv-native/http/PollingTransferManager.h"
#include "afv-native/Log.h"
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
        {
            std::lock_guard<std::recursive_mutex> lock(mMutex);
            int running = 0;
            curl_multi_perform(mCurlMultiHandle.get(), &running);
            processPendingMultiEvents();
        }
        int numfds = 0;
        curl_multi_poll(mCurlMultiHandle.get(), nullptr, 0, 100, &numfds);
    }
    LOG("PollingTransferManager", "Poll thread stopped");
}
