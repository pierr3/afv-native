#include "afv-native/atcClientFlat.h"
#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

ATCClientType ATCClient_Create(char *clientName, char *resourcePath, char *baseURL) {
    auto out = new afv_native::api::atcClient(clientName, resourcePath, baseURL);
    return (void *) out;
}

void ATCClient_Destroy(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    delete out;
}

bool ATCClient_IsInitialized(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsInitialized();
}

void ATCClient_SetCredentials(ATCClientType handle, char *username, char *password) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCredentials(username, password);
}

void ATCClient_SetCallsign(ATCClientType handle, char *callsign) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCallsign(callsign);
}

void ATCClient_SetClientPosition(ATCClientType handle, double lat, double lon, double amslm, double aglm) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetClientPosition(lat, lon, amslm, aglm);
}

bool ATCClient_IsVoiceConnected(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsVoiceConnected();
}

bool ATCClient_IsAPIConnected(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsAPIConnected();
}

bool ATCClient_Connect(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->Connect();
}

void ATCClient_Disconnect(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->Disconnect();
}

void ATCClient_SetAudioApi(ATCClientType handle, unsigned int api) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioApi(api);
}

const char **ATCClient_GetAudioApis(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioApisNative();
}

void ATCClient_FreeAudioApis(ATCClientType handle, char **apis) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeAudioApis(apis);
}

void ATCClient_SetAudioInputDevice(ATCClientType handle, char *inputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioInputDevice(inputDevice);
}

afv_native::api::AudioInterfaceNative **ATCClient_GetAudioInputDevices(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioInputDevicesNative(mAudioApi);
}

const char *ATCClient_GetDefaultAudioInputDevice(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetDefaultAudioInputDeviceNative(mAudioApi);
}

void ATCClient_SetAudioOutputDevice(ATCClientType handle, char *outputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioOutputDevice(outputDevice);
}

void ATCClient_SetAudioSpeakersOutputDevice(ATCClientType handle, char *outputDevice) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetAudioSpeakersOutputDevice(outputDevice);
}

afv_native::api::AudioInterfaceNative **ATCClient_GetAudioOutputDevices(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetAudioOutputDevicesNative(mAudioApi);
}

const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientType handle, unsigned int mAudioApi) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetDefaultAudioOutputDeviceNative(mAudioApi);
}

void ATCClient_FreeAudioDevices(ATCClientType handle, afv_native::api::AudioInterfaceNative **in) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeAudioDevices(in);
}

const double ATCClient_GetInputPeak(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetInputPeak();
}

const double ATCClient_GetInputVu(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetInputVu();
}

void ATCClient_SetEnableInputFilters(ATCClientType handle, bool enableInputFilters) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetEnableInputFilters(enableInputFilters);
}

void ATCClient_SetEnableOutputEffects(ATCClientType handle, bool enableEffects) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetEnableOutputEffects(enableEffects);
}

bool ATCClient_GetEnableInputFilters(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetEnableInputFilters();
}

void ATCClient_StartAudio(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->StartAudio();
}

void ATCClient_StopAudio(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->StopAudio();
}

bool ATCClient_IsAudioRunning(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->IsAudioRunning();
}

void ATCClient_SetTx(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetTx(freq, active);
}

void ATCClient_SetRx(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRx(freq, active);
}

void ATCClient_SetXc(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetXc(freq, active);
}

void ATCClient_SetCrossCoupleAcross(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetCrossCoupleAcross(freq, active);
}

void ATCClient_SetOnHeadset(ATCClientType handle, unsigned int freq, bool active) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetOnHeadset(freq, active);
}

bool ATCClient_GetTxActive(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTxActive(freq);
}

bool ATCClient_GetRxActive(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetRxActive(freq);
}

bool ATCClient_GetOnHeadset(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetOnHeadset(freq);
}

bool ATCClient_GetTxState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTxState(freq);
}

bool ATCClient_GetRxState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetRxState(freq);
}

bool ATCClient_GetXcState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetXcState(freq);
}

bool ATCClient_GetCrossCoupleAcrossState(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetCrossCoupleAcrossState(freq);
}

void ATCClient_UseTransceiversFromStation(ATCClientType handle, char *station, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->UseTransceiversFromStation(station, freq);
}

void ATCClient_FetchTransceiverInfo(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FetchTransceiverInfo(station);
}

void ATCClient_FetchStationVccs(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FetchStationVccs(station);
}

void ATCClient_GetStation(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->GetStation(station);
}

int ATCClient_GetTransceiverCountForStation(ATCClientType handle, char *station) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTransceiverCountForStation(station);
}

int ATCClient_GetTransceiverCountForFrequency(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetTransceiverCountForFrequency(freq);
}

void ATCClient_SetPtt(ATCClientType handle, bool pttState) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPtt(pttState);
}

const char *ATCClient_LastTransmitOnFreq(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->LastTransmitOnFreqNative(freq);
}

void ATCClient_SetRadioGainAll(ATCClientType handle, float gain) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRadioGainAll(gain);
}

void ATCClient_SetRadioGain(ATCClientType handle, unsigned int freq, float gain) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetRadioGain(freq, gain);
}

void ATCClient_SetPlaybackChannelAll(ATCClientType handle, afv_native::PlaybackChannel channel) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPlaybackChannelAll(channel);
}

void ATCClient_SetPlaybackChannel(ATCClientType handle, unsigned int freq, afv_native::PlaybackChannel channel) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetPlaybackChannel(freq, channel);
}

int ATCClient_GetPlaybackChannel(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->GetPlaybackChannel(freq);
}

bool ATCClient_AddFrequency(ATCClientType handle, unsigned int freq, char *stationName) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    return out->AddFrequency(freq, stationName);
}

void ATCClient_RemoveFrequency(ATCClientType handle, unsigned int freq) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->RemoveFrequency(freq);
}

bool ATCClient_IsFrequencyActive(ATCClientType handle, unsigned int freq) {
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

void ATCClient_Reset(ATCClientType handle) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->reset();
}

void ATCClient_FreeString(ATCClientType handle, char *in) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->FreeString(in);
}

void ATCClient_SetHardware(ATCClientType handle, afv_native::HardwareType hardware) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->SetHardware(hardware);
}

void ATCClient_RaiseClientEvent(ATCClientType handle, void (*callback)(afv_native::ClientEventType, void *, void *)) {
    afv_native::api::atcClient *out = (afv_native::api::atcClient *) handle;
    out->RaiseClientEvent(callback);
}
