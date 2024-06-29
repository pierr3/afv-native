/* afv/ATCRadioSimulation.cpp
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

 * Also derived from: work by Mike Evans starting 10/21/20.
 */

#include "afv-native/afv/ATCRadioSimulation.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/event.h"
#include "afv-native/util/other.h"
#include <cstddef>
#include <memory>

using namespace afv_native;
using namespace afv_native::afv;

const float  fxClickGain         = 1.3f;
const float  fxBlockToneGain     = 0.25f;
const float  fxBlockToneFreq     = 180.0f;
const float  fxAcBusGain         = 0.005f;
const float  fxVhfWhiteNoiseGain = 0.17f;
const float  fxHfWhiteNoiseGain  = 0.16f;
const double minDb               = -40.0;
const double maxDb               = 0.0;

AtcCallsignMeta::AtcCallsignMeta(): source(), transceivers() {
    source = std::make_shared<RemoteVoiceSource>();
}

AtcOutputAudioDevice::AtcOutputAudioDevice(std::weak_ptr<ATCRadioSimulation> radio, bool onHeadset):
    mRadio(radio), onHeadset(onHeadset) {
}

audio::SourceStatus AtcOutputAudioDevice::getAudioFrame(audio::SampleType *bufferOut) {
    return mRadio.lock()->getAudioFrame(bufferOut, onHeadset);
}

ATCRadioSimulation::ATCRadioSimulation(struct event_base *evBase, std::shared_ptr<EffectResources> resources, cryptodto::UDPChannel *channel):
    IncomingAudioStreams(0), mEvBase(evBase), mResources(std::move(resources)), mChannel(), mStreamMapLock(), mHeadsetIncomingStreams(), mSpeakerIncomingStreams(), mRadioStateLock(), mPtt(false), mLastFramePtt(false), mTxSequence(0), mVoiceSink(std::make_shared<VoiceCompressionSink>(*this)), mVoiceFilter(std::make_shared<audio::SpeexPreprocessor>(mVoiceSink)), mMaintenanceTimer(mEvBase, std::bind(&ATCRadioSimulation::maintainIncomingStreams, this)), mVoiceTimeoutTimer(mEvBase, std::bind(&ATCRadioSimulation::maintainVoiceTimeout, this)), mVuMeter(300 / audio::frameLengthMs) // VU is a 300ms zero to peak response...
{
    setUDPChannel(channel);
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
    mVoiceTimeoutTimer.enable(voiceTimeoutIntervalMs);
}

ATCRadioSimulation::~ATCRadioSimulation() {
}

void ATCRadioSimulation::putAudioFrame(const audio::SampleType *bufferIn) {
    if (mTick) {
        mTick->tick();
    }

    audio::SampleType samples[audio::frameSizeSamples];
    if (static_cast<bool>(mVoiceFilter)) {
        mVoiceFilter->transformFrame(samples, bufferIn);
    } else {
        ::memcpy(samples, bufferIn, sizeof(samples));
    }

    float value = 0;
    for (int i = 0; i < audio::frameSizeSamples; i++) {
        value = samples[i] * mMicVolume;
        if (value > 1.0f) {
            value = 1.0f;
        }
        if (value < -1.0f) {
            value = -1.0f;
        }
        samples[i] = value;
    }

    // do the peak/Vu calcs
    {
        auto             *b    = samples;
        int               i    = audio::frameSizeSamples - 1;
        audio::SampleType peak = fabs(*(b++));
        while (i-- > 0) {
            peak = std::max<audio::SampleType>(peak, fabs(*(b++)));
        }
        double peakDb = 20.0 * log10(peak);
        peakDb        = std::max(minDb, peakDb);
        peakDb        = std::min(maxDb, peakDb);
        double ratio  = (peakDb - minDb) / (maxDb - minDb);
        if (ratio < 0.30) {
            ratio = 0;
        }
        if (ratio > 1.0) {
            ratio = 1;
        }
        mVuMeter.addDatum(ratio);
    }

    if (!mPtt.load() && !mLastFramePtt) {
        // Tick the sequence over when we have no Ptt as the compressed endpoint wont' get called to do that.
        std::atomic_fetch_add<uint32_t>(&mTxSequence, 1);
        return;
    }

    mVoiceSink->putAudioFrame(samples);
}

void ATCRadioSimulation::processCompressedFrame(std::vector<unsigned char> compressedData) {
    if (mChannel != nullptr && mChannel->isOpen()) {
        dto::AudioTxOnTransceivers audioOutDto;
        {
            std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);

            if (!mPtt.load()) {
                audioOutDto.LastPacket = true;
                mLastFramePtt          = false;
            } else {
                audioOutDto.LastPacket = false;
                mLastFramePtt          = true;
            }

            for (auto &[_, radio]: mRadioState) {
                if (!radio.tx) {
                    continue;
                }

                for (const auto trans: radio.transceivers) {
                    audioOutDto.Transceivers.emplace_back(trans.ID);
                }
            }
        }
        audioOutDto.SequenceCounter = std::atomic_fetch_add<uint32_t>(&mTxSequence, 1);
        audioOutDto.Callsign = mCallsign;
        audioOutDto.Audio    = std::move(compressedData);
        mChannel->sendDto(audioOutDto);
    }
}

void ATCRadioSimulation::mix_buffers(audio::SampleType *RESTRICT src_dst, const audio::SampleType *RESTRICT src2, float src2_gain) {
    for (int i = 0; i < audio::frameSizeSamples; i++) {
        src_dst[i] += (src2_gain * src2[i]);
    }
}

bool ATCRadioSimulation::getTxActive(unsigned int radio) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(radio)) {
        return false;
    }

    if (!mRadioState[radio].tx) {
        return false;
    }
    return mPtt.load();
}

bool ATCRadioSimulation::getRxActive(unsigned int radio) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(radio)) {
        return false;
    }

    if (!mRadioState[radio].rx) {
        return false;
    }

    return (mRadioState[radio].mLastRxCount > 0);
}

inline bool freqIsHF(unsigned int freq) {
    return freq < 30000000;
}

bool ATCRadioSimulation::_process_radio(const std::map<void *, audio::SampleType[audio::frameSizeSamples]> &sampleCache, unsigned int rxIter, bool onHeadset) {
    if (!isFrequencyActive(rxIter)) {
        resetRadioFx(rxIter);
        return false;
    }

    std::shared_ptr<OutputDeviceState> state = onHeadset ? mHeadsetState : mSpeakerState;

    ::memset(state->mChannelBuffer, 0, audio::frameSizeBytes);
    if (mPtt.load() && mRadioState[rxIter].tx) {
        // don't analyze and mix-in the radios transmitting, but suppress the
        // effects.
        resetRadioFx(rxIter);
        return true;
    }
    // now, find all streams that this applies to.
    float    crackleGain       = 0.0f;
    float    hfGain            = 0.0f;
    float    vhfGain           = 0.0f;
    float    acBusGain         = 0.0f;
    uint32_t concurrentStreams = 0;
    for (auto &srcPair: (onHeadset ? mHeadsetIncomingStreams : mSpeakerIncomingStreams)) {
        if (!srcPair.second.source || !srcPair.second.source->isActive() ||
            (sampleCache.find(srcPair.second.source.get()) == sampleCache.end())) {
            continue;
        }
        bool  mUseStream = false;
        float voiceGain  = 1.0f;

        std::vector<afv::dto::RxTransceiver> matchingTransceivers;
        std::copy_if(srcPair.second.transceivers.begin(),
                     srcPair.second.transceivers.end(), std::back_inserter(matchingTransceivers), [&](afv::dto::RxTransceiver k) {
                         return k.Frequency == mRadioState[rxIter].Frequency;
                     });

        if (matchingTransceivers.size() > 0) {
            mUseStream = true;

            auto closestTransceiver = *std::max_element(matchingTransceivers.begin(),
                                                        matchingTransceivers.end(), [](afv::dto::RxTransceiver a, afv::dto::RxTransceiver b) {
                                                            return (a.DistanceRatio < b.DistanceRatio);
                                                        });

            float crackleFactor = 0.0f;
            if (!mRadioState[rxIter].mBypassEffects) {
                crackleFactor = static_cast<float>((exp(closestTransceiver.DistanceRatio) *
                                                    pow(closestTransceiver.DistanceRatio, -4.0) / 350.0) -
                                                   0.00776652);
                crackleFactor = fmax(0.0f, crackleFactor);
                crackleFactor = fmin(0.20f, crackleFactor);

                if (freqIsHF(rxIter)) {
                    if (!mRadioState[rxIter].mHfSquelch) {
                        hfGain = fxHfWhiteNoiseGain;
                    } else {
                        hfGain = 0.0f;
                    }
                    vhfGain   = 0.0f;
                    acBusGain = 0.001f;
                    voiceGain = 0.20f;
                } else {
                    hfGain    = 0.0f;
                    vhfGain   = fxVhfWhiteNoiseGain;
                    acBusGain = fxAcBusGain;
                    crackleGain += crackleFactor * 2;
                    voiceGain = 1.0 - crackleFactor * 3.7;
                }
            }
        }

        if (mUseStream) {
            // then include this stream.
            try {
                mix_buffers(state->mChannelBuffer,
                            sampleCache.at(srcPair.second.source.get()),
                            voiceGain * mRadioState[rxIter].Gain);
                concurrentStreams++;
            } catch (const std::out_of_range &) {
                LOG("ATCRadioSimulation", "internal error:  Tried to mix uncached stream");
            }
        }
    }

    if (concurrentStreams > 0) {
        if (mRadioState[rxIter].mLastRxCount == 0) {
            // Post Begin Voice Receiving Notfication
            unsigned int freq = rxIter;
            mRadioState[rxIter].liveTransmittingCallsigns = {}; // We know for sure nobody is transmitting yet
            ClientEventCallback->invokeAll(ClientEventType::FrequencyRxBegin, &freq, nullptr);
            LOG("ATCRadioSimulation", "FrequencyRxBegin event: %i", freq);
        }
        if (!mRadioState[rxIter].mBypassEffects) {
            // limiter effect
            for (unsigned int i = 0; i < audio::frameSizeSamples; i++) {
                if (state->mChannelBuffer[i] > 1.0f) {
                    state->mChannelBuffer[i] = 1.0f;
                }
                if (state->mChannelBuffer[i] < -1.0f) {
                    state->mChannelBuffer[i] = -1.0f;
                }
            }

            set_radio_effects(rxIter);
            mRadioState[rxIter].vhfFilter->transformFrame(state->mChannelBuffer,
                                                          state->mChannelBuffer);
            mRadioState[rxIter].simpleCompressorEffect.transformFrame(state->mChannelBuffer,
                                                                      state->mChannelBuffer);
            if (!mix_effect(mRadioState[rxIter].Crackle,
                            crackleGain * mRadioState[rxIter].Gain, state)) {
                mRadioState[rxIter].Crackle.reset();
            }
            if (!mix_effect(mRadioState[rxIter].HfWhiteNoise,
                            hfGain * mRadioState[rxIter].Gain, state)) {
                mRadioState[rxIter].HfWhiteNoise.reset();
            }
            if (!mix_effect(mRadioState[rxIter].VhfWhiteNoise,
                            vhfGain * mRadioState[rxIter].Gain, state)) {
                mRadioState[rxIter].VhfWhiteNoise.reset();
            }
            if (!mix_effect(mRadioState[rxIter].AcBus,
                            acBusGain * mRadioState[rxIter].Gain, state)) {
                mRadioState[rxIter].AcBus.reset();
            }
        } // bypass effects
        if (concurrentStreams > 1) {
            if (!mRadioState[rxIter].BlockTone) {
                mRadioState[rxIter].BlockTone = std::make_shared<audio::SineToneSource>(fxBlockToneFreq);
            }
            if (!mix_effect(mRadioState[rxIter].BlockTone,
                            fxBlockToneGain * mRadioState[rxIter].Gain, state)) {
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
            mRadioState[rxIter].Click =
                std::make_shared<audio::RecordedSampleSource>(mResources->mClick, false);

            mRadioState[rxIter].liveTransmittingCallsigns = {}; // We know for sure nobody is transmitting anymore
            ClientEventCallback->invokeAll(ClientEventType::FrequencyRxEnd, &rxIter, nullptr);
            mRadioState[rxIter].lastVoiceTime = 0;
            LOG("ATCRadioSimulation", "FrequencyRxEnd event: %i", rxIter);
        }
    }
    mRadioState[rxIter].mLastRxCount = concurrentStreams;

    // if we have a pending click, play it.
    if (!mix_effect(mRadioState[rxIter].Click, fxClickGain * mRadioState[rxIter].Gain, state)) {
        mRadioState[rxIter].Click.reset();
    }

    // now, finally, mix the channel buffer into the mixing buffer.
    if (onHeadset) {
        if (mRadioState[rxIter].playbackChannel == PlaybackChannel::Left ||
            mRadioState[rxIter].playbackChannel == PlaybackChannel::Both) {
            mix_buffers(state->mLeftMixingBuffer, state->mChannelBuffer);
        }

        if (mRadioState[rxIter].playbackChannel == PlaybackChannel::Right ||
            mRadioState[rxIter].playbackChannel == PlaybackChannel::Both) {
            mix_buffers(state->mRightMixingBuffer, state->mChannelBuffer);
        }
    } else {
        mix_buffers(state->mMixingBuffer, state->mChannelBuffer);
    }

    return false;
}

audio::SourceStatus ATCRadioSimulation::getAudioFrame(audio::SampleType *bufferOut, bool onHeadset) {
    std::shared_ptr<OutputDeviceState> state = onHeadset ? mHeadsetState : mSpeakerState;

    std::lock_guard<std::mutex> streamGuard(mStreamMapLock);

    std::map<void *, audio::SampleType[audio::frameSizeSamples]> sampleCache;
    for (auto &src: (onHeadset ? mHeadsetIncomingStreams : mSpeakerIncomingStreams)) {
        if (src.second.source && src.second.source->isActive() &&
            (sampleCache.find(src.second.source.get()) == sampleCache.end())) {
            const auto rv =
                src.second.source->getAudioFrame(sampleCache[src.second.source.get()]);
            if (rv != audio::SourceStatus::OK) {
                sampleCache.erase(src.second.source.get());
            }
        }
    }

    ::memset(state->mLeftMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);
    ::memset(state->mRightMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);
    ::memset(state->mMixingBuffer, 0, sizeof(audio::SampleType) * audio::frameSizeSamples);

    {
        std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
        for (auto &[freq, radio]: mRadioState) {
            if (radio.onHeadset == onHeadset) {
                _process_radio(sampleCache, freq, onHeadset);
            }
        }
    }

    if (onHeadset) {
        audio::SampleType interleavedSamples[audio::frameSizeSamples * 2];
        interleave(state->mLeftMixingBuffer, state->mRightMixingBuffer, interleavedSamples, audio::frameSizeSamples);
        ::memcpy(bufferOut, interleavedSamples, sizeof(audio::SampleType) * audio::frameSizeSamples * 2);
    } else {
        ::memcpy(bufferOut, state->mMixingBuffer, sizeof(audio::SampleType) * audio::frameSizeSamples);
    }

    return audio::SourceStatus::OK;
}

void ATCRadioSimulation::set_radio_effects(unsigned int rxIter) {
    if (!mRadioState[rxIter].VhfWhiteNoise) {
        mRadioState[rxIter].VhfWhiteNoise =
            std::make_shared<audio::RecordedSampleSource>(mResources->mVhfWhiteNoise, true);
    }
    if (!mRadioState[rxIter].HfWhiteNoise) {
        mRadioState[rxIter].HfWhiteNoise =
            std::make_shared<audio::RecordedSampleSource>(mResources->mHfWhiteNoise, true);
    }
    if (!mRadioState[rxIter].Crackle) {
        mRadioState[rxIter].Crackle =
            std::make_shared<audio::RecordedSampleSource>(mResources->mCrackle, true);
    }
    if (!mRadioState[rxIter].AcBus) {
        mRadioState[rxIter].AcBus =
            std::make_shared<audio::RecordedSampleSource>(mResources->mAcBus, true);
    }
    if (!mRadioState[rxIter].vhfFilter) {
        mRadioState[rxIter].vhfFilter =
            std::make_shared<audio::VHFFilterSource>(mRadioState[rxIter].simulatedHardware);
    }
}

bool ATCRadioSimulation::mix_effect(std::shared_ptr<audio::ISampleSource> effect, float gain, std::shared_ptr<OutputDeviceState> state) {
    if (effect && gain > 0.0f) {
        auto rv = effect->getAudioFrame(state->mFetchBuffer);
        if (rv == audio::SourceStatus::OK) {
            ATCRadioSimulation::mix_buffers(state->mChannelBuffer, state->mFetchBuffer, gain);
        } else {
            return false;
        }
    }
    return true;
}

bool ATCRadioSimulation::_packetListening(const afv::dto::AudioRxOnTransceivers &pkt) {
    std::lock_guard<std::mutex> radioStateLock(mRadioStateLock);
    for (auto trans: pkt.Transceivers) {
        if (!isFrequencyActive(trans.Frequency)) {
            continue;
        }

        if (!mRadioState[trans.Frequency].rx) {
            continue;
        }

        mRadioState[trans.Frequency].lastTransmitCallsign = pkt.Callsign;
        mRadioState[trans.Frequency].lastVoiceTime        = time(0);

        if (pkt.LastPacket) {
            bool hasBeenDeleted = afv_native::util::removeIfExists(
                pkt.Callsign, mRadioState[trans.Frequency].liveTransmittingCallsigns);
            if (hasBeenDeleted) {
                // There is a risk that the pointer to lastTransmitCallsign might already change
                // or be invalid by the event handler receives it, needs to be checked in real conditions
                ClientEventCallback->invokeAll(
                    ClientEventType::StationRxEnd, &trans.Frequency,
                    (void *) mRadioState[trans.Frequency].lastTransmitCallsign.c_str());
                LOG("ATCRadioSimulation", "StationRxEnd event: %i: %s", trans.Frequency,
                    mRadioState[trans.Frequency].lastTransmitCallsign.c_str());
            }
        } else {
            if (!afv_native::util::vectorContains(pkt.Callsign,
                                                  mRadioState[trans.Frequency].liveTransmittingCallsigns)) {
                LOG("ATCRadioSimulation", "StationRxBegin event: %i: %s", trans.Frequency,
                    mRadioState[trans.Frequency].lastTransmitCallsign.c_str());

                // Need to emit that we have a new pilot that started transmitting
                ClientEventCallback->invokeAll(
                    ClientEventType::StationRxBegin, &trans.Frequency,
                    (void *) mRadioState[trans.Frequency].lastTransmitCallsign.c_str());

                mRadioState[trans.Frequency].liveTransmittingCallsigns.emplace_back(pkt.Callsign);
            }
        }

        return true;
    }

    return false;
}

void ATCRadioSimulation::rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt) {
    // FIXME:  Deal with the case of a single-callsign transmitting multiple different voicestreams simultaneously.
    if (_packetListening(pkt)) {
        std::lock_guard<std::mutex> streamMapLock(mStreamMapLock);
        mHeadsetIncomingStreams[pkt.Callsign].source->appendAudioDTO(pkt);
        mHeadsetIncomingStreams[pkt.Callsign].transceivers = pkt.Transceivers;

        mSpeakerIncomingStreams[pkt.Callsign].source->appendAudioDTO(pkt);
        mSpeakerIncomingStreams[pkt.Callsign].transceivers = pkt.Transceivers;
    }
}

bool ATCRadioSimulation::addFrequency(unsigned int radio, bool onHeadset, std::string stationName, HardwareType hardware, PlaybackChannel channel) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    bool                        isUnused = isFrequencyActiveButUnused(radio);
    if (isFrequencyActive(radio) && !isUnused) {
        LOG("ATCRadioSimulation", "addFrequency cancelled: %i is already active", radio);
        return false;
    }

    if (isUnused) {
        LOG("ATCRadioSimulation", "addFrequency overriding unused: %i", radio);
    }

    mRadioState[radio] = AtcRadioState();

    mRadioState[radio].Frequency         = radio;
    mRadioState[radio].onHeadset         = onHeadset;
    mRadioState[radio].playbackChannel   = channel;
    mRadioState[radio].stationName       = stationName;
    mRadioState[radio].simulatedHardware = hardware;
    mRadioState[radio].mBypassEffects    = mDefaultBypassEffects;
    mRadioState[radio].mHfSquelch        = mDefaultEnableHfSquelch;

    if (stationName.find("_ATIS") != std::string::npos) {
        mRadioState[radio].isATIS = true;
    }
    // reset all of the effects, except the click which should be audiable due to the Squelch-gate kicking in on the new frequency
    resetRadioFx(radio, true);
    LOG("ATCRadioSimulation", "addFrequency: %s: %i", stationName.c_str(), radio);

    return true;
}

void ATCRadioSimulation::resetRadioFx(unsigned int radio, bool except_click) {
    if (!except_click) {
        mRadioState[radio].Click.reset();
        mRadioState[radio].mLastRxCount = 0;
    }
    mRadioState[radio].BlockTone.reset();
    mRadioState[radio].Crackle.reset();
    mRadioState[radio].VhfWhiteNoise.reset();
    mRadioState[radio].HfWhiteNoise.reset();
    mRadioState[radio].AcBus.reset();
    mRadioState[radio].vhfFilter.reset();
}

void ATCRadioSimulation::setPtt(bool pressed) {
    mPtt.store(pressed);
}

void ATCRadioSimulation::setGain(unsigned int radio, float gain) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    mRadioState[radio].Gain = gain;
    LOG("ATCRadioSimulation", "setGain: %i: %f", radio, gain);
}

void ATCRadioSimulation::setMicrophoneVolume(float volume) {
    mMicVolume = volume;
}

void ATCRadioSimulation::dtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen, void *user_data) {
    auto *thisRs = reinterpret_cast<ATCRadioSimulation *>(user_data);
    thisRs->instDtoHandler(dtoName, bufIn, bufLen);
}

void ATCRadioSimulation::instDtoHandler(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen) {
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

void ATCRadioSimulation::setUDPChannel(cryptodto::UDPChannel *newChannel) {
    if (mChannel != nullptr) {
        mChannel->unregisterDtoHandler("AR");
    }
    mChannel = newChannel;
    if (mChannel != nullptr) {
        mChannel->registerDtoHandler("AR", [this](const unsigned char *data, size_t len) {
            try {
                dto::AudioRxOnTransceivers rxAudio;
                auto objHdl = msgpack::unpack(reinterpret_cast<const char *>(data), len);
                objHdl.get().convert(rxAudio);
                this->rxVoicePacket(rxAudio);
            } catch (const msgpack::type_error &e) {
                LOG("ATCRadioSimulation", "unable to unpack audio data received: %s", e.what());
                LOGDUMPHEX("ATCRadioSimulation", data, len);
            }
        });
    }
}

void ATCRadioSimulation::maintainVoiceTimeout() {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    std::map<unsigned int, AtcRadioState>::iterator it;

    for (it = mRadioState.begin(); it != mRadioState.end(); it++) {
        if (it->second.lastVoiceTime == 0) {
            continue;
        }
        LOG("ATCRadioSimulation", "Potential VoiceTimeout.. %i %i", it->second.lastVoiceTime,
            time(0) - it->second.lastVoiceTime);

        if (time(0) - it->second.lastVoiceTime >= voiceTimeoutIntervalS) {
            LOG("ATCRadioSimulation", "Found VoiceTimeout.. %i", it->second.Frequency);
            // Voice channel rx has timed out.. update things.
            it->second.lastVoiceTime = 0;
            for (const auto &c: it->second.liveTransmittingCallsigns) {
                ClientEventCallback->invokeAll(ClientEventType::StationRxEnd,
                                               &it->second.Frequency, (void *) c.c_str());
                LOG("ATCRadioSimulation", "StationRxEnd TIMEOUT event: %i: %s",
                    it->second.Frequency, c.c_str());
            }

            ClientEventCallback->invokeAll(ClientEventType::FrequencyRxEnd,
                                           &it->second.Frequency, nullptr);
            LOG("ATCRadioSimulation", "FrequencyRxEnd TIMEOUT event: %i",
                it->second.Frequency);
        }
    }

    mVoiceTimeoutTimer.enable(voiceTimeoutIntervalMs);
}

void ATCRadioSimulation::maintainIncomingStreams() {
    std::lock_guard<std::mutex> ml(mStreamMapLock);
    std::vector<std::string>    callsignsToPurge;
    std::vector<std::string>    speakerCallsignsToPurge;
    util::monotime_t            now = util::monotime_get();
    for (const auto &streamPair: mHeadsetIncomingStreams) {
        if ((now - streamPair.second.source->getLastActivityTime()) > audio::compressedSourceCacheTimeoutMs) {
            callsignsToPurge.emplace_back(streamPair.first);
        }
    }
    for (const auto &streamPair: mSpeakerIncomingStreams) {
        if ((now - streamPair.second.source->getLastActivityTime()) > audio::compressedSourceCacheTimeoutMs) {
            speakerCallsignsToPurge.emplace_back(streamPair.first);
        }
    }
    for (const auto &callsign: callsignsToPurge) {
        mHeadsetIncomingStreams.erase(callsign);
    }
    for (const auto &callsign: speakerCallsignsToPurge) {
        mSpeakerIncomingStreams.erase(callsign);
    }
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
}

void ATCRadioSimulation::setCallsign(const std::string &newCallsign) {
    mCallsign = newCallsign;
    LOG("ATCRadioSimulation", "setCallsign: %s", newCallsign.c_str());
}

void ATCRadioSimulation::setClientPosition(double lat, double lon, double amslm, double aglm) {
    mClientLatitude     = lat;
    mClientLongitude    = lon;
    mClientAltitudeMSLM = amslm;
    mClientAltitudeGLM  = aglm;
}

void ATCRadioSimulation::reset() {
    {
        std::lock_guard<std::mutex> ml(mStreamMapLock);
        mHeadsetIncomingStreams.clear();
        mSpeakerIncomingStreams.clear();
    }
    {
        std::lock_guard<std::mutex> ml(mRadioStateLock);
        mRadioState.clear();
    }
    mTxSequence.store(0);
    mPtt.store(false);
    mLastFramePtt = false;
    // reset the voice compression codec state.
    mVoiceSink->reset();
}

double ATCRadioSimulation::getVu() const {
    return std::max(-40.0, mVuMeter.getAverage());
}

double ATCRadioSimulation::getPeak() const {
    return std::max(-40.0, mVuMeter.getMax());
}

bool ATCRadioSimulation::getEnableInputFilters() const {
    return static_cast<bool>(mVoiceFilter);
}

void ATCRadioSimulation::setEnableInputFilters(bool enableInputFilters) {
    if (enableInputFilters) {
        if (!mVoiceFilter) {
            mVoiceFilter = std::make_shared<audio::SpeexPreprocessor>(mVoiceSink);
        }
    } else {
        mVoiceFilter.reset();
    }
    LOG("ATCRadioSimulation", "setEnableInputFilters: %i", enableInputFilters);
}

void ATCRadioSimulation::setEnableOutputEffects(bool enableEffects) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    for (auto &[_, thisRadio]: mRadioState) {
        thisRadio.mBypassEffects = !enableEffects;
    }
    mDefaultBypassEffects = !enableEffects;
    LOG("ATCRadioSimulation", "setEnableOutputEffects: %i", enableEffects);
}

void ATCRadioSimulation::setEnableHfSquelch(bool enableSquelch) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    for (auto &[_, thisRadio]: mRadioState) {
        thisRadio.mHfSquelch = enableSquelch;
    }
    mDefaultEnableHfSquelch = enableSquelch;
    LOG("ATCRadioSimulation", "setEnableHfSquelch: %i", enableSquelch);
}

void ATCRadioSimulation::setupDevices(util::ChainedCallback<void(ClientEventType, void *, void *)> *eventCallback) {
    mHeadsetDevice = std::make_shared<AtcOutputAudioDevice>(shared_from_this(), true);
    mSpeakerDevice = std::make_shared<AtcOutputAudioDevice>(shared_from_this(), false);

    mHeadsetState = std::make_shared<OutputDeviceState>();
    mSpeakerState = std::make_shared<OutputDeviceState>();

    ClientEventCallback = eventCallback;
}

void ATCRadioSimulation::setOnHeadset(unsigned int radio, bool onHeadset) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(radio)) {
        LOG("ATCRadioSimulation", "setOnHeadset failed, frequency inactive: %i", radio);
        return;
    }
    mRadioState[radio].onHeadset = onHeadset;
}

void afv_native::afv::ATCRadioSimulation::setRx(unsigned int freq, bool rx) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setRxRadio failed, frequency inactive: %i", freq);
        return;
    }
    if (!rx) {
        // Emit client callback for the end of station transmission
        ClientEventCallback->invokeAll(ClientEventType::FrequencyRxEnd, &freq, nullptr);
        LOG("ATCRadioSimulation", "FrequencyRxEnd event: %i", freq);
        for (auto callsign: mRadioState[freq].liveTransmittingCallsigns) {
            ClientEventCallback->invokeAll(ClientEventType::StationRxEnd, &freq,
                                           (void *) callsign.c_str());
            LOG("ATCRadioSimulation", "SetRx false StationRxEnd event: %i: %s", freq,
                callsign.c_str());
        }
        mRadioState[freq].liveTransmittingCallsigns = {};
        mRadioState[freq].lastVoiceTime             = 0;
    }
    mRadioState[freq].rx = rx;
    LOG("ATCRadioSimulation", "setRxRadio: %i", freq);
}

void afv_native::afv::ATCRadioSimulation::setTx(unsigned int freq, bool tx) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setTxRadio failed, frequency inactive: %i", freq);
        return;
    }
    mRadioState[freq].tx = tx;
    LOG("ATCRadioSimulation", "setTxRadio: %i", freq);
};

void afv_native::afv::ATCRadioSimulation::setXc(unsigned int freq, bool xc) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setXcRadio failed, frequency inactive: %i", freq);
        return;
    }
    mRadioState[freq].xc = xc;
    LOG("ATCRadioSimulation", "setXcRadio: %i", freq);
};

void afv_native::afv::ATCRadioSimulation::setCrossCoupleAcross(unsigned int freq, bool crossCoupleAcross) {
    std::lock_guard<std::mutex> lock(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setXcRadio failed, frequency inactive: %i", freq);
        return;
    }
    mRadioState[freq].xc                = crossCoupleAcross;
    mRadioState[freq].crossCoupleAcross = crossCoupleAcross;
    if (crossCoupleAcross) {
        mRadioState[freq].xc = false;
    }
    LOG("ATCRadioSimulation", "setXcRadio: %i", freq);
}

bool afv_native::afv::ATCRadioSimulation::getCrossCoupleAcrossState(unsigned int freq) {
    std::lock_guard<std::mutex> lock(mRadioStateLock);
    return mRadioState.count(freq) != 0 ? mRadioState[freq].crossCoupleAcross : false;
}

void afv_native::afv::ATCRadioSimulation::setGainAll(float gain) {
    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    for (auto &[_, radio]: mRadioState) {
        radio.Gain = gain;
    }
    LOG("ATCRadioSimulation", "setGainAll: %f", gain);
}

bool afv_native::afv::ATCRadioSimulation::isFrequencyActive(unsigned int freq) {
    return mRadioState.count(freq) != 0;
};

bool afv_native::afv::ATCRadioSimulation::isFrequencyActiveButUnused(unsigned int freq) {
    if (!isFrequencyActive(freq)) {
        return false;
    }

    if (mRadioState[freq].tx == false && mRadioState[freq].rx == false &&
        mRadioState[freq].xc == false) {
        return true;
    }

    return false;
}

void ATCRadioSimulation::setTransceivers(unsigned int freq, std::vector<afv::dto::StationTransceiver> transceivers) {
    // What we received is an array of StationTransceivers we got from the API..
    // we need to convert these to regular Transceivers before we put them into
    // the Radio State Object

    std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setTransceivers failed, frequency inactive: %i", freq);
        return;
    }
    mRadioState[freq].transceivers.clear();

    for (auto inTrans: transceivers) {
        // Transceiver IDs all set to 0 here, they will be updated when
        // coalesced into the global transceiver package
        dto::Transceiver out(0, freq, inTrans.LatDeg, inTrans.LonDeg, inTrans.HeightMslM,
                             inTrans.HeightAglM);
        mRadioState[freq].transceivers.emplace_back(out);
    }
}

std::vector<afv::dto::Transceiver> ATCRadioSimulation::makeTransceiverDto() {
    std::lock_guard<std::mutex>        radioStateGuard(mRadioStateLock);
    std::vector<afv::dto::Transceiver> retSet;
    unsigned int                       i = 0;
    for (auto &state: mRadioState) {
        if (!state.second.rx) {
            continue;
        }

        if (state.second.transceivers.empty()) {
            // If there are no transceivers received from the network, we're
            // using the client position
            retSet.emplace_back(i, state.first, mClientLatitude, mClientLongitude, mClientAltitudeMSLM, mClientAltitudeGLM);
            // Update the radioStack with the added transponder
            state.second.transceivers = {retSet.back()};
            i++;
        } else {
            for (auto &trans: state.second.transceivers) {
                retSet.emplace_back(i, trans.Frequency, trans.LatDeg, trans.LonDeg,
                                    trans.HeightMslM, trans.HeightAglM);
                trans.ID = i;
                i++;
            }
        }
    }
    return std::move(retSet);
}

std::vector<afv::dto::CrossCoupleGroup> ATCRadioSimulation::makeCrossCoupleGroupDto() {
    // Make one cross couple group per frequency
    std::vector<afv::dto::CrossCoupleGroup> out   = {{0, {}}};
    unsigned int                            index = 1;

    for (auto &[frequency, radio]: mRadioState) {
        if (!radio.xc && !radio.crossCoupleAcross) {
            continue;
        }
        // There are transceivers and they need to be coupled

        if (!radio.tx || !radio.rx) {
            // If the radio is not transmitting or receiving, we don't need to couple it
            continue;
        }

        if (radio.transceivers.empty()) {
            // If there are no transceivers, we don't need to couple it
            continue;
        }

        if (radio.crossCoupleAcross) {
            // Cross couple accross allows for cross coupling multiple frequencies together
            for (const auto trans: radio.transceivers) {
                out[0].TransceiverIDs.push_back(trans.ID);
            }
        } else if (radio.xc) {
            // Standard cross couples just couples all the transceivers together
            afv::dto::CrossCoupleGroup group(index, {});

            for (const auto trans: radio.transceivers) {
                group.TransceiverIDs.push_back(trans.ID);
            }

            out.push_back(group);
            index++;
        }
    }

    return std::move(out);
}

void afv_native::afv::ATCRadioSimulation::setTick(std::shared_ptr<audio::ITick> tick) {
    mTick = tick;
}

std::string afv_native::afv::ATCRadioSimulation::getLastTransmitOnFreq(unsigned int freq) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    return mRadioState[freq].lastTransmitCallsign;
}

void afv_native::afv::ATCRadioSimulation::stationTransceiverUpdateCallback(const std::string &stationName, std::map<std::string, std::vector<afv::dto::StationTransceiver>> transceivers) {
    if (transceivers[stationName].size() == 0) {
        return;
    }

    auto it = std::find_if(mRadioState.begin(), mRadioState.end(), [&stationName](const auto &t) {
        return t.second.stationName == stationName;
    });
    if (it != mRadioState.end()) {
        setTransceivers(it->second.Frequency, transceivers[stationName]);
    }
}

bool afv_native::afv::ATCRadioSimulation::getOnHeadset(unsigned int freq) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    return mRadioState.count(freq) != 0 ? mRadioState[freq].onHeadset : true;
}

void afv_native::afv::ATCRadioSimulation::setPlaybackChannelAll(PlaybackChannel channel) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    for (auto radio: mRadioState) {
        mRadioState[radio.first].playbackChannel = channel;
    }
}

void afv_native::afv::ATCRadioSimulation::setPlaybackChannel(unsigned int freq, PlaybackChannel channel) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "setSplitAudioChannels failed, frequency inactive: %i", freq);
        return;
    }
    mRadioState[freq].playbackChannel = channel;
}

afv_native::PlaybackChannel afv_native::afv::ATCRadioSimulation::getPlaybackChannel(unsigned int freq) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    return mRadioState.count(freq) != 0 ? mRadioState[freq].playbackChannel : PlaybackChannel::Both;
}

bool afv_native::afv::ATCRadioSimulation::getRxState(unsigned int freq) {
    return mRadioState.count(freq) != 0 ? mRadioState[freq].rx : false;
}
bool afv_native::afv::ATCRadioSimulation::getTxState(unsigned int freq) {
    return mRadioState.count(freq) != 0 ? mRadioState[freq].tx : false;
}

bool afv_native::afv::ATCRadioSimulation::getXcState(unsigned int freq) {
    return mRadioState.count(freq) != 0 ? mRadioState[freq].xc : false;
}

void afv_native::afv::ATCRadioSimulation::removeFrequency(unsigned int freq) {
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (!isFrequencyActive(freq)) {
        LOG("ATCRadioSimulation", "removeFrequency cancelled, frequency does not exist: %i", freq);
        return;
    }
    ClientEventCallback->invokeAll(ClientEventType::FrequencyRxEnd, &freq, nullptr);
    LOG("ATCRadioSimulation", "FrequencyRxEnd event: %i", freq);
    for (auto callsign: mRadioState[freq].liveTransmittingCallsigns) {
        ClientEventCallback->invokeAll(ClientEventType::StationRxEnd, &freq,
                                       (void *) callsign.c_str());
        LOG("ATCRadioSimulation", "removeFrequency StationRxEnd event: %i: %s", freq,
            callsign.c_str());
    }
    resetRadioFx(freq, false);
    mRadioState.erase(freq);
    LOG("ATCRadioSimulation", "removeFrequency: %i", freq);
}

int afv_native::afv::ATCRadioSimulation::getTransceiverCountForFrequency(unsigned int freq) {
    std::lock_guard<std::mutex> lock(mRadioStateLock);
    if (isFrequencyActive(freq)) {
        return mRadioState[freq].transceivers.size();
    }
    return 0;
}

void afv_native::afv::ATCRadioSimulation::interleave(audio::SampleType *leftChannel, audio::SampleType *rightChannel, audio::SampleType *outputBuffer, size_t numSamples) {
    for (size_t i = 0; i < numSamples; i++) {
        outputBuffer[2 * i] = leftChannel[i]; // Interleave left channel data
        outputBuffer[2 * i + 1] = rightChannel[i]; // Interleave right channel data
    }
}
std::map<unsigned int, AtcRadioState> afv_native::afv::ATCRadioSimulation::getRadioState() {
    std::lock_guard<std::mutex> lock(mRadioStateLock);
    return mRadioState;
}
