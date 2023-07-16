#ifndef MINIAUDIO_DEVICE_H
#define MINIAUDIO_DEVICE_H

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_WEBAUDIO
#define MA_NO_NULL
#define MA_NO_CUSTOM
#include "miniaudio.h"

#include "afv-native/Log.h"
#include "afv-native/audio/AudioDevice.h"

#include <map>
#include <algorithm>
#include <string>

#ifdef _WIN32
#include "windows.h"
#endif

namespace afv_native
{
    namespace audio
    {
        class MiniAudioAudioDevice : public AudioDevice
        {
        public:
            explicit MiniAudioAudioDevice(
                    const std::string& userStreamName,
                    const std::string& outputDeviceName,
                    const std::string& inputDeviceName,
                    Api audioApi,
                    int outputChannel=0);
            virtual ~MiniAudioAudioDevice();

            bool openOutput() override;
            bool openInput() override;
            void close() override;

            static std::map<int, ma_device_info> getCompatibleInputDevices(unsigned int api);
            static std::map<int, ma_device_info> getCompatibleOutputDevices(unsigned int api);
            static std::map<unsigned int, std::string> getAvailableBackends();

        private:
            bool initOutput();
            bool initInput();
            bool getDeviceForName(const std::string& deviceName, bool forInput, ma_device_id& deviceId);
            static void maOutputCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
            static void maInputCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
            int outputCallback(void* outputBuffer, unsigned int nFrames);
            int inputCallback(const void* inputBuffer, unsigned int nFrames);

        private:
            std::string mUserStreamName;
            std::string mOutputDeviceName;
            std::string mInputDeviceName;
            bool mOutputInitialized;
            bool mInputInitialized;
            ma_context context;
            ma_device outputDev;
            ma_device inputDev;
            unsigned int mAudioApi;
            //
            // The output channel parameter allows for sound playback to be played in the left ear, right ear or both
            // 0: both ears, 1: left ear, 2: right ear
            //
            int mOutputChannel = 0;
        };
    }
}

#endif // !MINIAUDIO_DEVICE_H
