#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <algorithm>
#include <iterator> 

#include "afv-native/atcClientWrapper.h"
#include "afv-native/atcClient.h"
#include <event2/event.h>

namespace atcapi
{
    struct event_base* ev_base;

    std::mutex afvMutex;
    std::unique_ptr<afv_native::ATCClient> client;
    std::unique_ptr<std::thread> eventThread;
    std::atomic<bool>keepAlive{ false };
    std::atomic<bool> isInitialized{ false };
}

using namespace atcapi;

afv_native::api::atcClient::atcClient(std::string clientName, std::string resourcePath) {
        #ifdef WIN32
            WORD wVersionRequested;
            WSADATA wsaData;
            wVersionRequested = MAKEWORD(2, 2);
            WSAStartup(wVersionRequested, &wsaData);
        #endif

        ev_base = event_base_new();

        client = std::make_unique<afv_native::ATCClient>(ev_base, resourcePath, clientName);

        keepAlive = true;
        eventThread = std::make_unique<std::thread>([]
        {
            while (keepAlive)
            {
                event_base_loop(ev_base, EVLOOP_NONBLOCK);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        isInitialized = true;
}

afv_native::api::atcClient::~atcClient() {
    keepAlive = false;
    if (eventThread->joinable())
    {
        eventThread->join();
    }
    client.reset();
    isInitialized = false;
    #ifdef WIN32
        WSACleanup();
    #endif
}

bool afv_native::api::atcClient::IsInitialized() {
    return isInitialized;
}

void afv_native::api::atcClient::SetCredentials(std::string username, std::string password) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setCredentials(std::string(username), std::string(password));
}

void afv_native::api::atcClient::SetCallsign(std::string callsign) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setCallsign(std::string(callsign));
}

void afv_native::api::atcClient::SetClientPosition(double lat, double lon, double amslm, double aglm) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setClientPosition(lat, lon, amslm, aglm);
}

bool afv_native::api::atcClient::IsVoiceConnected() {
    return client->isVoiceConnected();
}

bool afv_native::api::atcClient::IsAPIConnected() {
    return client->isAPIConnected();
}

bool afv_native::api::atcClient::Connect() {
    return client->connect();
}

void afv_native::api::atcClient::Disconnect() {
    return client->disconnect();
}

void afv_native::api::atcClient::SetAudioApi(unsigned int api) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setAudioApi(api);
}

std::map<unsigned int, std::string> afv_native::api::atcClient::GetAudioApis() {
    return afv_native::audio::AudioDevice::getAPIs();
}

void afv_native::api::atcClient::SetAudioInputDevice(std::string inputDevice) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setAudioInputDevice(inputDevice);
}

void afv_native::api::atcClient::SetAudioOutputDevice(std::string outputDevice) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setAudioOutputDevice(outputDevice);
}

void afv_native::api::atcClient::SetAudioSpeakersOutputDevice(std::string outputDevice) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setSpeakerOutputDevice(outputDevice);
}

std::vector<std::string> afv_native::api::atcClient::GetAudioInputDevices(unsigned int mAudioApi) {
    std::vector<std::string> out;
    auto devices = afv_native::audio::AudioDevice::getCompatibleInputDevicesForApi(mAudioApi);
    
    std::transform(devices.begin(), devices.end(), std::back_inserter(out),
    [](auto &kv){ return kv.second.name;});

    return out;
}

std::vector<std::string> afv_native::api::atcClient::GetAudioOutputDevices(unsigned int mAudioApi) {
    std::vector<std::string> out;
    auto devices = afv_native::audio::AudioDevice::getCompatibleOutputDevicesForApi(mAudioApi);
    
    std::transform(devices.begin(), devices.end(), std::back_inserter(out),
    [](auto &kv){ return kv.second.name;});

    return out;
}

double afv_native::api::atcClient::GetInputPeak() const {
    return client->getInputPeak();
}

double afv_native::api::atcClient::GetInputVu() const {
    return client->getInputVu();
}

void afv_native::api::atcClient::SetEnableInputFilters(bool enableInputFilters) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setEnableInputFilters(enableInputFilters);
}

void afv_native::api::atcClient::SetEnableOutputEffects(bool enableEffects) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setEnableOutputEffects(enableEffects);
}

bool afv_native::api::atcClient::GetEnableInputFilters() const {
    return client->getEnableInputFilters();
}

void afv_native::api::atcClient::StartAudio() {
    client->startAudio();
}

void afv_native::api::atcClient::StopAudio() {
    client->stopAudio();
}

bool afv_native::api::atcClient::IsAudioRunning() {
    if (!client->mAudioDevice)
        return false;
    else
        return true;
}

void afv_native::api::atcClient::SetTx(unsigned int freq, bool active) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setTx(freq, active);
}

void afv_native::api::atcClient::SetRx(unsigned int freq, bool active) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setRx(freq, active);
}

void afv_native::api::atcClient::SetXc(unsigned int freq, bool active) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setXc(freq, active);
}

void afv_native::api::atcClient::SetOnHeadset(unsigned int freq, bool active) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setOnHeadset(freq, active);
}

bool afv_native::api::atcClient::GetOnHeadset(unsigned int freq) {
    return client->getOnHeadset(freq);
}

bool afv_native::api::atcClient::GetTxActive(unsigned int freq) {
    return client->getTxActive(freq);
};

bool afv_native::api::atcClient::GetRxActive(unsigned int freq) {
    return client->getRxActive(freq);
};

bool afv_native::api::atcClient::GetTxState(unsigned int freq) {
    return client->GetTxState(freq);
};

bool afv_native::api::atcClient::GetXcState(unsigned int freq) {
    return client->GetXcState(freq);
};

bool afv_native::api::atcClient::GetRxState(unsigned int freq) {
    return client->GetRxState(freq);
};

void afv_native::api::atcClient::UseTransceiversFromStation(std::string station, int freq) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->linkTransceivers(station, freq);
};

int afv_native::api::atcClient::GetTransceiverCountForStation(std::string station) {
    auto tcs = client->getStationTransceivers();
    return tcs[station].size();
};

void afv_native::api::atcClient::SetRadiosGain(float gain) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setRadioGainAll(gain);
}

void afv_native::api::atcClient::FetchTransceiverInfo(std::string station) {
    client->requestStationTransceivers(station);
}

void afv_native::api::atcClient::SearchStation(std::string station, unsigned int freq) {
    client->searchForStation(station, freq);
}

void afv_native::api::atcClient::FetchStationVccs(std::string station) {
    client->requestStationVccs(station);
}

void afv_native::api::atcClient::SetPtt(bool pttState) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setPtt(pttState);
}

std::string afv_native::api::atcClient::LastTransmitOnFreq(unsigned int freq) {
    return client->lastTransmitOnFreq(freq);
}

void afv_native::api::atcClient::AddFrequency(unsigned int freq, std::string stationName) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->addFrequency(freq, true, stationName);
}

void afv_native::api::atcClient::RemoveFrequency(unsigned int freq) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->removeFrequency(freq);
}

bool afv_native::api::atcClient::IsFrequencyActive(unsigned int freq) {
    return client->isFrequencyActive(freq);
}

void afv_native::api::atcClient::SetAtisRecording(bool state) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setRecordAtis(state);
}

bool afv_native::api::atcClient::IsAtisRecording() {
    return client->isAtisRecording();
}

void afv_native::api::atcClient::SetAtisListening(bool state) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->listenToAtis(state);
}

bool afv_native::api::atcClient::IsAtisListening() {
    return client->isAtisListening();
}

void afv_native::api::atcClient::StartAtisPlayback(std::string callsign, unsigned int freq) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->startAtisPlayback(callsign, freq);
}

void afv_native::api::atcClient::StopAtisPlayback() {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->stopAtisPlayback();
}

bool afv_native::api::atcClient::IsAtisPlayingBack() {
    return client->isAtisPlayingBack();
}

void afv_native::api::atcClient::RaiseClientEvent(std::function<void(afv_native::ClientEventType, void*, void*)> callback)
{
    client->ClientEventCallback.addCallback(nullptr, [callback](afv_native::ClientEventType evt, void* data, void* data2)
    {
        callback(evt, data, data2);
    });
}