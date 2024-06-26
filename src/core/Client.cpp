/* core/Client.cpp
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

#include "afv-native/Client.h"
#include "afv-native/Log.h"
#include "afv-native/afv/VoiceSession.h"
#include "afv-native/afv/params.h"
#include <functional>
#include <memory>

using namespace afv_native;

Client::Client(struct event_base *evBase, std::string resourceBasePath, unsigned int numRadios, const std::string &clientName, std::string baseUrl):
    mFxRes(std::make_shared<afv::EffectResources>(resourceBasePath)), mEvBase(evBase), mTransferManager(mEvBase), mAPISession(mEvBase, mTransferManager, std::move(baseUrl), clientName), mVoiceSession(mAPISession),
    mRadioSim(
        std::make_shared<afv::RadioSimulation>(mEvBase, mFxRes, &mVoiceSession.getUDPChannel(), numRadios)),
    mSpeakerDevice(), mHeadsetDevice(), mClientLatitude(0.0), mClientLongitude(0.0), mClientAltitudeMSLM(0.0), mClientAltitudeGLM(0.0), mRadioState(2), mCallsign(), mTxUpdatePending(false), mWantPtt(false), mPtt(false), mTransceiverUpdateTimer(mEvBase, std::bind(&Client::sendTransceiverUpdate, this)), mClientName(clientName), mAudioApi(0), mAudioInputDeviceName(), mSpeakerDeviceName(), mHeadsetDeviceName(), mSplitAudioChannels(false), ClientEventCallback() {
    mAPISession.StateCallback.addCallback(this, std::bind(&Client::sessionStateCallback, this, std::placeholders::_1));
    mAPISession.AliasUpdateCallback.addCallback(this, std::bind(&Client::aliasUpdateCallback, this));
    mVoiceSession.StateCallback.addCallback(this, std::bind(&Client::voiceStateCallback, this, std::placeholders::_1));
    // forcibly synchronise the RadioSim state.
    mRadioSim->setTxRadio(0);
    for (size_t i = 0; i < mRadioState.size(); i++) {
        mRadioSim->setFrequency(i, mRadioState[i].mNextFreq);
    }
    mRadioSim->setupDevices(&ClientEventCallback);
}

Client::~Client() {
    mVoiceSession.StateCallback.removeCallback(this);
    mAPISession.StateCallback.removeCallback(this);
    mAPISession.AliasUpdateCallback.removeCallback(this);

    // disconnect the radiosim from the UDP channel so if it's held open by the
    // audio device, it doesn't crash the client.
    mRadioSim->setPtt(false);
    mRadioSim->setUDPChannel(nullptr);
}

void Client::setClientPosition(double lat, double lon, double amslm, double aglm) {
    mClientLatitude     = lat;
    mClientLongitude    = lon;
    mClientAltitudeMSLM = amslm;
    mClientAltitudeGLM  = aglm;
}

void Client::setRadioState(unsigned int radioNum, int freq) {
    if (radioNum > mRadioState.size()) {
        return;
    }
    if (mRadioState[radioNum].mNextFreq == freq) {
        // no change.
        return;
    }
    mRadioState[radioNum].mNextFreq = freq;
    // pass down so we get inbound filtering.
    mRadioSim->setFrequency(radioNum, freq);
    if (mRadioState[radioNum].mCurrentFreq != freq) {
        queueTransceiverUpdate();
    }
}

void Client::setTxRadio(unsigned int radioNum) {
    mRadioSim->setTxRadio(radioNum);
}

bool Client::connect() {
    if (!isAPIConnected()) {
        if (mAPISession.getState() != afv::APISessionState::Disconnected) {
            return false;
        }
        mAPISession.Connect();
    } else {
        mVoiceSession.Connect();
    }
    return true;
}

void Client::disconnect() {
    // voicesession must come first.
    if (isVoiceConnected()) {
        mVoiceSession.Disconnect(true);
    } else {
        mAPISession.Disconnect();
    }
}

void Client::setCredentials(const std::string &username, const std::string &password) {
    if (mAPISession.getState() != afv::APISessionState::Disconnected) {
        return;
    }
    mAPISession.setUsername(username);
    mAPISession.setPassword(password);
}

void Client::setCallsign(std::string callsign) {
    if (isVoiceConnected()) {
        return;
    }
    mVoiceSession.setCallsign(callsign);
    mRadioSim->setCallsign(callsign);
    mCallsign = std::move(callsign);
}

void Client::voiceStateCallback(afv::VoiceSessionState state) {
    afv::VoiceSessionError voiceError;
    int                    channelErrno;

    switch (state) {
        case afv::VoiceSessionState::Connected:
            LOG("afv::Client", "Voice Session Connected");
            startAudio();
            queueTransceiverUpdate();
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerConnected, nullptr, nullptr);
            break;
        case afv::VoiceSessionState::Disconnected:
            LOG("afv::Client", "Voice Session Disconnected");
            stopAudio();
            stopTransceiverUpdate();
            // bring down the API session too.
            mAPISession.Disconnect();
            mRadioSim->reset();
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerDisconnected, nullptr, nullptr);
            break;
        case afv::VoiceSessionState::Error:
            LOG("afv::Client", "got error from voice session");
            stopAudio();
            stopTransceiverUpdate();
            // bring down the API session too.
            mAPISession.Disconnect();
            mRadioSim->reset();
            voiceError = mVoiceSession.getLastError();
            if (voiceError == afv::VoiceSessionError::UDPChannelError) {
                channelErrno = mVoiceSession.getUDPChannel().getLastErrno();
                ClientEventCallback.invokeAll(ClientEventType::VoiceServerChannelError, &channelErrno, nullptr);
            } else {
                ClientEventCallback.invokeAll(ClientEventType::VoiceServerError, &voiceError, nullptr);
            }
            break;
    }
}

void Client::sessionStateCallback(afv::APISessionState state) {
    afv::APISessionError sessionError;
    switch (state) {
        case afv::APISessionState::Reconnecting:
            LOG("afv_native::Client", "Reconnecting API Session");
            break;
        case afv::APISessionState::Running:
            LOG("afv_native::Client", "Connected to AFV API Server");
            if (!isVoiceConnected()) {
                mVoiceSession.setCallsign(mCallsign);
                mVoiceSession.Connect();
                mAPISession.updateStationAliases();
            }
            ClientEventCallback.invokeAll(ClientEventType::APIServerConnected, nullptr, nullptr);
            break;
        case afv::APISessionState::Disconnected:
            LOG("afv_native::Client", "Disconnected from AFV API Server.  Terminating sessions");
            // because we only ever commence a normal API Session teardown from a voicesession hook,
            // we don't need to call into voiceSession in this case only.
            ClientEventCallback.invokeAll(ClientEventType::APIServerDisconnected, nullptr, nullptr);
            break;
        case afv::APISessionState::Error:
            LOG("afv_native::Client", "Got error from AFV API Server.  Disconnecting session");
            sessionError = mAPISession.getLastError();
            ClientEventCallback.invokeAll(ClientEventType::APIServerError, &sessionError, nullptr);
            break;
        default:
            // ignore the other transitions.
            break;
    }
}

void Client::startAudio() {
    if (mSpeakerDeviceName.empty() || mHeadsetDeviceName.empty() ||
        mAudioInputDeviceName.empty()) {
        const char *error = "Audio devices are not configured correctly. You will not be able to communicate on voice until you correctly configure your audio devices in the xPilot settings.";
        if (!mInvalidDeviceConfig) {
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
            mInvalidDeviceConfig = true;
        }
        LOG("afv::Client", error);
        return;
    }

    if (!mSpeakerDevice) {
        LOG("afv::Client", "Initializing speaker audio device...");
        mSpeakerDevice = audio::AudioDevice::makeDevice("afv::speaker", mSpeakerDeviceName, mAudioInputDeviceName, mAudioApi, mSplitAudioChannels);

        if (!mSpeakerDevice) {
            const char *error = "Audio Error: Could not initialize speaker device context.";
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
            LOG("afv::Client", error);
            return;
        }
    } else {
        LOG("afv::Client", "Speaker device already exists, skipping creation.");
    }

    if (mSpeakerDevice->openOutput()) {
        mSpeakerDevice->setSink(nullptr);
        mSpeakerDevice->setSource(mRadioSim->speakerDevice());
    } else {
        const char *error = "Audio Error: Could not open speaker audio device. Please check the xPilot audio settings and try again.";
        ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
        LOG("afv::Client", error);
        return;
    }

    if (!mHeadsetDevice) {
        LOG("afv::Client", "Initializing headset audio device...");
        mHeadsetDevice = audio::AudioDevice::makeDevice("afv::headset", mHeadsetDeviceName, mAudioInputDeviceName, mAudioApi, mSplitAudioChannels);

        if (!mHeadsetDevice) {
            const char *error = "Audio Error: Could not initialize headset device context.";
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
            LOG("afv::Client", error);
            return;
        }
    } else {
        LOG("afv::Client", "Headset device already exists, skipping creation.");
    }

    if (mHeadsetDevice->openOutput()) {
        if (mHeadsetDevice->openInput()) {
            mHeadsetDevice->setSink(mRadioSim);
            mHeadsetDevice->setSource(mRadioSim->headsetDevice());
        } else {
            const char *error = "Audio Error: Could not initialize microphone device.";
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
            LOG("afv::Client", error);
            return;
        }
    } else {
        stopAudio();
        const char *error = "Audio Error: Could not initialize headset device. Please check the xPilot audio settings and try again.";
        ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
        LOG("afv::Client", error);
    }
}

void Client::stopAudio() {
    if (mSpeakerDevice) {
        mSpeakerDevice->close();
        mSpeakerDevice.reset();
    }

    if (mHeadsetDevice) {
        mHeadsetDevice->close();
        mHeadsetDevice.reset();
    }
}

std::vector<afv::dto::Transceiver> Client::makeTransceiverDto() {
    std::vector<afv::dto::Transceiver> retSet;
    for (unsigned i = 0; i < mRadioState.size(); i++) {
        retSet.emplace_back(i, mRadioState[i].mNextFreq, mClientLatitude, mClientLongitude, mClientAltitudeMSLM, mClientAltitudeGLM);
    }
    return std::move(retSet);
}

void Client::sendTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    auto transceiverDto = makeTransceiverDto();
    mTxUpdatePending    = true;

    /* ok - magic!
     *
     * so, in order to ensure that we flip the radio states to the CORRECT ONE
     * when the callback fires, we copy capture the update message itself (which
     * is all value copies) and use that to do the internal state update.
     */
    mVoiceSession.postTransceiverUpdate(transceiverDto, [this, transceiverDto](http::Request *r, bool success) {
        if (success && r->getStatusCode() == 200) {
            for (unsigned i = 0; i < this->mRadioState.size(); i++) {
                this->mRadioState[i].mCurrentFreq = transceiverDto[i].Frequency;
            }
            this->mTxUpdatePending = false;
            this->unguardPtt();
        }
    });
    mTransceiverUpdateTimer.enable(afv::afvTransceiverUpdateIntervalMs);
}

void Client::queueTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    mTransceiverUpdateTimer.enable(0);
}

void Client::unguardPtt() {
    if (mWantPtt && !mPtt) {
        LOG("Client", "PTT was guarded - checking.");
        if (!areTransceiversSynced()) {
            LOG("Client", "Freqs still unsync'd.  Restarting update.");
            queueTransceiverUpdate();
            return;
        }
        LOG("Client", "Freqs in sync - allowing PTT now.");
        mPtt = true;
        mRadioSim->setPtt(true);
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr, nullptr);
    }
}

void Client::setPtt(bool pttState) {
    if (pttState) {
        mWantPtt = true;
        // if we're setting the Ptt, we have to check a few things.
        // if we're still pending an update, and the radios are out of step, guard the Ptt.
        if (!areTransceiversSynced() || mTxUpdatePending) {
            if (!mTxUpdatePending) {
                LOG("Client", "Wanted to Open PTT mid-update - guarding");
                queueTransceiverUpdate();
            }
            return;
        }
    } else {
        mWantPtt = false;
    }
    if (mWantPtt == mPtt) {
        return;
    }
    mPtt = mWantPtt;
    mRadioSim->setPtt(mPtt);
    if (mPtt) {
        LOG("Client", "Opened PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr, nullptr);
    } else if (!mWantPtt) {
        LOG("Client", "Closed PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttClosed, nullptr, nullptr);
    }
}

bool Client::areTransceiversSynced() const {
    for (const auto &iter: mRadioState) {
        if (iter.mNextFreq != iter.mCurrentFreq) {
            return false;
        }
    }
    return true;
}

void Client::setAudioInputDevice(std::string inputDevice) {
    mAudioInputDeviceName = inputDevice;
}

void Client::setSpeakerDevice(std::string speakerDevice) {
    mSpeakerDeviceName = speakerDevice;
}

void Client::setHeadsetDevice(std::string headsetDevice) {
    mHeadsetDeviceName = headsetDevice;
}

bool Client::isAPIConnected() const {
    auto sState = mAPISession.getState();
    return sState == afv::APISessionState::Running || sState == afv::APISessionState::Reconnecting;
}

bool Client::isVoiceConnected() const {
    return mVoiceSession.isConnected();
}

void Client::setBaseUrl(std::string newUrl) {
    mAPISession.setBaseUrl(std::move(newUrl));
}

void Client::stopTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
}

void Client::setAudioApi(audio::AudioDevice::Api api) {
    mAudioApi = api;
}

void Client::setRadioGain(unsigned int radioNum, float gain) {
    mRadioSim->setGain(radioNum, gain);
}

void Client::setMicrophoneVolume(float volume) {
    if (volume > 18) {
        volume = 18;
    }
    if (volume < -60) {
        volume = -60;
    }
    volume = (float) pow(10, volume / 20);
    mRadioSim->setMicrophoneVolume(volume);
}

bool Client::getEnableInputFilters() const {
    return mRadioSim->getEnableInputFilters();
}

void Client::setEnableInputFilters(bool enableInputFilters) {
    mRadioSim->setEnableInputFilters(enableInputFilters);
}

double Client::getInputPeak() const {
    if (mRadioSim) {
        return mRadioSim->getPeak();
    }
    return -INFINITY;
}

double Client::getInputVu() const {
    if (mRadioSim) {
        return mRadioSim->getVu();
    }
    return -INFINITY;
}

void Client::setEnableOutputEffects(bool enableEffects) {
    mRadioSim->setEnableOutputEffects(enableEffects);
}

void Client::setEnableHfSquelch(bool enableSquelch) {
    mRadioSim->setEnableHfSquelch(enableSquelch);
}

void Client::setOnHeadset(unsigned int radio, bool onHeadset) {
    mRadioSim->setOnHeadset(radio, onHeadset);
}

void Client::setSplitAudioChannels(bool split) {
    mSplitAudioChannels = split;
    mRadioSim->setSplitAudioChannels(split);
}

void Client::aliasUpdateCallback() {
    ClientEventCallback.invokeAll(ClientEventType::StationAliasesUpdated, nullptr, nullptr);
}

std::vector<afv::dto::Station> Client::getStationAliases() const {
    return std::move(mAPISession.getStationAliases());
}

std::shared_ptr<const afv::RadioSimulation> Client::getRadioSimulation() const {
    return mRadioSim;
}

std::shared_ptr<const audio::AudioDevice> Client::getHeadsetDevice() const {
    return mHeadsetDevice;
}

std::shared_ptr<const audio::AudioDevice> Client::getSpeakerDevice() const {
    return mSpeakerDevice;
}

bool Client::getRxActive(unsigned int radioNumber) {
    if (mRadioSim) {
        return mRadioSim->getRxActive(radioNumber);
    }
    return false;
}

bool Client::getTxActive(unsigned int radioNumber) {
    if (mRadioSim) {
        return mRadioSim->getTxActive(radioNumber);
    }
    return false;
}
