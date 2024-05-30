#ifndef atcClientFlat_h
#define atcClientFlat_h

#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

typedef void *ATCClientType;

/*
struct AFV_NATIVE_API AudioInterfaceNative {
        char *id;
        char *name;
        bool  isDefault;
    };

    enum class HardwareType {
        Schmid_ED_137B,
        Rockwell_Collins_2100,
        Garex_220
    };

    enum class PlaybackChannel {
        Both,
        Left,
        Right
    };


*/

AFV_NATIVE_API ATCClientType ATCClient_Create(char *clientName, char *resourcePath, char *baseURL);
AFV_NATIVE_API void ATCClient_Destroy(ATCClientType handle);
AFV_NATIVE_API bool ATCClient_IsInitialized(ATCClientType handle);
AFV_NATIVE_API void ATCClient_SetCredentials(ATCClientType handle, char *username, char *password);
AFV_NATIVE_API void ATCClient_SetCallsign(ATCClientType handle, char *callsign);
AFV_NATIVE_API void ATCClient_SetClientPosition(ATCClientType handle, double lat, double lon, double amslm, double aglm);
AFV_NATIVE_API bool ATCClient_IsVoiceConnected(ATCClientType handle);
AFV_NATIVE_API bool ATCClient_IsAPIConnected(ATCClientType handle);
AFV_NATIVE_API bool ATCClient_Connect(ATCClientType handle);
AFV_NATIVE_API void ATCClient_Disconnect(ATCClientType handle);
AFV_NATIVE_API void ATCClient_SetAudioApi(ATCClientType handle, unsigned int api);
AFV_NATIVE_API const char **ATCClient_GetAudioApis(ATCClientType handle);
AFV_NATIVE_API void ATCClient_FreeAudioApis(ATCClientType handle, char **apis);
AFV_NATIVE_API void ATCClient_SetAudioInputDevice(ATCClientType handle, char *inputDevice);
AFV_NATIVE_API afv_native::api::AudioInterfaceNative **ATCClient_GetAudioInputDevices(ATCClientType handle, unsigned int mAudioApi);
AFV_NATIVE_API const char *ATCClient_GetDefaultAudioInputDevice(ATCClientType handle, unsigned int mAudioApi);
AFV_NATIVE_API void ATCClient_SetAudioOutputDevice(ATCClientType handle, char *outputDevice);
AFV_NATIVE_API void ATCClient_SetAudioSpeakersOutputDevice(ATCClientType handle, char *outputDevice);
AFV_NATIVE_API afv_native::api::AudioInterfaceNative **ATCClient_GetAudioOutputDevices(ATCClientType handle, unsigned int mAudioApi);
AFV_NATIVE_API const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientType handle, unsigned int mAudioApi);
AFV_NATIVE_API void ATCClient_FreeAudioDevices(ATCClientType handle, afv_native::api::AudioInterfaceNative **in);
AFV_NATIVE_API const double ATCClient_GetInputPeak(ATCClientType handle);
AFV_NATIVE_API const double ATCClient_GetInputVu(ATCClientType handle);
AFV_NATIVE_API void ATCClient_SetEnableInputFilters(ATCClientType handle, bool enableInputFilters);
AFV_NATIVE_API void ATCClient_SetEnableOutputEffects(ATCClientType handle, bool enableEffects);
AFV_NATIVE_API bool ATCClient_GetEnableInputFilters(ATCClientType handle);
AFV_NATIVE_API void ATCClient_StartAudio(ATCClientType handle);
AFV_NATIVE_API void ATCClient_StopAudio(ATCClientType handle);
AFV_NATIVE_API bool ATCClient_IsAudioRunning(ATCClientType handle);
AFV_NATIVE_API void ATCClient_SetTx(ATCClientType handle, unsigned int freq, bool active);
AFV_NATIVE_API void ATCClient_SetRx(ATCClientType handle, unsigned int freq, bool active);
AFV_NATIVE_API void ATCClient_SetXc(ATCClientType handle, unsigned int freq, bool active);
AFV_NATIVE_API void ATCClient_SetCrossCoupleAcross(ATCClientType handle, unsigned int freq, bool active);
AFV_NATIVE_API void ATCClient_SetOnHeadset(ATCClientType handle, unsigned int freq, bool active);
AFV_NATIVE_API bool ATCClient_GetTxActive(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetRxActive(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetOnHeadset(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetTxState(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetRxState(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetXcState(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_GetCrossCoupleAcrossState(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API void ATCClient_UseTransceiversFromStation(ATCClientType handle, char *station, unsigned int freq);
AFV_NATIVE_API void ATCClient_FetchTransceiverInfo(ATCClientType handle, char *station);
AFV_NATIVE_API void ATCClient_FetchStationVccs(ATCClientType handle, char *station);
AFV_NATIVE_API void ATCClient_GetStation(ATCClientType handle, char *station);
AFV_NATIVE_API int ATCClient_GetTransceiverCountForStation(ATCClientType handle, char *station);
AFV_NATIVE_API int ATCClient_GetTransceiverCountForFrequency(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API void ATCClient_SetPtt(ATCClientType handle, bool pttState);
AFV_NATIVE_API const char *ATCClient_LastTransmitOnFreq(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API void ATCClient_SetRadioGainAll(ATCClientType handle, float gain);
AFV_NATIVE_API void ATCClient_SetRadioGain(ATCClientType handle, unsigned int freq, float gain);
AFV_NATIVE_API void ATCClient_SetPlaybackChannelAll(ATCClientType handle, afv_native::PlaybackChannel channel);
AFV_NATIVE_API void ATCClient_SetPlaybackChannel(ATCClientType handle, unsigned int freq, afv_native::PlaybackChannel channel);
AFV_NATIVE_API int ATCClient_GetPlaybackChannel(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_AddFrequency(ATCClientType handle, unsigned int freq, char *stationName);
AFV_NATIVE_API void ATCClient_RemoveFrequency(ATCClientType handle, unsigned int freq);
AFV_NATIVE_API bool ATCClient_IsFrequencyActive(ATCClientType handle, unsigned int freq);
// afv_native::SimpleAtcRadioState **ATCClient_GetRadioState(ATCClientType handle);
// void ATCClient_FreeRadioState(ATCClientType handle, afv_native::SimpleAtcRadioState **state);
AFV_NATIVE_API void ATCClient_Reset(ATCClientType handle);
AFV_NATIVE_API void ATCClient_FreeString(ATCClientType handle, char *in);
AFV_NATIVE_API void ATCClient_SetHardware(ATCClientType handle, afv_native::HardwareType hardware);
AFV_NATIVE_API void ATCClient_RaiseClientEvent(ATCClientType handle, void (*callback)(afv_native::ClientEventType, void *, void *));

#endif