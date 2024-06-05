#pragma once
#include <afv-native/audio/audio_params.h>

namespace afv_native {
    class OutputDeviceState {
      public:
        /** mChannelBuffer is our single-radio/channel workbuffer - we do our
         * per-channel fx mixing in here before we mix into the mMixingBuffer
         */
        audio::SampleType *mChannelBuffer;

        /** mMixingBuffer is our aggregated mixing buffer for all radios/channels - when we're
         * finished mixing and the final effects pass, we copy this to the output/target buffer.
         */

        audio::SampleType *mMixingBuffer; // for single channel mode
        audio::SampleType *mLeftMixingBuffer;
        audio::SampleType *mRightMixingBuffer;
        audio::SampleType *mFetchBuffer;

        OutputDeviceState();
        virtual ~OutputDeviceState();
    };
} // namespace afv_native
