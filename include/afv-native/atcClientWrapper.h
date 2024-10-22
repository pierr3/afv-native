#pragma once
#include "afv-native/afv/dto/StationTransceiver.h"
#include "afv_native_export.h"
#include "event.h"
#include "hardwareType.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

// --- REMOVE ABOVE BEFORE PUBLISHING ---

namespace afv_native {
    typedef void (*log_fn)(const char *subsystem, const char *file, int line, const char *lineOut);
    typedef std::function<void(std::string subsystem, std::string file, int line, std::string lineOut)> modern_log_fn;

    struct SimpleAtcRadioState {
        bool            tx;
        bool            rx;
        bool            xc;
        bool            crossCoupleAcross;
        bool            onHeadset;
        unsigned int    Frequency;
        std::string     stationName       = "";
        HardwareType    simulatedHardware = HardwareType::Schmid_ED_137B;
        bool            isATIS            = false;
        PlaybackChannel playbackChannel   = PlaybackChannel::Both;

        std::string lastTransmitCallsign = "";
    };
} // namespace afv_native

namespace afv_native::api {
    AFV_NATIVE_API void setLogger(afv_native::modern_log_fn gLogger);

    struct AFV_NATIVE_API AudioInterface {
        std::string id;
        std::string name;
        bool        isDefault;
    };

    struct AFV_NATIVE_API AudioInterfaceNative {
        char *id;
        char *name;
        bool  isDefault;
    };

    class atcClient {
      public:
        AFV_NATIVE_API atcClient(std::string clientName, std::string resourcePath = "", std::string baseURL = "https://voice1.vatsim.net");
        AFV_NATIVE_API atcClient(char *clientName, char *resourcePath, char *baseURL);
        AFV_NATIVE_API ~atcClient();

        AFV_NATIVE_API bool IsInitialized();

        AFV_NATIVE_API void SetCredentials(std::string username, std::string password);
        AFV_NATIVE_API void SetCredentials(char *username, char *password);

        AFV_NATIVE_API void SetCallsign(std::string callsign);
        AFV_NATIVE_API void SetCallsign(char *callsign);

        AFV_NATIVE_API void SetClientPosition(double lat, double lon, double amslm, double aglm);

        AFV_NATIVE_API bool IsVoiceConnected();
        AFV_NATIVE_API bool IsAPIConnected();

        AFV_NATIVE_API bool Connect();
        AFV_NATIVE_API void Disconnect();

        AFV_NATIVE_API void SetAudioApi(int api);
        AFV_NATIVE_API std::map<int, std::string> GetAudioApis();
        AFV_NATIVE_API const char                        **GetAudioApisNative();
        AFV_NATIVE_API void FreeAudioApis(char **apis);

        AFV_NATIVE_API void SetAudioInputDevice(std::string inputDevice);
        AFV_NATIVE_API void SetAudioInputDevice(char *inputDevice);
        AFV_NATIVE_API std::vector<AudioInterface> GetAudioInputDevices(unsigned int mAudioApi);
        AFV_NATIVE_API AudioInterfaceNative **GetAudioInputDevicesNative(unsigned int mAudioApi);
        AFV_NATIVE_API std::string GetDefaultAudioInputDevice(unsigned int mAudioApi);
        AFV_NATIVE_API const char *GetDefaultAudioInputDeviceNative(unsigned int mAudioApi);

        AFV_NATIVE_API void SetAudioOutputDevice(std::string outputDevice);
        AFV_NATIVE_API void SetAudioOutputDevice(char *outputDevice);
        AFV_NATIVE_API void SetAudioSpeakersOutputDevice(std::string outputDevice);
        AFV_NATIVE_API void SetAudioSpeakersOutputDevice(char *outputDevice);
        AFV_NATIVE_API std::vector<AudioInterface> GetAudioOutputDevices(unsigned int mAudioApi);
        AFV_NATIVE_API AudioInterfaceNative **GetAudioOutputDevicesNative(unsigned int mAudioApi);
        AFV_NATIVE_API std::string GetDefaultAudioOutputDevice(unsigned int mAudioApi);
        AFV_NATIVE_API const char *GetDefaultAudioOutputDeviceNative(unsigned int mAudioApi);
        AFV_NATIVE_API void FreeAudioDevices(AudioInterfaceNative **in);

        AFV_NATIVE_API double GetInputPeak() const;
        AFV_NATIVE_API double GetInputVu() const;

        AFV_NATIVE_API void SetEnableInputFilters(bool enableInputFilters);
        AFV_NATIVE_API void SetEnableOutputEffects(bool enableEffects);
        AFV_NATIVE_API bool GetEnableInputFilters() const;

        AFV_NATIVE_API void StartAudio();
        AFV_NATIVE_API void StopAudio();
        AFV_NATIVE_API bool IsAudioRunning();

        AFV_NATIVE_API void SetTx(unsigned int freq, bool active);
        AFV_NATIVE_API void SetRx(unsigned int freq, bool active);
        AFV_NATIVE_API void SetXc(unsigned int freq, bool active);
        AFV_NATIVE_API void SetCrossCoupleAcross(unsigned int freq, bool active);
        AFV_NATIVE_API void SetOnHeadset(unsigned int freq, bool active);

        AFV_NATIVE_API bool GetTxActive(unsigned int freq);
        AFV_NATIVE_API bool GetRxActive(unsigned int freq);

        AFV_NATIVE_API bool GetOnHeadset(unsigned int freq);

        AFV_NATIVE_API bool GetTxState(unsigned int freq);
        AFV_NATIVE_API bool GetRxState(unsigned int freq);
        AFV_NATIVE_API bool GetXcState(unsigned int freq);
        AFV_NATIVE_API bool GetCrossCoupleAcrossState(unsigned int freq);

        // Use this to set the current transceivers to the transceivers from this station, pulled from the AFV database, only one at a time can be active
        AFV_NATIVE_API void UseTransceiversFromStation(std::string station, unsigned int freq);
        AFV_NATIVE_API void UseTransceiversFromStation(char *station, unsigned int freq);

        AFV_NATIVE_API void UseAllActiveTransceivers(unsigned int freq);
        AFV_NATIVE_API void SetManualTransceivers(unsigned int freq, std::vector<afv_native::afv::dto::StationTransceiver> transceivers);

        AFV_NATIVE_API void FetchTransceiverInfo(std::string station);
        AFV_NATIVE_API void FetchTransceiverInfo(char *station);
        AFV_NATIVE_API void FetchStationVccs(std::string station);
        AFV_NATIVE_API void FetchStationVccs(char *station);
        AFV_NATIVE_API void GetStation(std::string station);
        AFV_NATIVE_API void GetStation(char *station);

        AFV_NATIVE_API int GetTransceiverCountForStation(std::string station);
        AFV_NATIVE_API int GetTransceiverCountForStation(char *station);
        AFV_NATIVE_API int GetTransceiverCountForFrequency(unsigned int freq);

        AFV_NATIVE_API void SetPtt(bool pttState);

        AFV_NATIVE_API void SetAtisRecording(bool state);
        AFV_NATIVE_API bool IsAtisRecording();

        AFV_NATIVE_API void SetAtisListening(bool state);
        AFV_NATIVE_API bool IsAtisListening();

        AFV_NATIVE_API void StartAtisPlayback(std::string callsign, unsigned int freq);
        AFV_NATIVE_API void StartAtisPlayback(char *callsign, unsigned int freq);
        AFV_NATIVE_API void StopAtisPlayback();
        AFV_NATIVE_API bool IsAtisPlayingBack();

        AFV_NATIVE_API std::string LastTransmitOnFreq(unsigned int freq);
        AFV_NATIVE_API const char *LastTransmitOnFreqNative(unsigned int freq);

        AFV_NATIVE_API void SetRadioGainAll(float gain);
        AFV_NATIVE_API void SetRadioGain(unsigned int freq, float gain);

        // Sets the playback channel for all channels and saves it to be used in the future for new channels
        AFV_NATIVE_API void SetPlaybackChannelAll(PlaybackChannel channel);
        // Sets the playback channel for a single channel
        AFV_NATIVE_API void SetPlaybackChannel(unsigned int freq, PlaybackChannel channel);
        AFV_NATIVE_API int GetPlaybackChannel(unsigned int freq);

        AFV_NATIVE_API bool AddFrequency(unsigned int freq, std::string stationName = "");
        AFV_NATIVE_API bool AddFrequency(unsigned int freq, char *stationName);
        AFV_NATIVE_API void RemoveFrequency(unsigned int freq);
        AFV_NATIVE_API bool IsFrequencyActive(unsigned int freq);
        AFV_NATIVE_API std::map<unsigned int, SimpleAtcRadioState> getRadioState();
        AFV_NATIVE_API SimpleAtcRadioState **getRadioStateNative();
        AFV_NATIVE_API void FreeRadioState(SimpleAtcRadioState **state);

        AFV_NATIVE_API void reset();
        AFV_NATIVE_API void FreeString(char *in);

        AFV_NATIVE_API void SetHardware(afv_native::HardwareType hardware);

        AFV_NATIVE_API void RaiseClientEvent(std::function<void(afv_native::ClientEventType, void *, void *)> callback);
        AFV_NATIVE_API void RaiseClientEvent(void *handle, void (*callback)(afv_native::ClientEventType, void *, void *));

        //
        // Deprecated functions
        //
        [[deprecated("Use SetPlaybackChannelAll instead")]] AFV_NATIVE_DEPRECATED void SetHeadsetOutputChannel(int channel);
        [[deprecated("Use SetRadioGainAll instead")]] AFV_NATIVE_DEPRECATED void SetRadiosGain(float gain);
        [[deprecated("Use modern afv_native::api::setLogger() instead")]] AFV_NATIVE_DEPRECATED static void setLogger(afv_native::log_fn gLogger);
    };
} // namespace afv_native::api