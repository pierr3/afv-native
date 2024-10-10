//
//  atcClient.cpp
//  afv_native
//
//  Created by Mike Evans on 10/18/20.
//

#include "afv-native/atcClient.h"
#include "afv-native/Log.h"
#include "afv-native/afv/ATCRadioSimulation.h"
#include "afv-native/afv/VoiceSession.h"
#include "afv-native/afv/params.h"
#include "afv-native/event.h"
#include <functional>
#include <memory>

using namespace afv_native;

ATCClient::ATCClient(struct event_base *evBase, const std::string &resourceBasePath, const std::string &clientName, std::string baseUrl) 
try :
    mFxRes(std::make_shared<afv::EffectResources>(resourceBasePath)), mEvBase(evBase), mTransferManager(mEvBase), mAPISession(mEvBase, mTransferManager, std::move(baseUrl), clientName), mVoiceSession(mAPISession),
    mATCRadioStack(std::make_shared<afv::ATCRadioSimulation>(mEvBase,
                                                             mFxRes,
                                                             &mVoiceSession.getUDPChannel())),
    mAudioDevice(), mSpeakerDevice(), mCallsign(), mTxUpdatePending(false), mWantPtt(false), mPtt(false), mAtisRecording(false), mTransceiverUpdateTimer(mEvBase, std::bind(&ATCClient::sendTransceiverUpdate, this)), mClientName(clientName), mAudioApi(-1), mAudioInputDeviceId(), mAudioOutputDeviceId(), ClientEventCallback() {
    LOG("ATCClient", "Entry");
    mAPISession.StateCallback.addCallback(this, std::bind(&ATCClient::sessionStateCallback, this, std::placeholders::_1));
    mAPISession.AliasUpdateCallback.addCallback(this, std::bind(&ATCClient::aliasUpdateCallback, this));
    mAPISession.StationTransceiversUpdateCallback.addCallback(this, std::bind(&ATCClient::stationTransceiversUpdateCallback, this, std::placeholders::_1));
    mAPISession.StationVccsCallback.addCallback(this, std::bind(&ATCClient::stationVccsCallback, this, std::placeholders::_1, std::placeholders::_2));
    mAPISession.StationSearchCallback.addCallback(this, std::bind(&ATCClient::stationSearchCallback, this, std::placeholders::_1, std::placeholders::_2));
    mVoiceSession.StateCallback.addCallback(this, std::bind(&ATCClient::voiceStateCallback, this, std::placeholders::_1));
    mATCRadioStack->setupDevices(&ClientEventCallback);
    LOG("ATCClient", "Event Base is %ld", evBase);
}
catch (std::exception &e) {
    LOG("ATCClient","Exception in Constructor: %s",e.what());
}

ATCClient::~ATCClient() {
    mVoiceSession.StateCallback.removeCallback(this);
    mAPISession.StateCallback.removeCallback(this);
    mAPISession.AliasUpdateCallback.removeCallback(this);

    // disconnect the radiosim from the UDP channel so if it's held open by the
    // audio device, it doesn't crash the client.

    mATCRadioStack->setPtt(false);
    mATCRadioStack->setUDPChannel(nullptr);
}

void ATCClient::setClientPosition(double lat, double lon, double amslm, double aglm) {
    mATCRadioStack->setClientPosition(lat, lon, amslm, aglm);
}

std::string ATCClient::lastTransmitOnFreq(unsigned int freq) {
    return mATCRadioStack->getLastTransmitOnFreq(freq);
}

void ATCClient::setTx(unsigned int freq, bool active) {
    mATCRadioStack->setTx(freq, active);
    queueTransceiverUpdate();
}

void ATCClient::setRx(unsigned int freq, bool active) {
    mATCRadioStack->setRx(freq, active);
    queueTransceiverUpdate();
}

void ATCClient::setXc(unsigned int freq, bool active) {
    mATCRadioStack->setXc(freq, active);
    queueTransceiverUpdate();
}

void afv_native::ATCClient::setCrossCoupleAcross(unsigned int freq, bool active) {
    mATCRadioStack->setCrossCoupleAcross(freq, active);
    queueTransceiverUpdate();
}

bool ATCClient::connect() {
    LOG("afv::ATCClient", "Attempting to connect");
    if (!isAPIConnected()) {
        if (mAPISession.getState() != afv::APISessionState::Disconnected) {
            LOG("afv::ATCClient",
                "API State is not set to disconnected on connect attempt, "
                "current state is %d",
                static_cast<int>(mAPISession.getState()));
            return false;
        }
        LOG("afv::ATCClient", "Connecting to API Session");
        mAPISession.Connect();
    } else {
        LOG("afv::ATCClient", "API Is Connected.. attempting Voice Session Connect");
        mVoiceSession.Connect();
    }
    return true;
}

void ATCClient::disconnect() {
    // voicesession must come first.
    if (isVoiceConnected()) {
        mVoiceSession.Disconnect(true);
        mATCRadioStack->reset();
    } else {
        mAPISession.Disconnect();
    }
}

void ATCClient::setCredentials(const std::string &username, const std::string &password) {
    if (mAPISession.getState() != afv::APISessionState::Disconnected) {
        return;
    }
    mAPISession.setUsername(username);
    mAPISession.setPassword(password);
}

void ATCClient::setCallsign(std::string callsign) {
    if (isVoiceConnected()) {
        return;
    }
    mVoiceSession.setCallsign(callsign);
    mATCRadioStack->setCallsign(callsign);
    mCallsign = std::move(callsign);
}

void ATCClient::voiceStateCallback(afv::VoiceSessionState state) {
    afv::VoiceSessionError voiceError;
    int                    channelErrno;

    switch (state) {
        case afv::VoiceSessionState::Connected:
            LOG("afv::ATCClient", "Voice Session Connected");
            startAudio();
            queueTransceiverUpdate();
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerConnected, nullptr, nullptr);
            break;
        case afv::VoiceSessionState::Disconnected:
            LOG("afv::ATCClient", "Voice Session Disconnected");
            stopAudio();
            stopTransceiverUpdate();
            // bring down the API session too.
            mAPISession.Disconnect();
            mATCRadioStack->reset();
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerDisconnected, nullptr, nullptr);
            break;
        case afv::VoiceSessionState::Error:
            LOG("afv::ATCClient", "got error from voice session");
            stopAudio();
            stopTransceiverUpdate();
            // bring down the API session too.
            mAPISession.Disconnect();
            mATCRadioStack->reset();
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

void ATCClient::sessionStateCallback(afv::APISessionState state) {
    afv::APISessionError sessionError;
    switch (state) {
        case afv::APISessionState::Reconnecting:
            LOG("afv_native::ATCClient", "Reconnecting API Session");
            break;
        case afv::APISessionState::Running:
            LOG("afv_native::ATCClient", "Connected to AFV API Server");
            if (!isVoiceConnected()) {
                mVoiceSession.setCallsign(mCallsign);
                mVoiceSession.Connect();
                mAPISession.updateStationAliases();
            }
            ClientEventCallback.invokeAll(ClientEventType::APIServerConnected, nullptr, nullptr);
            break;
        case afv::APISessionState::Disconnected:
            LOG("afv_native::ATCClient", "Disconnected from AFV API Server.  Terminating sessions");
            // because we only ever commence a normal API Session teardown
            // from a voicesession hook, we don't need to call into
            // voiceSession in this case only.
            ClientEventCallback.invokeAll(ClientEventType::APIServerDisconnected, nullptr, nullptr);
            break;
        case afv::APISessionState::Error:
            LOG("afv_native::ATCClient", "Got error from AFV API Server.  Disconnecting session");
            sessionError = mAPISession.getLastError();
            ClientEventCallback.invokeAll(ClientEventType::APIServerError, &sessionError, nullptr);
            break;
        default:
            // ignore the other transitions.
            break;
    }
}

void ATCClient::startAudio() {
    if (mAudioSpeakerDeviceId.empty() || mAudioOutputDeviceId.empty() ||
        mAudioInputDeviceId.empty() || mAudioApi == -1) {
        LOG("afv::ATCClient", "Audio device and API not set, cannot start audio");
        return;
    }

    mAudioStoppedThroughCallback = false;
    if (!mSpeakerDevice) {
        LOG("afv::ATCClient", "Initialising Speaker Audio...");
        mSpeakerDevice = audio::AudioDevice::makeDevice("afv::speaker", mAudioSpeakerDeviceId, mAudioInputDeviceId, mAudioApi);
        LOG("afv::ATCClient", "Speaker Device %s created", mAudioSpeakerDeviceId.c_str());
        if (!mSpeakerDevice) {
            LOG("afv::ATCClient", "Could not initiate speaker audio context.");
            const char *error = "Could not initiate speaker audio context.";
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
        } else {
            mSpeakerDevice->setNotificationFunc(std::bind(&ATCClient::deviceStoppedCallback, this, std::placeholders::_1, std::placeholders::_2));
            LOG("afv::ATCClient", "Speaker Device %s notification setup",
                mAudioSpeakerDeviceId.c_str());
        }
    } else {
        LOG("afv::ATCClient", "Speaker device already exists, skipping creation");
    }
    mSpeakerDevice->setSink(nullptr);
    mSpeakerDevice->setSource(mATCRadioStack->speakerDevice());
    LOG("afv::ATCClient", "Speaker Device %s fully setup", mAudioSpeakerDeviceId.c_str());

    if (!mSpeakerDevice->openOutput()) {
        LOG("afv::ATCClient", "Unable to open Speaker audio device.");
        const char *error = "Unable to open Speaker audio device.";
        stopAudio();
        ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
    }
    LOG("afv::ATCClient", "Speaker Device %s output opened",
        mAudioSpeakerDeviceId.c_str());

    if (!mAudioDevice) {
        LOG("afv::ATCClient", "Initialising Headset Audio...");
        mAudioDevice = audio::AudioDevice::makeDevice("afv::headset", mAudioOutputDeviceId, mAudioInputDeviceId, mAudioApi, true);
        LOG("afv::ATCClient", "Headset Device %s created", mAudioOutputDeviceId.c_str());
        if (!mAudioDevice) {
            LOG("afv::ATCClient", "Could not initiate headset audio context.");
            const char *error = "Could not initiate headset audio context.";
            ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
        } else {
            mAudioDevice->setNotificationFunc(std::bind(&ATCClient::deviceStoppedCallback, this, std::placeholders::_1, std::placeholders::_2));
            LOG("afv::ATCClient", "Headset Device %s notification setup",
                mAudioOutputDeviceId.c_str());
        }
    } else {
        LOG("afv::ATCClient", "Headset device already exists, skipping creation");
    }
    mAudioDevice->setSink(mATCRadioStack);
    mAudioDevice->setSource(mATCRadioStack->headsetDevice());
    LOG("afv::ATCClient", "Headset Device %s fully setup", mAudioOutputDeviceId.c_str());

    if (mAudioDevice->openOutput()) {
        LOG("afv::ATCClient", "Headset output device opened");
        if (!mAudioDevice->openInput()) {
            LOG("afv::ATCClient", "Couldn't initialize headset microphone device");
            ClientEventCallback.invokeAll(ClientEventType::InputDeviceError, nullptr, nullptr);
        } else {
            LOG("afv::ATCClient", "Headset input device opened");
        }
    } else {
        LOG("afv::ATCClient", "Unable to open Headset output device.");
        const char *error = "Unable to open Headset audio device.";
        stopAudio();
        ClientEventCallback.invokeAll(ClientEventType::AudioError, reinterpret_cast<void *>(const_cast<char *>(error)), nullptr);
    }
}

void ATCClient::stopAudio() {
    if (mAudioDevice) {
        mAudioDevice->close();
        mAudioDevice.reset();
    }
    if (mSpeakerDevice) {
        mSpeakerDevice->close();
        mSpeakerDevice.reset();
    }
    LOG("afv::ATCClient", "Audio Stopped");
}

std::vector<afv::dto::Transceiver> ATCClient::makeTransceiverDto() {
    return std::move(mATCRadioStack->makeTransceiverDto());
}

void ATCClient::sendTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    auto transceiverDto = makeTransceiverDto();
    mTxUpdatePending    = true;

    mVoiceSession.postTransceiverUpdate(transceiverDto, [this](http::Request *r, bool success) {
        if (success && r->getStatusCode() == 200) {
            this->mTxUpdatePending = false;
            this->unguardPtt();
        }
    });

    // We now also update any cross coupled transceivers
    mVoiceSession.postCrossCoupleGroupUpdate(mATCRadioStack->makeCrossCoupleGroupDto(), [this](http::Request *r, bool success) {
        if (!success) {
            LOG("ATCClient", "Failed to post cross couple transceivers update with code %s",
                std::to_string(r->getStatusCode()).c_str());
        }
    });

    // We now also get an update on the transceivers for all active stations
    // This is to handle child stations transceiver changes
    /*for (const auto &el : mATCRadioStack->mRadioState) {
        if (el.second.stationName.size()>0)
            this->requestStationTransceivers(el.second.stationName);
    }*/

    mTransceiverUpdateTimer.enable(afv::afvATCTransceiverUpdateIntervalMs);
}

void ATCClient::queueTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    mTransceiverUpdateTimer.enable(0);
}

void ATCClient::unguardPtt() {
    if (mWantPtt && !mPtt) {
        LOG("ATCClient", "PTT was guarded - checking.");
        mPtt = true;
        mATCRadioStack->setPtt(true);
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr, nullptr);
    }
}

void ATCClient::setRecordAtis(bool state) {
    // if (!mPtt && mAudioDevice) {
    //     mAtisRecording = !mAtisRecording;
    //     mATCRadioStack->setRecordAtis(mAtisRecording);
    // }
}

bool ATCClient::isAtisRecording() {
    return mAtisRecording;
}

void ATCClient::startAtisPlayback(std::string atisCallsign, unsigned int freq) {
    // if (!isAtisRecording() && isVoiceConnected()) {
    //     this->addFrequency(freq, true, atisCallsign);
    //     this->linkTransceivers(atisCallsign, freq);
    //     mATCRadioStack->startAtisPlayback(atisCallsign);
    // }
};

void ATCClient::stopAtisPlayback() {
    // mATCRadioStack->stopAtisPlayback();
};

bool ATCClient::isAtisPlayingBack() {
    // return mATCRadioStack->isAtisPlayingBack();
    return false;
};

void ATCClient::listenToAtis(bool state) {
    // mATCRadioStack->listenToAtis(state);
};

bool ATCClient::isAtisListening() {
    // return mATCRadioStack->isAtisListening();
    return false;
};

void ATCClient::setPtt(bool pttState) {
    if (pttState) {
        mWantPtt = true;
        // if we're setting the Ptt, we have to check a few things.
        // if we're still pending an update, and the radios are out of
        // step, guard the Ptt.
        if (mTxUpdatePending) {
            if (!mTxUpdatePending) {
                LOG("ATCClient", "Wanted to Open PTT mid-update - guarding");
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
    mATCRadioStack->setPtt(mPtt);
    if (mPtt) {
        LOG("Client", "Opened PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr, nullptr);
    } else if (!mWantPtt) {
        LOG("Client", "Closed PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttClosed, nullptr, nullptr);
    }
}

void ATCClient::setRT(bool rtState) {
    // mATCRadioStack->setRT(rtState);
}

void ATCClient::setAudioInputDevice(std::string inputDevice) {
    mAudioInputDeviceId = inputDevice;
}

void ATCClient::setAudioOutputDevice(std::string outputDevice) {
    mAudioOutputDeviceId = outputDevice;
}

void ATCClient::setSpeakerOutputDevice(std::string outputDevice) {
    mAudioSpeakerDeviceId = outputDevice;
}

bool ATCClient::isAPIConnected() const {
    auto sState = mAPISession.getState();
    return sState == afv::APISessionState::Running || sState == afv::APISessionState::Reconnecting;
}

bool ATCClient::isVoiceConnected() const {
    return mVoiceSession.isConnected();
}

void ATCClient::setBaseUrl(std::string newUrl) {
    mAPISession.setBaseUrl(std::move(newUrl));
}

void ATCClient::stopTransceiverUpdate() {
    mTransceiverUpdateTimer.disable();
}

void ATCClient::setAudioApi(audio::AudioDevice::Api api) {
    mAudioApi = api;
}

void ATCClient::setRadioGain(unsigned int freq, float gain) {
    mATCRadioStack->setGain(freq, gain);
}

void ATCClient::setRadioGainAll(float gain) {
    mATCRadioStack->setGainAll(gain);
}

bool ATCClient::getEnableInputFilters() const {
    return mATCRadioStack->getEnableInputFilters();
}

void ATCClient::setEnableInputFilters(bool enableInputFilters) {
    mATCRadioStack->setEnableInputFilters(enableInputFilters);
}

double ATCClient::getInputPeak() const {
    if (mATCRadioStack) {
        return mATCRadioStack->getPeak();
    }
    return -INFINITY;
}

double ATCClient::getInputVu() const {
    if (mATCRadioStack) {
        return mATCRadioStack->getVu();
    }
    return -INFINITY;
}

void ATCClient::setEnableOutputEffects(bool enableEffects) {
    mATCRadioStack->setEnableOutputEffects(enableEffects);
}

void ATCClient::aliasUpdateCallback() {
    ClientEventCallback.invokeAll(ClientEventType::StationAliasesUpdated, nullptr, nullptr);
}

void ATCClient::stationVccsCallback(std::string stationName, std::map<std::string, unsigned int> vccs) {
    ClientEventCallback.invokeAll(ClientEventType::VccsReceived,
                                  (void *) stationName.c_str(), &vccs);
}

void ATCClient::stationSearchCallback(bool found, std::pair<std::string, unsigned int> data) {
    ClientEventCallback.invokeAll(ClientEventType::StationDataReceived, &found, &data);
}

void ATCClient::getStation(std::string callsign) {
    mAPISession.getStation(callsign);
}

void ATCClient::stationTransceiversUpdateCallback(std::string stationName) {
    auto transceivers = getStationTransceivers();
    LOG("ATCClient", "Receiving new transceivers for station %s", stationName.c_str());
    // We can now link any pending new transceivers if we had requested them
    if (linkNewTransceiversFrequencyFlag > 0) {
        if (transceivers[stationName].size() > 0) {
            this->linkTransceivers(stationName, linkNewTransceiversFrequencyFlag);
        } else {
            LOG("ATCClient", "Tried to acquire new transceivers but did not find any for station");
        }

        linkNewTransceiversFrequencyFlag = -1;
    } else {
        // We got new station transceivers that we don't need to link
        // immediately, but can wait until the next transceiver update
        mATCRadioStack->stationTransceiverUpdateCallback(stationName, transceivers);
    }

    ClientEventCallback.invokeAll(ClientEventType::StationTransceiversUpdated,
                                  (void *) stationName.c_str(), nullptr);
}

std::map<std::string, std::vector<afv::dto::StationTransceiver>> ATCClient::getStationTransceivers() const {
    return mAPISession.getStationTransceivers();
}

std::vector<afv::dto::Station> ATCClient::getStationAliases() const {
    return std::move(mAPISession.getStationAliases());
}

void ATCClient::logAudioStatistics() {
    if (mAudioDevice) {
        LOG("ATCClient", "Headset Buffer Underflows: %d",
            mAudioDevice->OutputUnderflows.load());
        LOG("ATCClient", "Speaker Buffer Underflows: %d",
            mSpeakerDevice->OutputUnderflows.load());
        LOG("ATCClient", "Input Buffer Overflows: %d",
            mAudioDevice->InputOverflows.load());
    }
}

std::shared_ptr<const audio::AudioDevice> ATCClient::getAudioDevice() const {
    return mAudioDevice;
}

bool ATCClient::getRxActive(unsigned int freq) {
    if (mATCRadioStack) {
        return mATCRadioStack->getRxActive(freq);
    }
    return false;
}

bool ATCClient::getTxActive(unsigned int freq) {
    if (mATCRadioStack) {
        return mATCRadioStack->getTxActive(freq);
    }
    return false;
}

bool ATCClient::GetTxState(unsigned int freq) {
    if (mATCRadioStack) {
        return mATCRadioStack->getTxState(freq);
    }
    return false;
};

bool ATCClient::GetXcState(unsigned int freq) {
    if (mATCRadioStack) {
        return mATCRadioStack->getXcState(freq);
    }
    return false;
};

bool ATCClient::GetRxState(unsigned int freq) {
    if (mATCRadioStack) {
        return mATCRadioStack->getRxState(freq);
    }
    return false;
};

bool afv_native::ATCClient::GetCrossCoupleAcrossState(unsigned int freq) {
    return mATCRadioStack->getCrossCoupleAcrossState(freq);
};

void ATCClient::setOnHeadset(unsigned int freq, bool onHeadset) {
    mATCRadioStack->setOnHeadset(freq, onHeadset);
}

bool ATCClient::getOnHeadset(unsigned int freq) {
    return mATCRadioStack->getOnHeadset(freq);
}

void ATCClient::requestStationTransceivers(std::string inStation) {
    mAPISession.requestStationTransceivers(inStation);
}

void ATCClient::requestStationVccs(std::string inStation) {
    mAPISession.requestStationVccs(inStation);
}

bool ATCClient::addFrequency(unsigned int freq, bool onHeadset, std::string stationName) {
    bool hasBeenAdded = mATCRadioStack->addFrequency(freq, onHeadset, stationName, this->activeHardware);
    if (hasBeenAdded) {
        queueTransceiverUpdate();
    }
    return hasBeenAdded;
}

bool ATCClient::isFrequencyActive(unsigned int freq) {
    return mATCRadioStack->isFrequencyActive(freq);
}

void ATCClient::removeFrequency(unsigned int freq) {
    mATCRadioStack->removeFrequency(freq);
    queueTransceiverUpdate();
}

void ATCClient::setHardware(HardwareType hardware) {
    this->activeHardware = hardware;
}

void ATCClient::setManualTransceivers(unsigned int freq, std::vector<afv::dto::StationTransceiver> transceivers) {
    if (transceivers.size() > 0) {
        mATCRadioStack->setTransceivers(freq, transceivers);
        queueTransceiverUpdate();
    }
}

void ATCClient::linkTransceivers(std::string callsign, unsigned int freq) {
    auto transceivers = getStationTransceivers();
    if (transceivers[callsign].size() > 0) {
        mATCRadioStack->setTransceivers(freq, transceivers[callsign]);
        queueTransceiverUpdate();
    } else {
        linkNewTransceiversFrequencyFlag = freq;
        this->requestStationTransceivers(callsign);
        LOG("ATCClient", "Need to fetch transceivers for station %s", callsign.c_str());
    }
}

void ATCClient::setTick(std::shared_ptr<audio::ITick> tick) {
    mATCRadioStack->setTick(tick);
}

void afv_native::ATCClient::deviceStoppedCallback(std::string deviceName, int errorCode) {
    if (errorCode != 0) {
        return;
    }

    LOG("afv::ATCClient",
        "Audio device %s was stopped, potential error (disconnected while in "
        "use, etc)",
        deviceName.c_str());

    ClientEventCallback.invokeAll(ClientEventType::AudioDeviceStoppedError,
                                  (void *) deviceName.c_str(), nullptr);
}

void afv_native::ATCClient::setPlaybackChannel(unsigned int freq, PlaybackChannel channel) {
    mATCRadioStack->setPlaybackChannel(freq, channel);
}

void afv_native::ATCClient::setPlaybackChannelAll(PlaybackChannel channel) {
    mATCRadioStack->setPlaybackChannelAll(channel);
}

afv_native::PlaybackChannel afv_native::ATCClient::getPlaybackChannel(unsigned int freq) {
    return mATCRadioStack->getPlaybackChannel(freq);
}
int afv_native::ATCClient::getTransceiverCountForFrequency(unsigned int freq) {
    return mATCRadioStack->getTransceiverCountForFrequency(freq);
}
std::map<unsigned int, afv::AtcRadioState> afv_native::ATCClient::getRadioState() {
    return mATCRadioStack->getRadioState();
}

void afv_native::ATCClient::reset() {
    mATCRadioStack->reset();
}
