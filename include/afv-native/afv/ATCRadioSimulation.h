/* afv/ATCRadioSimulation.h
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

#include "afv-native/Log.h"
#include "afv-native/afv/EffectResources.h"
#include "afv-native/afv/RemoteVoiceSource.h"
#include "afv-native/afv/RollingAverage.h"
#include "afv-native/afv/VoiceCompressionSink.h"
#include "afv-native/afv/dto/CrossCoupleGroup.h"
#include "afv-native/afv/dto/StationTransceiver.h"
#include "afv-native/afv/dto/Transceiver.h"
#include "afv-native/afv/dto/voice_server/AudioRxOnTransceivers.h"
#include "afv-native/afv/dto/voice_server/AudioTxOnTransceivers.h"
#include "afv-native/audio/ISampleSink.h"
#include "afv-native/audio/ISampleSource.h"
#include "afv-native/audio/ITick.h"
#include "afv-native/audio/OutputDeviceState.h"
#include "afv-native/audio/PinkNoiseGenerator.h"
#include "afv-native/audio/SimpleCompressorEffect.h"
#include "afv-native/audio/SineToneSource.h"
#include "afv-native/audio/SpeexPreprocessor.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/cryptodto/UDPChannel.h"
#include "afv-native/event.h"
#include "afv-native/event/EventCallbackTimer.h"
#include "afv-native/hardwareType.h"
#include "afv-native/util/ChainedCallback.h"
#include "afv-native/util/other.h"
#include "afv-native/utility.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace afv_native { namespace afv {

    class ATCRadioSimulation;

    class AtcOutputAudioDevice: public audio::ISampleSource {
      public:
        AtcOutputAudioDevice(std::weak_ptr<ATCRadioSimulation> radio, bool onHeadset);
        audio::SourceStatus getAudioFrame(audio::SampleType *bufferOut) override;

      private:
        std::weak_ptr<ATCRadioSimulation> mRadio;
        bool                              onHeadset = false;
    };

    /** RadioState is the internal state object for each radio within a ATCRadioSimulation.
     *
     * It tracks the current playback position of the mixing effects, the channel frequency and gain.
     */
    class AtcRadioState {
      public:
        unsigned int                                 Frequency;
        float                                        Gain = 1.0;
        std::shared_ptr<audio::RecordedSampleSource> Click;
        std::shared_ptr<audio::RecordedSampleSource> Crackle;
        std::shared_ptr<audio::RecordedSampleSource> AcBus;
        std::shared_ptr<audio::RecordedSampleSource> VhfWhiteNoise;
        std::shared_ptr<audio::RecordedSampleSource> HfWhiteNoise;
        std::shared_ptr<audio::SineToneSource>       BlockTone;
        audio::SimpleCompressorEffect                simpleCompressorEffect;
        std::shared_ptr<audio::VHFFilterSource>      vhfFilter;
        int                                          mLastRxCount;
        bool                                         mBypassEffects = false;
        bool                                         mHfSquelch     = false;
        bool                                         onHeadset         = true;
        bool                                         tx                = false;
        bool                                         rx                = true;
        bool                                         xc                = false;
        bool                                         crossCoupleAcross = false;
        std::string                                  stationName       = "";
        std::vector<dto::Transceiver>                transceivers;
        HardwareType    simulatedHardware = HardwareType::Schmid_ED_137B;
        bool            isATIS            = false;
        PlaybackChannel playbackChannel   = PlaybackChannel::Both;

        std::string              lastTransmitCallsign      = "";
        std::vector<std::string> liveTransmittingCallsigns = {};
    };

    /** CallsignMeta is the per-packetstream metadata stored within the ATCRadioSimulation object.
     *
     * It's used to hold the RemoteVoiceSource object for that callsign+channel combination,
     * and the list of transceivers that this packet stream relates to.
     */
    struct AtcCallsignMeta {
        std::shared_ptr<RemoteVoiceSource> source;
        std::vector<dto::RxTransceiver>    transceivers;
        AtcCallsignMeta();
    };

    enum class AtcRadioSimulationState {
        RxStarted,
        RxStopped
    };

    /** ATCRadioSimulation provides the foundation for handling radio channels and mixing them
     * into an audio stream, as well as handling the samples from the micrphone input.
     *
     * The ATCRadioSimulation object itself provides both an ISampleSource (the output from the
     * radio stack) and an ISampleSink (the input into the voice transmission path).
     *
     * The implementation assumes continuous recording (and hence input into the ISampleSink
     * methods) and instead provides the Ptt functions to control the conversion of said input into
     * voice packets.
     */
    class ATCRadioSimulation: public std::enable_shared_from_this<ATCRadioSimulation>, public audio::ISampleSink, public ICompressedFrameSink {
      public:
        ATCRadioSimulation(struct event_base *evBase, std::shared_ptr<EffectResources> resources, cryptodto::UDPChannel *channel);
        virtual ~ATCRadioSimulation();

        ATCRadioSimulation(const ATCRadioSimulation &copySrc) = delete;

        bool _packetListening(const afv::dto::AudioRxOnTransceivers &pkt);
        void rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt);

        void setCallsign(const std::string &newCallsign);
        void setClientPosition(double lat, double lon, double amslm, double aglm);

        bool addFrequency(unsigned int radio, bool onHeadset, std::string stationName = "", HardwareType hardware = HardwareType::Schmid_ED_137B, PlaybackChannel channel = PlaybackChannel::Both);
        void removeFrequency(unsigned int freq);
        bool isFrequencyActive(unsigned int freq);
        bool isFrequencyActiveButUnused(unsigned int freq);

        std::map<unsigned int, AtcRadioState> getRadioState();

        bool getRxState(unsigned int freq);
        bool getTxState(unsigned int freq);
        bool getXcState(unsigned int freq);
        bool getCrossCoupleAcrossState(unsigned int freq);

        void setRx(unsigned int freq, bool rx);
        void setTx(unsigned int freq, bool tx);
        void setXc(unsigned int freq, bool xc);
        void setCrossCoupleAcross(unsigned int freq, bool crossCoupleAcross);

        bool getTxActive(unsigned int radio);
        bool getRxActive(unsigned int radio);

        std::string getLastTransmitOnFreq(unsigned int freq);
        /**
         * Returns the number of transceivers for a given frequency.
         *
         * @param freq The frequency for which to retrieve the transceiver count.
         * @return The number of transceivers for the given frequency.
         */
        int getTransceiverCountForFrequency(unsigned int freq);

        void setGain(unsigned int radio, float gain);
        void setGainAll(float gain);

        void setPlaybackChannelAll(PlaybackChannel channel);
        void setPlaybackChannel(unsigned int freq, PlaybackChannel channel);
        afv_native::PlaybackChannel getPlaybackChannel(unsigned int freq);

        void setMicrophoneVolume(float volume);

        void setTransceivers(unsigned int freq, std::vector<afv::dto::StationTransceiver> transceivers);
        std::vector<afv::dto::Transceiver>      makeTransceiverDto();
        std::vector<afv::dto::CrossCoupleGroup> makeCrossCoupleGroupDto();

        void setPtt(bool pressed);
        void setUDPChannel(cryptodto::UDPChannel *newChannel);

        double getVu() const;
        double getPeak() const;

        void reset();

        bool getEnableInputFilters() const;
        void setEnableInputFilters(bool enableInputFilters);

        void setEnableOutputEffects(bool enableEffects);
        void setEnableHfSquelch(bool enableHfSquelch);

        void setupDevices(util::ChainedCallback<void(ClientEventType, void *, void *)> *eventCallback);

        void setOnHeadset(unsigned int radio, bool onHeadset);
        bool getOnHeadset(unsigned int freq);

        void stationTransceiverUpdateCallback(const std::string &stationName, std::map<std::string, std::vector<afv::dto::StationTransceiver>> transceivers);

        void putAudioFrame(const audio::SampleType *bufferIn) override;
        audio::SourceStatus getAudioFrame(audio::SampleType *bufferOut, bool onHeadset);

        /** Contains the number of IncomingAudioStreams known to the simulation stack */
        std::atomic<uint32_t> IncomingAudioStreams;

        void setTick(std::shared_ptr<audio::ITick> tick);

        int lastReceivedRadio() const;
        util::ChainedCallback<void(AtcRadioSimulationState)> RadioStateCallback;

        std::shared_ptr<audio::ISampleSource> speakerDevice() {
            return mSpeakerDevice;
        }
        std::shared_ptr<audio::ISampleSource> headsetDevice() {
            return mHeadsetDevice;
        }

      protected:
        /** maintenanceTimerIntervalMs is the internal in milliseconds between periodic cleanups
         * of the inbound audio frame objects.
         *
         * This maintenance occurs in the main execution thread as not to hold the audio playback
         * thread unnecessarily, particularly as it may involve alloc/free operations.
         *
         * The current constant of 30s should be frequent enough to prevent massive memory issues,
         * without causing performance issues.
         */
        static const int maintenanceTimerIntervalMs = 30 * 1000; /* every 30s */

        util::ChainedCallback<void(ClientEventType, void *, void *)> *ClientEventCallback;

        struct event_base               *mEvBase;
        std::shared_ptr<EffectResources> mResources;
        cryptodto::UDPChannel           *mChannel;
        std::string                      mCallsign;
        double                           mClientLatitude     = 0.0;
        double                           mClientLongitude    = 0.0;
        double                           mClientAltitudeMSLM = 100;
        double                           mClientAltitudeGLM  = 100;

        std::mutex mStreamMapLock;
        std::unordered_map<std::string, struct AtcCallsignMeta> mHeadsetIncomingStreams;
        std::unordered_map<std::string, struct AtcCallsignMeta> mSpeakerIncomingStreams;

        std::mutex                            mRadioStateLock;
        std::atomic<bool>                     mPtt;
        bool                                  mLastFramePtt;
        std::atomic<uint32_t>                 mTxSequence;
        std::map<unsigned int, AtcRadioState> mRadioState;
        std::shared_ptr<audio::ITick>         mTick;

        bool mDefaultEnableHfSquelch = false;
        bool mDefaultBypassEffects = false;

        std::shared_ptr<AtcOutputAudioDevice> mHeadsetDevice;
        std::shared_ptr<AtcOutputAudioDevice> mSpeakerDevice;

        std::shared_ptr<OutputDeviceState> mHeadsetState;
        std::shared_ptr<OutputDeviceState> mSpeakerState;

        float mMicVolume = 1.0f;

        unsigned int mLastReceivedRadio;

        std::shared_ptr<VoiceCompressionSink>     mVoiceSink;
        std::shared_ptr<audio::SpeexPreprocessor> mVoiceFilter;

        event::EventCallbackTimer mMaintenanceTimer;
        RollingAverage<double>    mVuMeter;

        void resetRadioFx(unsigned int radio, bool except_click = false);

        void set_radio_effects(unsigned int rxIter);

        bool mix_effect(std::shared_ptr<audio::ISampleSource> effect, float gain, std::shared_ptr<OutputDeviceState> state);

        void processCompressedFrame(std::vector<unsigned char> compressedData) override;

        static void dtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen, void *user_data);
        void instDtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen);

        void maintainIncomingStreams();

      private:
        bool _process_radio(const std::map<void *, audio::SampleType[audio::frameSizeSamples]> &sampleCache, unsigned int rxIter, bool onHeadset);

        void interleave(audio::SampleType *leftChannel, audio::SampleType *rightChannel, audio::SampleType *outputBuffer, size_t numSamples);

        /** mix_buffers is a utility function that mixes two buffers of audio together. The src_dst
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
        static void mix_buffers(audio::SampleType *RESTRICT src_dst, const audio::SampleType *RESTRICT src2, float src2_gain = 1.0);
    };
}} // namespace afv_native::afv

#endif // AFV_NATIVE_RADIOSIMULATION_H
