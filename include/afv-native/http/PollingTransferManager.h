#ifndef AFV_NATIVE_POLLINGTRANSFERMANAGER_H
#define AFV_NATIVE_POLLINGTRANSFERMANAGER_H

#include "afv-native/http/TransferManager.h"
#include <atomic>
#include <thread>

namespace afv_native { namespace http {

    class PollingTransferManager: public TransferManager {
      public:
        PollingTransferManager();
        ~PollingTransferManager() override;

        PollingTransferManager(const PollingTransferManager &) = delete;
        PollingTransferManager &operator=(const PollingTransferManager &) = delete;

        void process() override;

      private:
        std::thread       mPollThread;
        std::atomic<bool> mRunning{true};

        void pollLoop();
    };

}} // namespace afv_native::http

#endif // AFV_NATIVE_POLLINGTRANSFERMANAGER_H
