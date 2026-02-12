#include "afv-native/audio/OutputDeviceState.h"

using namespace afv_native;

OutputDeviceState::OutputDeviceState():
    mChannelBuffer(audio::frameSizeSamples, 0),
    mMixingBuffer(audio::frameSizeSamples, 0),
    mFetchBuffer(audio::frameSizeSamples, 0),
    mLeftMixingBuffer(audio::frameSizeSamples, 0),
    mRightMixingBuffer(audio::frameSizeSamples, 0) {
}
