#ifndef AFV_NATIVE_CALLBACKTIMER_H
#define AFV_NATIVE_CALLBACKTIMER_H

#include <Poco/Util/Timer.h>
#include <Poco/Util/TimerTask.h>
#include <atomic>
#include <functional>
#include <mutex>

namespace afv_native { namespace event {

    class CallbackTimer {
      public:
        explicit CallbackTimer(std::function<void()> callback);
        ~CallbackTimer();

        CallbackTimer(const CallbackTimer &) = delete;
        CallbackTimer &operator=(const CallbackTimer &) = delete;

        void enable(unsigned int delayMs);
        void disable();
        bool pending() const;

      private:
        Poco::Util::Timer          mTimer;
        Poco::Util::TimerTask::Ptr mTask;
        std::function<void()>      mCallback;
        std::atomic<bool>          mPending{false};
        std::mutex                 mMutex;
        bool                       mShuttingDown{false};
    };

}} // namespace afv_native::event

#endif // AFV_NATIVE_CALLBACKTIMER_H
