#include "afv-native/atcClientFlat.h"
#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

AFV_NATIVE_API ATCClientType ATCClient_Create(char *clientName, char *resourcePath, char *baseURL) {
    auto out = new afv_native::api::atcClient(clientName, resourcePath, baseURL);
    return (void *) out;
}

AFV_NATIVE_API void ATCClient_Destroy(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    delete out;
}

AFV_NATIVE_API bool ATCClient_IsInitialized(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsInitialized();
}

AFV_NATIVE_API void ATCClient_SetCredentials(ATCClientType handle, char *username, char *password) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCredentials(username, password);
}

AFV_NATIVE_API void ATCClient_SetCallsign(ATCClientType handle, char *callsign) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCallsign(callsign);
}

AFV_NATIVE_API void ATCClient_SetClientPosition(ATCClientType handle, double lat, double lon, double amslm, double aglm) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetClientPosition(lat, lon, amslm, aglm);
}

AFV_NATIVE_API bool ATCClient_IsVoiceConnected(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsVoiceConnected();
}

AFV_NATIVE_API bool ATCClient_IsAPIConnected(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsAPIConnected();
}

AFV_NATIVE_API bool ATCClient_Connect(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->Connect();
}

AFV_NATIVE_API void ATCClient_Disconnect(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->Disconnect();
}

AFV_NATIVE_API void ATCClient_SetAudioApi(ATCClientType handle, unsigned int api) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioApi(api);
}

AFV_NATIVE_API const char **ATCClient_GetAudioApis(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioApisNative();
}

AFV_NATIVE_API void ATCClient_FreeAudioApis(ATCClientType handle, char **apis) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeAudioApis(apis);
}

AFV_NATIVE_API void ATCClient_SetAudioInputDevice(ATCClientType handle, char *inputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioInputDevice(inputDevice);
}

AFV_NATIVE_API afv_native::api::AudioInterfaceNative **ATCClient_GetAudioInputDevices(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioInputDevicesNative(mAudioApi);
}

AFV_NATIVE_API AFV_NATIVE_API const char *ATCClient_GetDefaultAudioInputDevice(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetDefaultAudioInputDeviceNative(mAudioApi);
}

AFV_NATIVE_API void ATCClient_SetAudioOutputDevice(ATCClientType handle, char *outputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioOutputDevice(outputDevice);
}

AFV_NATIVE_API void ATCClient_SetAudioSpeakersOutputDevice(ATCClientType handle, char *outputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioSpeakersOutputDevice(outputDevice);
}

AFV_NATIVE_API afv_native::api::AudioInterfaceNative **ATCClient_GetAudioOutputDevices(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioOutputDevicesNative(mAudioApi);
}

AFV_NATIVE_API const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetDefaultAudioOutputDeviceNative(mAudioApi);
}

AFV_NATIVE_API void ATCClient_FreeAudioDevices(ATCClientType handle, afv_native::api::AudioInterfaceNative **in) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeAudioDevices(in);
}

AFV_NATIVE_API const double ATCClient_GetInputPeak(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetInputPeak();
}

AFV_NATIVE_API const double ATCClient_GetInputVu(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetInputVu();
}

AFV_NATIVE_API void ATCClient_SetEnableInputFilters(ATCClientType handle, bool enableInputFilters) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetEnableInputFilters(enableInputFilters);
}

AFV_NATIVE_API void ATCClient_SetEnableOutputEffects(ATCClientType handle, bool enableEffects) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetEnableOutputEffects(enableEffects);
}

AFV_NATIVE_API bool ATCClient_GetEnableInputFilters(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetEnableInputFilters();
}

AFV_NATIVE_API void ATCClient_StartAudio(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->StartAudio();
}

AFV_NATIVE_API void ATCClient_StopAudio(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->StopAudio();
}

AFV_NATIVE_API bool ATCClient_IsAudioRunning(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsAudioRunning();
}

AFV_NATIVE_API void ATCClient_SetTx(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetTx(freq, active);
}

AFV_NATIVE_API void ATCClient_SetRx(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRx(freq, active);
}

AFV_NATIVE_API void ATCClient_SetXc(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetXc(freq, active);
}

AFV_NATIVE_API void ATCClient_SetCrossCoupleAcross(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCrossCoupleAcross(freq, active);
}

AFV_NATIVE_API void ATCClient_SetOnHeadset(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetOnHeadset(freq, active);
}

AFV_NATIVE_API bool ATCClient_GetTxActive(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTxActive(freq);
}

AFV_NATIVE_API bool ATCClient_GetRxActive(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetRxActive(freq);
}

AFV_NATIVE_API bool ATCClient_GetOnHeadset(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetOnHeadset(freq);
}

AFV_NATIVE_API bool ATCClient_GetTxState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTxState(freq);
}

AFV_NATIVE_API bool ATCClient_GetRxState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetRxState(freq);
}

AFV_NATIVE_API bool ATCClient_GetXcState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetXcState(freq);
}

AFV_NATIVE_API bool ATCClient_GetCrossCoupleAcrossState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetCrossCoupleAcrossState(freq);
}

AFV_NATIVE_API void ATCClient_UseTransceiversFromStation(ATCClientType handle, char *station, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->UseTransceiversFromStation(station, freq);
}

AFV_NATIVE_API void ATCClient_FetchTransceiverInfo(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FetchTransceiverInfo(station);
}

AFV_NATIVE_API void ATCClient_FetchStationVccs(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FetchStationVccs(station);
}

AFV_NATIVE_API void ATCClient_GetStation(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->GetStation(station);
}

AFV_NATIVE_API int ATCClient_GetTransceiverCountForStation(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTransceiverCountForStation(station);
}

AFV_NATIVE_API int ATCClient_GetTransceiverCountForFrequency(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTransceiverCountForFrequency(freq);
}

AFV_NATIVE_API void ATCClient_SetPtt(ATCClientType handle, bool pttState) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPtt(pttState);
}

AFV_NATIVE_API const char *ATCClient_LastTransmitOnFreq(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->LastTransmitOnFreqNative(freq);
}

AFV_NATIVE_API void ATCClient_SetRadioGainAll(ATCClientType handle, float gain) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRadioGainAll(gain);
}

AFV_NATIVE_API void ATCClient_SetRadioGain(ATCClientType handle, unsigned int freq, float gain) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRadioGain(freq, gain);
}

AFV_NATIVE_API AFV_NATIVE_API void ATCClient_SetPlaybackChannelAll(ATCClientType handle, afv_native::PlaybackChannel channel) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPlaybackChannelAll(channel);
}

AFV_NATIVE_API void ATCClient_SetPlaybackChannel(ATCClientType handle, unsigned int freq, afv_native::PlaybackChannel channel) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPlaybackChannel(freq, channel);
}

AFV_NATIVE_API int ATCClient_GetPlaybackChannel(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetPlaybackChannel(freq);
}

AFV_NATIVE_API bool ATCClient_AddFrequency(ATCClientType handle, unsigned int freq, char *stationName) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->AddFrequency(freq, stationName);
}

AFV_NATIVE_API void ATCClient_RemoveFrequency(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->RemoveFrequency(freq);
}

AFV_NATIVE_API bool ATCClient_IsFrequencyActive(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsFrequencyActive(freq);
}

// afv_native::SimpleAtcRadioState **ATCClient_GetRadioState(ATCClientType handle) {
//     afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
//     return out->getRadioStateNative();
// }

// void ATCClient_FreeRadioState(ATCClientType handle, afv_native::SimpleAtcRadioState **state) {
//     afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
//     out->FreeRadioState(state);
// }

AFV_NATIVE_API void ATCClient_Reset(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->reset();
}

AFV_NATIVE_API void ATCClient_FreeString(ATCClientType handle, char *in) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeString(in);
}

AFV_NATIVE_API void ATCClient_SetHardware(ATCClientType handle, afv_native::HardwareType hardware) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetHardware(hardware);
}

AFV_NATIVE_API void ATCClient_RaiseClientEvent(ATCClientType handle, void (*callback)(afv_native::ClientEventType, void *, void *)) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->RaiseClientEvent(callback);
}
