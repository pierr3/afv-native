/* afv/RadioSimulation.h
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2019 Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef AFV_NATIVE_RADIOSIMULATION_H
#define AFV_NATIVE_RADIOSIMULATION_H

#include <memory>
#include <unordered_map>

#include "afv-native/utility.h"
#include "afv-native/event.h"
#include "afv-native/afv/EffectResources.h"
#include "afv-native/afv/RemoteVoiceSource.h"
#include "afv-native/afv/RollingAverage.h"
#include "afv-native/afv/VoiceCompressionSink.h"
#include "afv-native/afv/dto/voice_server/AudioRxOnTransceivers.h"
#include "afv-native/audio/ISampleSink.h"
#include "afv-native/audio/ISampleSource.h"
#include "afv-native/audio/SineToneSource.h"
#include "afv-native/audio/SpeexPreprocessor.h"
#include "afv-native/audio/SimpleCompressorEffect.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/cryptodto/UDPChannel.h"
#include "afv-native/event/EventCallbackTimer.h"
#include "afv-native/util/ChainedCallback.h"

namespace afv_native {
    namespace afv {

        class RadioSimulation;

        class OutputAudioDevice : public audio::ISampleSource {
        public:
            OutputAudioDevice(std::weak_ptr<RadioSimulation> radio, bool onHeadset);
            audio::SourceStatus getAudioFrame(audio::SampleType *bufferOut) override;
        private:
            std::weak_ptr<RadioSimulation> mRadio;
            bool onHeadset = false;
        };

        class OutputDeviceState {
        public:
            audio::SampleType *mChannelBuffer;
            audio::SampleType *mMixingBuffer; // for single channel mode
            audio::SampleType *mLeftMixingBuffer;
            audio::SampleType *mRightMixingBuffer;
            audio::SampleType *mFetchBuffer;
            OutputDeviceState();
            virtual ~OutputDeviceState();
        };

        /** RadioState is the internal state object for each radio within a RadioSimulation.
         *
         * It tracks the current playback position of the mixing effects, the channel frequency and gain.
         */
        class RadioState {
        public:
            unsigned int Frequency;
            float Gain = 1.0;
            std::shared_ptr<audio::RecordedSampleSource> Click;
            std::shared_ptr<audio::RecordedSampleSource> Crackle;
            std::shared_ptr<audio::RecordedSampleSource> AcBus;
            std::shared_ptr<audio::RecordedSampleSource> VhfWhiteNoise;
            std::shared_ptr<audio::RecordedSampleSource> HfWhiteNoise;
            std::shared_ptr<audio::SineToneSource> BlockTone;
            audio::SimpleCompressorEffect simpleCompressorEffect;
            audio::VHFFilterSource vhfFilter;
            int mLastRxCount;
            bool mBypassEffects;
            bool mHfSquelch;
            bool mIsReceiving;
            bool onHeadset = true;
        };

        /** CallsignMeta is the per-packetstream metadata stored within the RadioSimulation object.
         *
         * It's used to hold the RemoteVoiceSource object for that callsign+channel combination,
         * and the list of transceivers that this packet stream relates to.
         */
        struct CallsignMeta {
            std::shared_ptr<RemoteVoiceSource> source;
            std::vector<dto::RxTransceiver> transceivers;
            CallsignMeta();
        };

        enum class RadioSimulationState
        {
            RxStarted,
            RxStopped
        };

        /** RadioSimulation provides the foundation for handling radio channels and mixing them
         * into an audio stream, as well as handling the samples from the micrphone input.
         *
         * The RadioSimulation object itself provides both an ISampleSource (the output from the radio
         * stack) and an ISampleSink (the input into the voice transmission path).
         *
         * The implementation assumes continuous recording (and hence input into the ISampleSink methods)
         * and instead provides the Ptt functions to control the conversion of said input into voice
         * packets.
         */
        class RadioSimulation:
                public std::enable_shared_from_this<RadioSimulation>,
                public audio::ISampleSink,
                public ICompressedFrameSink {
        public:
            RadioSimulation(
                    struct event_base *evBase,
                    std::shared_ptr<EffectResources> resources,
                    cryptodto::UDPChannel *channel,
                    unsigned int radioCount);
            virtual ~RadioSimulation();

            RadioSimulation(const RadioSimulation& copySrc) = delete;

            void rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt);

            void setCallsign(const std::string &newCallsign);
            void setFrequency(unsigned int radio, unsigned int frequency);
            void setGain(unsigned int radio, float gain);
            void setTxRadio(unsigned int radio);

            void setMicrophoneVolume(float volume);

            bool getTxActive(unsigned int radio);
            bool getRxActive(unsigned int radio);

            void setPtt(bool pressed);
            void setUDPChannel(cryptodto::UDPChannel *newChannel);

            double getVu() const;
            double getPeak() const;

            void reset();

            bool getEnableInputFilters() const;
            void setEnableInputFilters(bool enableInputFilters);

            void setEnableOutputEffects(bool enableEffects);
            void setEnableHfSquelch(bool enableHfSquelch);

            void setupDevices(util::ChainedCallback<void(ClientEventType, void*, void*)> *eventCallback);

            void setOnHeadset(unsigned int radio, bool onHeadset);
            void setSplitAudioChannels(bool splitChannels);

            void putAudioFrame(const audio::SampleType *bufferIn) override;
            audio::SourceStatus getAudioFrame(audio::SampleType *bufferOut, bool onHeadset);

            /** Contains the number of IncomingAudioStreams known to the simulation stack */
            std::atomic<uint32_t> IncomingAudioStreams;

            int lastReceivedRadio() const;
            util::ChainedCallback<void(RadioSimulationState)>  RadioStateCallback;

            std::shared_ptr<audio::ISampleSource> speakerDevice() { return mSpeakerDevice; }
            std::shared_ptr<audio::ISampleSource> headsetDevice() { return mHeadsetDevice; }

        protected:
            /** maintenanceTimerIntervalMs is the internal in milliseconds between periodic cleanups
             * of the inbound audio frame objects.
             *
             * This maintenance occurs in the main execution thread as not to hold the audio playback thread
             * unnecessarily, particularly as it may involve alloc/free operations.
             *
             * The current constant of 30s should be frequent enough to prevent massive memory issues, without
             * causing performance issues.
             */
            static const int maintenanceTimerIntervalMs = 30 * 1000; /* every 30s */

            util::ChainedCallback<void(ClientEventType, void*, void*)>  *ClientEventCallback;

            struct event_base *mEvBase;
            std::shared_ptr<EffectResources> mResources;
            cryptodto::UDPChannel *mChannel;
            std::string mCallsign;

            std::mutex mStreamMapLock;
            std::unordered_map<std::string, struct CallsignMeta> mHeadsetIncomingStreams;
            std::unordered_map<std::string, struct CallsignMeta> mSpeakerIncomingStreams;

            std::mutex mRadioStateLock;
            std::atomic<bool> mPtt;
            bool mLastFramePtt;
            unsigned int mTxRadio;
            std::atomic<uint32_t> mTxSequence;
            std::vector<RadioState> mRadioState;

            bool mSplitChannels = false;

            std::shared_ptr<OutputAudioDevice> mHeadsetDevice;
            std::shared_ptr<OutputAudioDevice> mSpeakerDevice;

            std::shared_ptr<OutputDeviceState> mHeadsetState;
            std::shared_ptr<OutputDeviceState> mSpeakerState;

            float mMicVolume = 1.0f;

            unsigned int mLastReceivedRadio;

            std::shared_ptr<VoiceCompressionSink> mVoiceSink;
            std::shared_ptr<audio::SpeexPreprocessor> mVoiceFilter;

            event::EventCallbackTimer mMaintenanceTimer;
            RollingAverage<double> mVuMeter;

            void resetRadioFx(unsigned int radio, bool except_click = false);

            void set_radio_effects(size_t rxIter);

            bool mix_effect(std::shared_ptr<audio::ISampleSource> effect, float gain, std::shared_ptr<OutputDeviceState> state);

            void processCompressedFrame(std::vector<unsigned char> compressedData) override;

            static void dtoHandler(
                    const std::string &dtoName, const unsigned char *bufIn, size_t bufLen, void *user_data);
            void instDtoHandler(
                    const std::string &dtoName, const unsigned char *bufIn, size_t bufLen);

            void maintainIncomingStreams();
        private:
            bool _process_radio(
                    const std::map<void *, audio::SampleType[audio::frameSizeSamples]> &sampleCache,
                    size_t rxIter,
                    bool onHeadset);

            inline void interleave(audio::SampleType* leftChannel, audio::SampleType* rightChannel, audio::SampleType* outputBuffer, size_t numSamples) {
                for (size_t i = 0; i < numSamples; i++) {
                    outputBuffer[2 * i] = leftChannel[i]; // Interleave left channel data
                    outputBuffer[2 * i + 1] = rightChannel[i]; // Interleave right channel data
                }
            }

            /** mix_buffers is a utility function that mixes two buffers of audio together.  The src_dst
             * buffer is assumed to be the final output buffer and is modified by the mixing in place.
             * src2 is read-only and will be scaled by the provided linear gain.
             *
             * @note as this mixer has some really fundamental assumptions about buffer alignment which
             * are required as it uses SSE2 intrinsics to perform the mixing.  These requirements are
             * met inside elsewhere inside this class.
             *
             * @param src_dst pointer to the source and destination buffer.
             * @param src2 pointer to the origin of the samples to mix in.
             * @param src2_gain linear gain to apply to src2.
             */
            static void mix_buffers(audio::SampleType* RESTRICT src_dst, const audio::SampleType* RESTRICT src2, float src2_gain = 1.0);
        };
    }
}

#endif //AFV_NATIVE_RADIOSIMULATION_H
