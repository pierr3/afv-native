#include "afv-native/event/CallbackTimer.h"

using namespace afv_native::event;

namespace {
    class CallbackTimerTask: public Poco::Util::TimerTask {
      public:
        CallbackTimerTask(std::function<void()> cb, std::atomic<bool> &pending):
            mCb(std::move(cb)), mPending(pending) {
        }

        void run() override {
            mPending.store(false);
            if (mCb) {
                mCb();
            }
        }

      private:
        std::function<void()> mCb;
        std::atomic<bool>    &mPending;
    };
} // namespace

CallbackTimer::CallbackTimer(std::function<void()> callback):
    mCallback(std::move(callback)) {
}

CallbackTimer::~CallbackTimer() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mShuttingDown = true;
        if (mTask) {
            mTask->cancel();
        }
    }
    mTimer.cancel(true);
}

void CallbackTimer::enable(unsigned int delayMs) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mShuttingDown) {
        return;
    }
    if (mTask) {
        mTask->cancel();
    }
    mPending.store(true);
    mTask = new CallbackTimerTask(mCallback, mPending);
    mTimer.schedule(mTask, static_cast<long>(delayMs), 0);
}

void CallbackTimer::disable() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTask) {
        mTask->cancel();
    }
    mPending.store(false);
}

bool CallbackTimer::pending() const {
    return mPending.load();
}
