#pragma once
#include <afv-native/audio/audio_params.h>
#include <vector>

namespace afv_native {
    class OutputDeviceState {
      public:
        /** mChannelBuffer is our single-radio/channel workbuffer - we do our per-channel fx mixing
         * in here before we mix into the mMixingBuffer
         */
        std::vector<audio::SampleType> mChannelBuffer;

        /** mMixingBuffer is our aggregated mixing buffer for all radios/channels - when we're
         * finished mixing and the final effects pass, we copy this to the output/target buffer.
         */

        std::vector<audio::SampleType> mMixingBuffer; // for single channel mode
        std::vector<audio::SampleType> mLeftMixingBuffer;
        std::vector<audio::SampleType> mRightMixingBuffer;
        std::vector<audio::SampleType> mFetchBuffer;

        OutputDeviceState();
        virtual ~OutputDeviceState() = default;
    };
} // namespace afv_native
