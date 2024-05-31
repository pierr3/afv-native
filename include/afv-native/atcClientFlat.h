#ifndef atcClientFlat_h
#define atcClientFlat_h

#include "afv-native/atcClient.h"
#include "afv-native/atcClientWrapper.h"

typedef struct ATCClientHandle_ *ATCClientHandle;

typedef void (*CharStarCallback)(const char *);
typedef void (*AudioApisCallback)(unsigned int id, const char *);
typedef void (*AudioInterfaceNativeCallback)(char *id, char *name, bool isDefault);

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

extern "C" {

    AFV_NATIVE_API ATCClientHandle ATCClient_Create(char *clientName, char *resourcePath, char *baseURL);
    AFV_NATIVE_API void ATCClient_Destroy(ATCClientHandle handle);
    AFV_NATIVE_API bool ATCClient_IsInitialized(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_SetCredentials(ATCClientHandle handle, char *username, char *password);
    AFV_NATIVE_API void ATCClient_SetCallsign(ATCClientHandle handle, char *callsign);
    AFV_NATIVE_API void ATCClient_SetClientPosition(ATCClientHandle handle, double lat, double lon, double amslm, double aglm);
    AFV_NATIVE_API bool ATCClient_IsVoiceConnected(ATCClientHandle handle);
    AFV_NATIVE_API bool ATCClient_IsAPIConnected(ATCClientHandle handle);
    AFV_NATIVE_API bool ATCClient_Connect(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_Disconnect(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_SetAudioApi(ATCClientHandle handle, unsigned int api);
    AFV_NATIVE_API void ATCClient_GetAudioApis(ATCClientHandle handle, AudioApisCallback callback);
    AFV_NATIVE_API void ATCClient_FreeAudioApis(ATCClientHandle handle, char **apis);
    AFV_NATIVE_API void ATCClient_SetAudioInputDevice(ATCClientHandle handle, char *inputDevice);
    AFV_NATIVE_API void ATCClient_GetAudioInputDevices(ATCClientHandle handle, unsigned int mAudioApi, AudioInterfaceNativeCallback callback);
    AFV_NATIVE_API const char *ATCClient_GetDefaultAudioInputDevice(ATCClientHandle handle, unsigned int mAudioApi);
    AFV_NATIVE_API void ATCClient_SetAudioOutputDevice(ATCClientHandle handle, char *outputDevice);
    AFV_NATIVE_API void ATCClient_SetAudioSpeakersOutputDevice(ATCClientHandle handle, char *outputDevice);
    AFV_NATIVE_API void ATCClient_GetAudioOutputDevices(ATCClientHandle handle, unsigned int mAudioApi, AudioInterfaceNativeCallback callback);
    AFV_NATIVE_API const char *ATCClient_GetDefaultAudioOutputDevice(ATCClientHandle handle, unsigned int mAudioApi);
    AFV_NATIVE_API void ATCClient_FreeAudioDevices(ATCClientHandle handle, afv_native::api::AudioInterfaceNative **in);
    AFV_NATIVE_API const double ATCClient_GetInputPeak(ATCClientHandle handle);
    AFV_NATIVE_API const double ATCClient_GetInputVu(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_SetEnableInputFilters(ATCClientHandle handle, bool enableInputFilters);
    AFV_NATIVE_API void ATCClient_SetEnableOutputEffects(ATCClientHandle handle, bool enableEffects);
    AFV_NATIVE_API bool ATCClient_GetEnableInputFilters(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_StartAudio(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_StopAudio(ATCClientHandle handle);
    AFV_NATIVE_API bool ATCClient_IsAudioRunning(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_SetTx(ATCClientHandle handle, unsigned int freq, bool active);
    AFV_NATIVE_API void ATCClient_SetRx(ATCClientHandle handle, unsigned int freq, bool active);
    AFV_NATIVE_API void ATCClient_SetXc(ATCClientHandle handle, unsigned int freq, bool active);
    AFV_NATIVE_API void ATCClient_SetCrossCoupleAcross(ATCClientHandle handle, unsigned int freq, bool active);
    AFV_NATIVE_API void ATCClient_SetOnHeadset(ATCClientHandle handle, unsigned int freq, bool active);
    AFV_NATIVE_API bool ATCClient_GetTxActive(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetRxActive(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetOnHeadset(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetTxState(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetRxState(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetXcState(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_GetCrossCoupleAcrossState(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API void ATCClient_UseTransceiversFromStation(ATCClientHandle handle, char *station, unsigned int freq);
    AFV_NATIVE_API void ATCClient_FetchTransceiverInfo(ATCClientHandle handle, char *station);
    AFV_NATIVE_API void ATCClient_FetchStationVccs(ATCClientHandle handle, char *station);
    AFV_NATIVE_API void ATCClient_GetStation(ATCClientHandle handle, char *station);
    AFV_NATIVE_API int ATCClient_GetTransceiverCountForStation(ATCClientHandle handle, char *station);
    AFV_NATIVE_API int ATCClient_GetTransceiverCountForFrequency(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API void ATCClient_SetPtt(ATCClientHandle handle, bool pttState);
    AFV_NATIVE_API const char *ATCClient_LastTransmitOnFreq(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API void ATCClient_SetRadioGainAll(ATCClientHandle handle, float gain);
    AFV_NATIVE_API void ATCClient_SetRadioGain(ATCClientHandle handle, unsigned int freq, float gain);
    AFV_NATIVE_API void ATCClient_SetPlaybackChannelAll(ATCClientHandle handle, afv_native::PlaybackChannel channel);
    AFV_NATIVE_API void ATCClient_SetPlaybackChannel(ATCClientHandle handle, unsigned int freq, afv_native::PlaybackChannel channel);
    AFV_NATIVE_API int ATCClient_GetPlaybackChannel(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_AddFrequency(ATCClientHandle handle, unsigned int freq, char *stationName);
    AFV_NATIVE_API void ATCClient_RemoveFrequency(ATCClientHandle handle, unsigned int freq);
    AFV_NATIVE_API bool ATCClient_IsFrequencyActive(ATCClientHandle handle, unsigned int freq);
    // afv_native::SimpleAtcRadioState **ATCClient_GetRadioState(ATCClientHandle handle);
    // void ATCClient_FreeRadioState(ATCClientHandle handle, afv_native::SimpleAtcRadioState **state);
    AFV_NATIVE_API void ATCClient_Reset(ATCClientHandle handle);
    AFV_NATIVE_API void ATCClient_FreeString(ATCClientHandle handle, char *in);
    AFV_NATIVE_API void ATCClient_SetHardware(ATCClientHandle handle, afv_native::HardwareType hardware);
    AFV_NATIVE_API void ATCClient_RaiseClientEvent(ATCClientHandle handle, void (*callback)(afv_native::ClientEventType, void *, void *));
}

#endif
