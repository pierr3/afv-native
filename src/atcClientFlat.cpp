#include "afv-native/atcClientFlat.h"
#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

struct ATCClientHandle_ {
    afv_native::api::atcClient *impl;

    ATCClientHandle_(char *clientName, char *resourcePath, char *baseURL) {
        impl = new afv_native::api::atcClient(std::string(clientName), std::string(resourcePath), std::string(baseURL));
    }

    ~ATCClientHandle_() {
        if (impl) {
            delete impl;
            impl = NULL;
        }
    }
};

AFV_NATIVE_API ATCClientHandle ATCClient_Create(char *clientName, char *resourcePath, char *baseURL) {
    LOG("FLAT", "ATC Create client %s resource %s base %s",clientName, resourcePath, baseURL);
    return new ATCClientHandle_(clientName, resourcePath, baseURL);
}

AFV_NATIVE_API void ATCClient_Destroy(ATCClientHandle handle) {
    delete handle;
}

AFV_NATIVE_API bool ATCClient_IsInitialized(ATCClientHandle handle) {
    return handle->impl->IsInitialized();
}

AFV_NATIVE_API void ATCClient_SetCredentials(ATCClientHandle handle, char *username, char *password) {
    handle->impl->SetCredentials(username, password);
}

AFV_NATIVE_API void ATCClient_SetCallsign(ATCClientHandle handle, char *callsign) {
    handle->impl->SetCallsign(callsign);
}

AFV_NATIVE_API void ATCClient_SetClientPosition(ATCClientHandle handle, double lat, double lon, double amslm, double aglm) {
    handle->impl->SetClientPosition(lat, lon, amslm, aglm);
}

AFV_NATIVE_API bool ATCClient_IsVoiceConnected(ATCClientHandle handle) {
    return handle->impl->IsVoiceConnected();
}

AFV_NATIVE_API bool ATCClient_IsAPIConnected(ATCClientHandle handle) {
    return handle->impl->IsAPIConnected();
}

AFV_NATIVE_API bool ATCClient_Connect(ATCClientHandle handle) {
    return handle->impl->Connect();
}

AFV_NATIVE_API void ATCClient_Disconnect(ATCClientHandle handle) {
    return handle->impl->Disconnect();
}

AFV_NATIVE_API void ATCClient_SetAudioApi(ATCClientHandle handle, unsigned int api) {
    handle->impl->SetAudioApi(api);
}

AFV_NATIVE_API void ATCClient_GetAudioApis(ATCClientHandle handle, AudioApisCallback callback) {
    typedef std::map<int, std::string> MapType;
    std::vector<std::string>           v;
    auto                               m = handle->impl->GetAudioApis();
    for (MapType::iterator it = m.begin(); it != m.end(); ++it) {
        v.push_back(it->second);
        callback(it->first, it->second.c_str());
    }
}

AFV_NATIVE_API void ATCClient_FreeAudioApis(ATCClientHandle handle, char **apis) {
    handle->impl->FreeAudioApis(apis);
}

AFV_NATIVE_API void ATCClient_SetAudioInputDevice(ATCClientHandle handle, char *inputDevice) {
    handle->impl->SetAudioInputDevice(inputDevice);
}

AFV_NATIVE_API void ATCClient_GetAudioInputDevices(ATCClientHandle handle, unsigned int mAudioApi, AudioInterfaceNativeCallback callback) {
    auto values     = handle->impl->GetAudioInputDevicesNative(mAudioApi);
    auto origvalues = values;
    for (auto *c = *values; c; c = *++values) {
        callback(c->id, c->name, c->isDefault);
    }
    handle->impl->FreeAudioDevices(origvalues);
}

AFV_NATIVE_API const char *ATCClient_GetDefaultAudioInputDevice(ATCClientHandle handle, unsigned int mAudioApi) {
    return handle->impl->GetDefaultAudioInputDeviceNative(mAudioApi);
}

AFV_NATIVE_API void ATCClient_SetAudioOutputDevice(ATCClientHandle handle, char *outputDevice) {
    handle->impl->SetAudioOutputDevice(outputDevice);
}

AFV_NATIVE_API void ATCClient_SetAudioSpeakersOutputDevice(ATCClientHandle handle, char *outputDevice) {
    handle->impl->SetAudioSpeakersOutputDevice(outputDevice);
}

AFV_NATIVE_API void ATCClient_GetAudioOutputDevices(ATCClientHandle handle, unsigned int mAudioApi, AudioInterfaceNativeCallback callback) {
    auto values     = handle->impl->GetAudioOutputDevicesNative(mAudioApi);
    auto origvalues = values;
    for (auto *c = *values; c; c = *++values) {
        callback(c->id, c->name, c->isDefault);
    }
    handle->impl->FreeAudioDevices(origvalues);
}

AFV_NATIVE_API const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientHandle handle, unsigned int mAudioApi) {
    return handle->impl->GetDefaultAudioOutputDeviceNative(mAudioApi);
}

AFV_NATIVE_API void ATCClient_FreeAudioDevices(ATCClientHandle handle, afv_native::api::AudioInterfaceNative **in) {
    handle->impl->FreeAudioDevices(in);
}

AFV_NATIVE_API const double ATCClient_GetInputPeak(ATCClientHandle handle) {
    return handle->impl->GetInputPeak();
}

AFV_NATIVE_API const double ATCClient_GetInputVu(ATCClientHandle handle) {
    return handle->impl->GetInputVu();
}

AFV_NATIVE_API void ATCClient_SetEnableInputFilters(ATCClientHandle handle, bool enableInputFilters) {
    handle->impl->SetEnableInputFilters(enableInputFilters);
}

AFV_NATIVE_API void ATCClient_SetEnableOutputEffects(ATCClientHandle handle, bool enableEffects) {
    handle->impl->SetEnableOutputEffects(enableEffects);
}

AFV_NATIVE_API bool ATCClient_GetEnableInputFilters(ATCClientHandle handle) {
    return handle->impl->GetEnableInputFilters();
}

AFV_NATIVE_API void ATCClient_StartAudio(ATCClientHandle handle) {
    handle->impl->StartAudio();
}

AFV_NATIVE_API void ATCClient_StopAudio(ATCClientHandle handle) {
    handle->impl->StopAudio();
}

AFV_NATIVE_API bool ATCClient_IsAudioRunning(ATCClientHandle handle) {
    return handle->impl->IsAudioRunning();
}

AFV_NATIVE_API void ATCClient_SetTx(ATCClientHandle handle, unsigned int freq, bool active) {
    handle->impl->SetTx(freq, active);
}

AFV_NATIVE_API void ATCClient_SetRx(ATCClientHandle handle, unsigned int freq, bool active) {
    handle->impl->SetRx(freq, active);
}

AFV_NATIVE_API void ATCClient_SetXc(ATCClientHandle handle, unsigned int freq, bool active) {
    handle->impl->SetXc(freq, active);
}

AFV_NATIVE_API void ATCClient_SetCrossCoupleAcross(ATCClientHandle handle, unsigned int freq, bool active) {
    handle->impl->SetCrossCoupleAcross(freq, active);
}

AFV_NATIVE_API void ATCClient_SetOnHeadset(ATCClientHandle handle, unsigned int freq, bool active) {
    handle->impl->SetOnHeadset(freq, active);
}

AFV_NATIVE_API bool ATCClient_GetTxActive(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetTxActive(freq);
}

AFV_NATIVE_API bool ATCClient_GetRxActive(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetRxActive(freq);
}

AFV_NATIVE_API bool ATCClient_GetOnHeadset(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetOnHeadset(freq);
}

AFV_NATIVE_API bool ATCClient_GetTxState(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetTxState(freq);
}

AFV_NATIVE_API bool ATCClient_GetRxState(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetRxState(freq);
}

AFV_NATIVE_API bool ATCClient_GetXcState(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetXcState(freq);
}

AFV_NATIVE_API bool ATCClient_GetCrossCoupleAcrossState(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetCrossCoupleAcrossState(freq);
}

AFV_NATIVE_API void ATCClient_UseTransceiversFromStation(ATCClientHandle handle, char *station, unsigned int freq) {
    handle->impl->UseTransceiversFromStation(station, freq);
}

AFV_NATIVE_API void ATCClient_FetchTransceiverInfo(ATCClientHandle handle, char *station) {
    handle->impl->FetchTransceiverInfo(station);
}

AFV_NATIVE_API void ATCClient_FetchStationVccs(ATCClientHandle handle, char *station) {
    handle->impl->FetchStationVccs(station);
}

AFV_NATIVE_API void ATCClient_GetStation(ATCClientHandle handle, char *station) {
    handle->impl->GetStation(station);
}

AFV_NATIVE_API int ATCClient_GetTransceiverCountForStation(ATCClientHandle handle, char *station) {
    return handle->impl->GetTransceiverCountForStation(station);
}

AFV_NATIVE_API int ATCClient_GetTransceiverCountForFrequency(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetTransceiverCountForFrequency(freq);
}

AFV_NATIVE_API void ATCClient_SetPtt(ATCClientHandle handle, bool pttState) {
    handle->impl->SetPtt(pttState);
}

AFV_NATIVE_API const char *ATCClient_LastTransmitOnFreq(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->LastTransmitOnFreqNative(freq);
}

AFV_NATIVE_API void ATCClient_SetRadioGainAll(ATCClientHandle handle, float gain) {
    handle->impl->SetRadioGainAll(gain);
}

AFV_NATIVE_API void ATCClient_SetRadioGain(ATCClientHandle handle, unsigned int freq, float gain) {
    handle->impl->SetRadioGain(freq, gain);
}

AFV_NATIVE_API void ATCClient_SetPlaybackChannelAll(ATCClientHandle handle, afv_native::PlaybackChannel channel) {
    handle->impl->SetPlaybackChannelAll(channel);
}

AFV_NATIVE_API void ATCClient_SetPlaybackChannel(ATCClientHandle handle, unsigned int freq, afv_native::PlaybackChannel channel) {
    handle->impl->SetPlaybackChannel(freq, channel);
}

AFV_NATIVE_API int ATCClient_GetPlaybackChannel(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->GetPlaybackChannel(freq);
}

AFV_NATIVE_API bool ATCClient_AddFrequency(ATCClientHandle handle, unsigned int freq, char *stationName) {
    return handle->impl->AddFrequency(freq, stationName);
}

AFV_NATIVE_API void ATCClient_RemoveFrequency(ATCClientHandle handle, unsigned int freq) {
    handle->impl->RemoveFrequency(freq);
}

AFV_NATIVE_API bool ATCClient_IsFrequencyActive(ATCClientHandle handle, unsigned int freq) {
    return handle->impl->IsFrequencyActive(freq);
}

// afv_native::SimpleAtcRadioState **ATCClient_GetRadioState(ATCClientHandle handle) {
//
//     handle->impl->getRadioStateNative();
// }

// void ATCClient_FreeRadioState(ATCClientHandle handle, afv_native::SimpleAtcRadioState **state) {
//
//     out->FreeRadioState(state);
// }

AFV_NATIVE_API void ATCClient_Reset(ATCClientHandle handle) {
    handle->impl->reset();
}

AFV_NATIVE_API void ATCClient_FreeString(ATCClientHandle handle, char *in) {
    handle->impl->FreeString(in);
}

AFV_NATIVE_API void ATCClient_SetHardware(ATCClientHandle handle, afv_native::HardwareType hardware) {
    handle->impl->SetHardware(hardware);
}

AFV_NATIVE_API void ATCClient_RaiseClientEvent(ATCClientHandle handle, void (*callback)(afv_native::ClientEventType, void *, void *)) {
    handle->impl->RaiseClientEvent(handle, callback);
}

AFV_NATIVE_API void ATCClient_SetTransceivers(ATCClientHandle handle, unsigned int freq, int count, StationTransceiverFlat_t transceivers[]) {
    auto trans = new std::vector<afv_native::afv::dto::StationTransceiver>;
    for (int j = 0; j < count; j++) {
        afv_native::afv::dto::StationTransceiver t;
        t.ID         = std::string(transceivers[j].ID);
        t.Name       = std::string(transceivers[j].Name);
        t.LatDeg     = transceivers[j].LatDeg;
        t.LonDeg     = transceivers[j].LonDeg;
        t.HeightMslM = transceivers[j].HeightMslM;
        t.HeightAglM = transceivers[j].HeightAglM;
        trans->push_back(t);
    }
    handle->impl->SetManualTransceivers(freq, *trans);
    delete trans;
}
