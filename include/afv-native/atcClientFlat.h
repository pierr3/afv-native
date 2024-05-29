#ifndef atcClientFlat_h
#define atcClientFlat_h

#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

typedef void *ATCClientType;

ATCClientType ATCClient_Create(char *clientName, char *resourcePath);
void          ATCClient_Destroy(ATCClientType handle);
bool          ATCClient_IsInitialized(ATCClientType handle);
void ATCClient_SetCredentials(ATCClientType handle, char *username, char *password);
void ATCClient_SetCallsign(ATCClientType handle, char *callsign);
void ATCClient_SetClientPosition(ATCClientType handle, double lat, double lon, double amslm, double aglm);
bool         ATCClient_IsVoiceConnected(ATCClientType handle);
bool         ATCClient_IsAPIConnected(ATCClientType handle);
bool         ATCClient_Connect(ATCClientType handle);
void         ATCClient_Disconnect(ATCClientType handle);
void         ATCClient_SetAudioApi(ATCClientType handle, unsigned int api);
const char **ATCClient_GetAudioApis(ATCClientType handle);
void         ATCClient_FreeAudioApis(ATCClientType handle, char **apis);
void ATCClient_SetAudioInputDevice(ATCClientType handle, char *inputDevice);
afv_native::api::AudioInterface **ATCClient_GetAudioInputDevices(ATCClientType handle, unsigned int mAudioApi);
const char *ATCClient_GetDefaultAudioInputDevice(ATCClientType handle, unsigned int mAudioApi);
void ATCClient_SetAudioOutputDevice(ATCClientType handle, char *outputDevice);
void ATCClient_SetAudioSpeakersOutputDevice(ATCClientType handle, char *outputDevice);
afv_native::api::AudioInterface **ATCClient_GetAudioOutputDevices(ATCClientType handle, unsigned int mAudioApi);
const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientType handle, unsigned int mAudioApi);
void ATCClient_FreeAudioDevices(ATCClientType handle, afv_native::api::AudioInterface **in);
const double ATCClient_GetInputPeak(ATCClientType handle);
const double ATCClient_GetInputVu(ATCClientType handle);
void ATCClient_SetEnableInputFilters(ATCClientType handle, bool enableInputFilters);
void ATCClient_SetEnableOutputEffects(ATCClientType handle, bool enableEffects);
bool ATCClient_GetEnableInputFilters(ATCClientType handle);
void ATCClient_StartAudio(ATCClientType handle);
void ATCClient_StopAudio(ATCClientType handle);
bool ATCClient_IsAudioRunning(ATCClientType handle);
void ATCClient_SetTx(ATCClientType handle, unsigned int freq, bool active);
void ATCClient_SetRx(ATCClientType handle, unsigned int freq, bool active);
void ATCClient_SetXc(ATCClientType handle, unsigned int freq, bool active);
void ATCClient_SetCrossCoupleAcross(ATCClientType handle, unsigned int freq, bool active);
void ATCClient_SetOnHeadset(ATCClientType handle, unsigned int freq, bool active);
bool ATCClient_GetTxActive(ATCClientType handle, unsigned int freq);
bool ATCClient_GetRxActive(ATCClientType handle, unsigned int freq);
bool ATCClient_GetOnHeadset(ATCClientType handle, unsigned int freq);
bool ATCClient_GetTxState(ATCClientType handle, unsigned int freq);
bool ATCClient_GetRxState(ATCClientType handle, unsigned int freq);
bool ATCClient_GetXcState(ATCClientType handle, unsigned int freq);
bool ATCClient_GetCrossCoupleAcrossState(ATCClientType handle, unsigned int freq);
void ATCClient_UseTransceiversFromStation(ATCClientType handle, char *station, unsigned int freq);
void ATCClient_FetchTransceiverInfo(ATCClientType handle, char *station);
void ATCClient_FetchStationVccs(ATCClientType handle, char *station);
void ATCClient_GetStation(ATCClientType handle, char *station);
int ATCClient_GetTransceiverCountForStation(ATCClientType handle, char *station);
int ATCClient_GetTransceiverCountForFrequency(ATCClientType handle, unsigned int freq);
void ATCClient_SetPtt(ATCClientType handle, bool pttState);
const char *ATCClient_LastTransmitOnFreq(ATCClientType handle, unsigned int freq);
void ATCClient_SetRadioGainAll(ATCClientType handle, float gain);
void ATCClient_SetRadioGain(ATCClientType handle, unsigned int freq, float gain);
void ATCClient_SetPlaybackChannelAll(ATCClientType handle, afv_native::PlaybackChannel channel);
void ATCClient_SetPlaybackChannel(ATCClientType handle, unsigned int freq, afv_native::PlaybackChannel channel);
int ATCClient_GetPlaybackChannel(ATCClientType handle, unsigned int freq);
bool ATCClient_AddFrequency(ATCClientType handle, unsigned int freq, char *stationName);
void ATCClient_RemoveFrequency(ATCClientType handle, unsigned int freq);
bool ATCClient_IsFrequencyActive(ATCClientType handle, unsigned int freq);
// afv_native::SimpleAtcRadioState **ATCClient_GetRadioState(ATCClientType handle);
// void ATCClient_FreeRadioState(ATCClientType handle, afv_native::SimpleAtcRadioState **state);
void ATCClient_Reset(ATCClientType handle);
void ATCClient_FreeString(ATCClientType handle, char *in);
void ATCClient_SetHardware(ATCClientType handle, afv_native::HardwareType hardware);
void ATCClient_RaiseClientEvent(ATCClientType handle, void (*callback)(afv_native::ClientEventType, void *, void *));

#endif