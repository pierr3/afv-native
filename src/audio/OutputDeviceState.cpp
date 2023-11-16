#include "afv-native/audio/OutputDeviceState.h"

using namespace afv_native;

OutputDeviceState::OutputDeviceState() {
    mChannelBuffer     = new audio::SampleType[audio::frameSizeSamples];
    mMixingBuffer      = new audio::SampleType[audio::frameSizeSamples];
    mFetchBuffer       = new audio::SampleType[audio::frameSizeSamples];
    mLeftMixingBuffer  = new audio::SampleType[audio::frameSizeSamples];
    mRightMixingBuffer = new audio::SampleType[audio::frameSizeSamples];
}

OutputDeviceState::~OutputDeviceState() {
    delete[] mFetchBuffer;
    delete[] mMixingBuffer;
    delete[] mLeftMixingBuffer;
    delete[] mRightMixingBuffer;
    delete[] mChannelBuffer;
}