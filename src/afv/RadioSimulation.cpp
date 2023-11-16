/* afv/RadioSimulation.cpp
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


#include <cmath>
#include <atomic>
#include <iostream>

#include "afv-native/Log.h"
#include "afv-native/afv/RadioSimulation.h"
#include "afv-native/afv/dto/voice_server/AudioTxOnTransceivers.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/audio/PinkNoiseGenerator.h"

using namespace afv_native;
using namespace afv_native::afv;

const float fxClickGain = 1.3f;
const float fxBlockToneGain = 0.25f;
const float fxBlockToneFreq = 180.0f;
const float fxAcBusGain = 0.005f;
const float fxVhfWhiteNoiseGain = 0.17f;
const float fxHfWhiteNoiseGain = 0.16f;
const double minDb = -40.0;
const double maxDb = 0.0;

CallsignMeta::CallsignMeta():
    source(),
    transceivers()
{
    source = std::make_shared<RemoteVoiceSource>();
}

OutputAudioDevice::OutputAudioDevice(std::weak_ptr<RadioSimulation> radio, bool onHeadset) :
    mRadio(radio),
    onHeadset(onHeadset)
{

}

audio::SourceStatus OutputAudioDevice::getAudioFrame(audio::SampleType *bufferOut)
{
    return mRadio.lock()->getAudioFrame(bufferOut, onHeadset);
}

OutputDeviceState::OutputDeviceState()
{
    mChannelBuffer = new audio::SampleType[audio::frameSizeSamples];
    mMixingBuffer = new audio::SampleType[audio::frameSizeSamples];
    mLeftMixingBuffer = new audio::SampleType[audio::frameSizeSamples];
    mRightMixingBuffer = new audio::SampleType[audio::frameSizeSamples];
    mFetchBuffer = new audio::SampleType[audio::frameSizeSamples];
}

OutputDeviceState::~OutputDeviceState()
{
    delete[] mFetchBuffer;
    delete[] mMixingBuffer;
    delete[] mLeftMixingBuffer;
    delete[] mRightMixingBuffer;
    delete[] mChannelBuffer;
}

RadioSimulation::RadioSimulation(
        struct event_base *evBase,
        std::shared_ptr<EffectResources> resources,
        cryptodto::UDPChannel *channel,
        unsigned int radioCount):
    IncomingAudioStreams(0),
    mEvBase(evBase),
    mResources(std::move(resources)),
    mChannel(),
    mStreamMapLock(),
    mHeadsetIncomingStreams(),
    mSpeakerIncomingStreams(),
    mRadioStateLock(),
    mPtt(false),
    mLastFramePtt(false),
    mTxRadio(0),
    mTxSequence(0),
    mRadioState(radioCount),
    mVoiceSink(std::make_shared<VoiceCompressionSink>(*this)),
    mVoiceFilter(),
    mMaintenanceTimer(mEvBase, std::bind(&RadioSimulation::maintainIncomingStreams, this)),
    mVuMeter(300 / audio::frameLengthMs) // VU is a 300ms zero to peak response...
{
    setUDPChannel(channel);
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
}

RadioSimulation::~RadioSimulation()
{

}

void RadioSimulation::putAudioFrame(const audio::SampleType *bufferIn)
{
    audio::SampleType samples[audio::frameSizeSamples];
    mVoiceFilter->transformFrame(samples, bufferIn);

    float value = 0;
    for(int i = 0; i < audio::frameSizeSamples; i++) {
        value = samples[i] * mMicVolume;
        if(value > 1.0f) {
            value = 1.0f;
        }
        if(value < -1.0f) {
            value = -1.0f;
        }
        samples[i] = value;
    }

    // do the peak/Vu calcs
    {
        auto *b = samples;
        int i = audio::frameSizeSamples - 1;
        audio::SampleType peak = fabs(*(b++));
        while (i-- > 0) {
            peak = std::max<audio::SampleType>(peak, fabs(*(b++)));
        }
        double peakDb = 20.0 * log10(peak);
        peakDb = std::max(minDb, peakDb);
        peakDb = std::min(maxDb, peakDb);
        double ratio = (peakDb - minDb) / (maxDb - minDb);
        if(ratio < 0.30)
            ratio = 0;
        if(ratio > 1.0)
            ratio = 1;
        mVuMeter.addDatum(ratio);
    }

    if (!mPtt.load() && !mLastFramePtt) {
        // Tick the sequence over when we have no Ptt as the compressed endpoint wont' get called to do that.
        std::atomic_fetch_add<uint32_t>(&mTxSequence, 1);
        return;
    }

    mVoiceSink->putAudioFrame(samples);
}

void RadioSimulation::processCompressedFrame(std::vector<unsigned char> compressedData)
{
    if (mChannel != nullptr && mChannel->isOpen()) {
        dto::AudioTxOnTransceivers audioOutDto;
        {
            std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);

            if (!mPtt.load()) {
                audioOutDto.LastPacket = true;
                mLastFramePtt = false;
            } else {
                audioOutDto.LastPacket = false;
                mLastFramePtt = true;
            }

            audioOutDto.Transceivers.emplace_back(mTxRadio);
        }
        audioOutDto.SequenceCounter = std::atomic_fetch_add<uint32_t>(&mTxSequence, 1);
        audioOutDto.Callsign = mCallsign;
        audioOutDto.Audio = std::move(compressedData);
        mChannel->sendDto(audioOutDto);
    }
}

void
RadioSimulation::mix_buffers(audio::SampleType* RESTRICT src_dst, const audio::SampleType* RESTRICT src2, float src2_gain)
{
    for (int i = 0; i < audio::frameSizeSamples; i++)
    {
        src_dst[i] += (src2_gain * src2[i]);
    }
}

bool RadioSimulation::getTxActive(unsigned int radio) {
    if (radio != mTxRadio) {
        return false;
    }
    return mPtt.load();
}

bool
RadioSimulation::getRxActive(unsigned int radio)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);

    return (mRadioState[radio].mLastRxCount > 0);
}

inline bool
freqIsHF(unsigned int freq)
{
    return freq < 30000000;
}

bool RadioSimulation::_process_radio(
    const std::map<void *, audio::SampleType[audio::frameSizeSamples]> &sampleCache,
    size_t rxIter,
    bool onHeadset)
{
    std::shared_ptr<OutputDeviceState> state = onHeadset ? mHeadsetState : mSpeakerState;

    ::memset(state->mChannelBuffer, 0, audio::frameSizeBytes);
    if (mPtt.load() && mTxRadio == rxIter) {
        // don't analyze and mix-in the radios transmitting, but suppress the
        // effects.
        resetRadioFx(rxIter);
        return true;
    }
    // now, find all streams that this applies to.
    float crackleGain = 0.0f;
    float hfGain = 0.0f;
    float vhfGain = 0.0f;
    float acBusGain = 0.0f;
    uint32_t concurrentStreams = 0;
    for (auto &srcPair: (onHeadset ? mHeadsetIncomingStreams : mSpeakerIncomingStreams)) {
        if (!srcPair.second.source || !srcPair.second.source->isActive() || (sampleCache.find(srcPair.second.source.get()) == sampleCache.end())) {
            continue;
        }
        bool mUseStream = false;
        float voiceGain = 1.0f;
        for (const afv::dto::RxTransceiver &tx: srcPair.second.transceivers) {
            if (tx.Frequency == mRadioState[rxIter].Frequency) {
                mUseStream = true;

                float crackleFactor = 0.0f;
                if (!mRadioState[rxIter].mBypassEffects) {
                    crackleFactor = static_cast<float>((exp(tx.DistanceRatio) * pow(tx.DistanceRatio, -4.0) / 350.0) - 0.00776652);
                    crackleFactor = fmax(0.0f, crackleFactor);
                    crackleFactor = fmin(0.20f, crackleFactor);

                    if (freqIsHF(tx.Frequency))
                    {
                        if (!mRadioState[rxIter].mHfSquelch)
                        {
                            hfGain = fxHfWhiteNoiseGain;
                        }
                        else
                        {
                            hfGain = 0.0f;
                        }
                        vhfGain = 0.0f;
                        acBusGain = 0.001f;
                        voiceGain = 0.20f;
                    }
                    else
                    {
                        hfGain = 0.0f;
                        vhfGain = fxVhfWhiteNoiseGain;
                        acBusGain = fxAcBusGain;
                        crackleGain += crackleFactor * 2;
                        voiceGain = 1.0 - crackleFactor * 3.7;
                    }
                }
                break; // matched once.  dont' bother anymore.
            }
        }
        if (mUseStream) {
            // then include this stream.
            try {
                mix_buffers(
                            state->mChannelBuffer,
                            sampleCache.at(srcPair.second.source.get()),
                            voiceGain * mRadioState[rxIter].Gain);
                concurrentStreams++;
            } catch (const std::out_of_range &) {
                LOG("RadioSimulation", "internal error:  Tried to mix uncached stream");
            }
        }
    }

    if (concurrentStreams > 0) {

        if (!mRadioState[rxIter].mBypassEffects) {

            // limiter effect
            for(unsigned int i = 0; i < audio::frameSizeSamples; i++)
            {
                if(state->mChannelBuffer[i] > 1.0f)
                    state->mChannelBuffer[i] = 1.0f;
                if(state->mChannelBuffer[i] < -1.0f)
                    state->mChannelBuffer[i] = -1.0f;
            }

            mRadioState[rxIter].vhfFilter.transformFrame(state->mChannelBuffer, state->mChannelBuffer);
            mRadioState[rxIter].simpleCompressorEffect.transformFrame(state->mChannelBuffer, state->mChannelBuffer);

            set_radio_effects(rxIter);
            if (!mix_effect(mRadioState[rxIter].Crackle, crackleGain * mRadioState[rxIter].Gain, state))
            {
                mRadioState[rxIter].Crackle.reset();
            }
            if (!mix_effect(mRadioState[rxIter].HfWhiteNoise, hfGain * mRadioState[rxIter].Gain, state))
            {
                mRadioState[rxIter].HfWhiteNoise.reset();
            }
            if (!mix_effect(mRadioState[rxIter].VhfWhiteNoise, vhfGain * mRadioState[rxIter].Gain, state))
            {
                mRadioState[rxIter].VhfWhiteNoise.reset();
            }
            if (!mix_effect(mRadioState[rxIter].AcBus, acBusGain * mRadioState[rxIter].Gain, state))
            {
                mRadioState[rxIter].AcBus.reset();
            }
        } // bypass effects
        if (concurrentStreams > 1) {
            if (!mRadioState[rxIter].BlockTone) {
                mRadioState[rxIter].BlockTone = std::make_shared<audio::SineToneSource>(fxBlockToneFreq);
            }
            if (!mix_effect(mRadioState[rxIter].BlockTone, fxBlockToneGain * mRadioState[rxIter].Gain, state)) {
                mRadioState[rxIter].BlockTone.reset();
            }
        } else {
            if (mRadioState[rxIter].BlockTone) {
                mRadioState[rxIter].BlockTone.reset();
            }
        }
    } else {
        resetRadioFx(rxIter, true);
        if (mRadioState[rxIter].mLastRxCount > 0) {
            mRadioState[rxIter].Click = std::make_shared<audio::RecordedSampleSource>(mResources->mClick, false);
        }
    }
    mRadioState[rxIter].mLastRxCount = concurrentStreams;

    // if we have a pending click, play it.
    if (!mix_effect(mRadioState[rxIter].Click, fxClickGain * mRadioState[rxIter].Gain, state)) {
        mRadioState[rxIter].Click.reset();
    }

    // now, finally, mix the channel buffer into the mixing buffer.
    if(mSplitChannels) {
        if(rxIter == 0) {
            mix_buffers(state->mLeftMixingBuffer, state->mChannelBuffer);
        } else if(rxIter == 1) {
            mix_buffers(state->mRightMixingBuffer, state->mChannelBuffer);
        }
    } else {
        mix_buffers(state->mMixingBuffer, state->mChannelBuffer);
    }

    return false;
}

audio::SourceStatus RadioSimulation::getAudioFrame(audio::SampleType *bufferOut, bool onHeadset)
{
    std::shared_ptr<OutputDeviceState> state = onHeadset ? mHeadsetState : mSpeakerState;

    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    std::lock_guard<std::mutex> streamGuard(mStreamMapLock);

    std::map<void *, audio::SampleType[audio::frameSizeSamples]> sampleCache;
    for (auto &src: (onHeadset ? mHeadsetIncomingStreams : mSpeakerIncomingStreams)) {
        if (src.second.source && src.second.source->isActive() && (sampleCache.find(src.second.source.get()) == sampleCache.end())) {
            const auto rv = src.second.source->getAudioFrame(sampleCache[src.second.source.get()]);
            if (rv != audio::SourceStatus::OK) {
                sampleCache.erase(src.second.source.get());
            }
        }
    }

    ::memset(state->mLeftMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);
    ::memset(state->mRightMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);
    ::memset(state->mMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);

    size_t rxIter = 0;
    for (rxIter = 0; rxIter < mRadioState.size(); rxIter++) {
        if(mRadioState[rxIter].onHeadset == onHeadset) {
            _process_radio(sampleCache, rxIter, onHeadset);
        }
    }

    if(mSplitChannels) {
        audio::SampleType interleavedSamples[audio::frameSizeSamples * 2];
        interleave(state->mLeftMixingBuffer, state->mRightMixingBuffer, interleavedSamples, audio::frameSizeSamples);
        ::memcpy(bufferOut, interleavedSamples, sizeof(audio::SampleType) * audio::frameSizeSamples * 2);
    }
    else {
        ::memcpy(bufferOut, state->mMixingBuffer, sizeof(audio::SampleType) * audio::frameSizeSamples);
    }

    return audio::SourceStatus::OK;
}

void RadioSimulation::set_radio_effects(size_t rxIter)
{
    if (!mRadioState[rxIter].VhfWhiteNoise)
    {
        mRadioState[rxIter].VhfWhiteNoise = std::make_shared<audio::RecordedSampleSource>(mResources->mVhfWhiteNoise, true);
    }
    if (!mRadioState[rxIter].HfWhiteNoise)
    {
        mRadioState[rxIter].HfWhiteNoise = std::make_shared<audio::RecordedSampleSource>(mResources->mHfWhiteNoise, true);
    }
    if (!mRadioState[rxIter].Crackle)
    {
        mRadioState[rxIter].Crackle = std::make_shared<audio::RecordedSampleSource>(mResources->mCrackle, true);
    }
    if (!mRadioState[rxIter].AcBus)
    {
        mRadioState[rxIter].AcBus = std::make_shared<audio::RecordedSampleSource>(mResources->mAcBus, true);
    }
}

bool RadioSimulation::mix_effect(std::shared_ptr<audio::ISampleSource> effect, float gain, std::shared_ptr<OutputDeviceState> state) {
    if (effect && gain > 0.0f) {
        auto rv = effect->getAudioFrame(state->mFetchBuffer);
        if (rv == audio::SourceStatus::OK) {
            RadioSimulation::mix_buffers(state->mChannelBuffer, state->mFetchBuffer, gain);
        } else {
            return false;
        }
    }
    return true;
}

void RadioSimulation::rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt)
{
    std::lock_guard<std::mutex> streamMapLock(mStreamMapLock);
    //FIXME:  Deal with the case of a single-callsign transmitting multiple different voicestreams simultaneously.
    mHeadsetIncomingStreams[pkt.Callsign].source->appendAudioDTO(pkt);
    mHeadsetIncomingStreams[pkt.Callsign].transceivers = pkt.Transceivers;

    mSpeakerIncomingStreams[pkt.Callsign].source->appendAudioDTO(pkt);
    mSpeakerIncomingStreams[pkt.Callsign].transceivers = pkt.Transceivers;
}

void RadioSimulation::setFrequency(unsigned int radio, unsigned int frequency)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (radio >= mRadioState.size()) {
        return;
    }
    if (mRadioState[radio].Frequency == frequency) {
        return;
    }
    mRadioState[radio].Frequency = frequency;
    // reset all of the effects, except the click which should be audiable due to the Squelch-gate kicking in on the new frequency
    resetRadioFx(radio, true);
    LOG("RadioSimulation", "setFrequency: %i: %i", radio, frequency);
}

void RadioSimulation::resetRadioFx(unsigned int radio, bool except_click)
{
    if (!except_click) {
        mRadioState[radio].Click.reset();
        mRadioState[radio].mLastRxCount = 0;
    }
    mRadioState[radio].BlockTone.reset();
    mRadioState[radio].Crackle.reset();
    mRadioState[radio].VhfWhiteNoise.reset();
    mRadioState[radio].HfWhiteNoise.reset();
    mRadioState[radio].AcBus.reset();
}

void RadioSimulation::setPtt(bool pressed)
{
    mPtt.store(pressed);
}

void RadioSimulation::setGain(unsigned int radio, float gain)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    mRadioState[radio].Gain = gain;
    LOG("RadioSimulation", "setGain: %i: %f", radio, gain);
}

void RadioSimulation::setTxRadio(unsigned int radio)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (radio >= mRadioState.size()) {
        return;
    }
    mTxRadio = radio;
    LOG("RadioSimulation", "setTxRadio: %i", radio);
}

void RadioSimulation::setMicrophoneVolume(float volume)
{
    mMicVolume = volume;
}

void RadioSimulation::dtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen, void *user_data)
{
    auto *thisRs = reinterpret_cast<RadioSimulation *>(user_data);
    thisRs->instDtoHandler(dtoName, bufIn, bufLen);
}

void RadioSimulation::instDtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen)
{
    if (dtoName == "AR") {
        try {
            dto::AudioRxOnTransceivers audioIn;
            auto unpacker = msgpack::unpack(reinterpret_cast<const char *>(bufIn), bufLen);
            auto msgpackObj = unpacker.get();
            msgpackObj.convert(audioIn);
            rxVoicePacket(audioIn);
        } catch (msgpack::type_error &e) {
            LOG("radiosimulation", "Error unmarshalling %s packet: %s", dtoName.c_str(), e.what());
        }
    }
}

void RadioSimulation::setUDPChannel(cryptodto::UDPChannel *newChannel)
{
    if (mChannel != nullptr) {
        mChannel->unregisterDtoHandler("AR");
    }
    mChannel = newChannel;
    if (mChannel != nullptr) {
        mChannel->registerDtoHandler(
                    "AR", [this](const unsigned char *data, size_t len) {
            try {
                dto::AudioRxOnTransceivers rxAudio;
                auto objHdl = msgpack::unpack(reinterpret_cast<const char *>(data), len);
                objHdl.get().convert(rxAudio);
                this->rxVoicePacket(rxAudio);
            } catch (const msgpack::type_error &e) {
                LOG("radiosimulation", "unable to unpack audio data received: %s", e.what());
                LOGDUMPHEX("radiosimulation", data, len);
            }
        });
    }
}

void RadioSimulation::maintainIncomingStreams()
{
    std::lock_guard<std::mutex> ml(mStreamMapLock);
    std::vector<std::string> callsignsToPurge;
    std::vector<std::string> speakerCallsignsToPurge;
    util::monotime_t now = util::monotime_get();
    for (const auto &streamPair: mHeadsetIncomingStreams) {
        auto idleTime = now - streamPair.second.source->getLastActivityTime();
        if ((now - streamPair.second.source->getLastActivityTime()) > audio::compressedSourceCacheTimeoutMs) {
            callsignsToPurge.emplace_back(streamPair.first);
        }
    }
    for (const auto &streamPair : mSpeakerIncomingStreams) {
        auto idleTime = now - streamPair.second.source->getLastActivityTime();
        if ((now - streamPair.second.source->getLastActivityTime()) > audio::compressedSourceCacheTimeoutMs) {
            speakerCallsignsToPurge.emplace_back(streamPair.first);
        }
    }
    for (const auto &callsign: callsignsToPurge) {
        mHeadsetIncomingStreams.erase(callsign);
    }
    for(const auto &callsign: speakerCallsignsToPurge) {
        mSpeakerIncomingStreams.erase(callsign);
    }
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
}

void RadioSimulation::setCallsign(const std::string &newCallsign)
{
    mCallsign = newCallsign;
}

void RadioSimulation::reset()
{
    {
        std::lock_guard<std::mutex> ml(mStreamMapLock);
        mHeadsetIncomingStreams.clear();
        mSpeakerIncomingStreams.clear();
    }
    mTxSequence.store(0);
    mPtt.store(false);
    mLastFramePtt = false;
    // reset the voice compression codec state.
    mVoiceSink->reset();
}

double RadioSimulation::getVu() const
{
    return std::max(-40.0, mVuMeter.getAverage());
}

double RadioSimulation::getPeak() const
{
    return std::max(-40.0, mVuMeter.getMax());
}

bool RadioSimulation::getEnableInputFilters() const
{
    return static_cast<bool>(mVoiceFilter);
}

void RadioSimulation::setEnableInputFilters(bool enableInputFilters)
{
    if (enableInputFilters) {
        if (!mVoiceFilter) {
            mVoiceFilter = std::make_shared<audio::SpeexPreprocessor>(mVoiceSink);
        }
    } else {
        mVoiceFilter.reset();
    }
}

void RadioSimulation::setEnableOutputEffects(bool enableEffects)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    for (auto &thisRadio: mRadioState) {
        thisRadio.mBypassEffects = !enableEffects;
    }
}

void RadioSimulation::setEnableHfSquelch(bool enableSquelch)
{
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    for (auto& thisRadio : mRadioState) {
        thisRadio.mHfSquelch = enableSquelch;
    }
}

void RadioSimulation::setupDevices(util::ChainedCallback<void (ClientEventType, void *, void *)> *eventCallback)
{
    mHeadsetDevice = std::make_shared<OutputAudioDevice>(shared_from_this(), true);
    mSpeakerDevice = std::make_shared<OutputAudioDevice>(shared_from_this(), false);

    mHeadsetState = std::make_shared<OutputDeviceState>();
    mSpeakerState = std::make_shared<OutputDeviceState>();

    ClientEventCallback = eventCallback;
}

void RadioSimulation::setOnHeadset(unsigned int radio, bool onHeadset)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    mRadioState[radio].onHeadset = onHeadset;
}

void RadioSimulation::setSplitAudioChannels(bool splitChannels)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    mSplitChannels = splitChannels;
}
