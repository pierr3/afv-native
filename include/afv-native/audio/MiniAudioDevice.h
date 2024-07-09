#ifndef MINIAUDIO_DEVICE_H
#define MINIAUDIO_DEVICE_H

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_WEBAUDIO
#define MA_NO_NULL
#define MA_NO_CUSTOM
#define MA_NO_DSOUND
#include "afv-native/Log.h"
#include "afv-native/audio/AudioDevice.h"
#include "miniaudio.h"
#include <cstring>
#include <map>
#include <memory>
#include <string>

#ifdef _WIN32
    #include "windows.h"
#endif

namespace afv_native { namespace audio {
    class MiniAudioAudioDevice: public AudioDevice {
      public:
        explicit MiniAudioAudioDevice(const std::string &userStreamName, const std::string &outputDeviceName, const std::string &inputDeviceName, Api audioApi, bool makeStereo = false);
        virtual ~MiniAudioAudioDevice();

        bool openOutput() override;
        bool openInput() override;
        void close() override;

        static std::map<int, ma_device_info> getCompatibleInputDevices(unsigned int api);
        static std::map<int, ma_device_info> getCompatibleOutputDevices(unsigned int api);
        static std::map<unsigned int, std::string> getAvailableBackends();
        static std::string getDeviceId(const ma_device_id &deviceId, const AudioDevice::Api &api, const std::string &deviceName);

      private:
        bool initOutput();
        bool initInput();
        bool getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId);
        bool getDeviceForId(const std::string &inDeviceId, bool forInput, ma_device_id &deviceId);
        static void maOutputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);
        static void maInputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);
        static void maNotificationCallback(const ma_device_notification *pNotification);
        int  outputCallback(void *outputBuffer, unsigned int nFrames);
        int  inputCallback(const void *inputBuffer, unsigned int nFrames);
        void notificationCallback(const ma_device_notification *pNotification);

      private:
        std::string  mUserStreamName;
        std::string  mOutputDeviceId;
        std::string  mInputDeviceId;
        bool         mOutputInitialized;
        bool         mInputInitialized;
        ma_context   context;
        ma_device    outputDev;
        ma_device    inputDev;
        bool         mStereo = false;
        unsigned int mAudioApi;
        bool         mHasClosedManually = false;
    };
}} // namespace afv_native::audio

#endif // !MINIAUDIO_DEVICE_H
