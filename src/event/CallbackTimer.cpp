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

    // One-shot. The schedule(task, delay, interval) overload is the PERIODIC
    // one: passing 0 as the interval re-runs the task with no delay, forever,
    // from the moment it first fires. Every caller here re-arms from inside its
    // own callback, so a repeating task is never what is wanted.
    Poco::Clock clock;
    clock += static_cast<Poco::Clock::ClockDiff>(delayMs) * 1000; // ms -> us
    mTimer.schedule(mTask, clock);
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
