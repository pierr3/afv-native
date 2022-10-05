#ifndef MINIAUDIO_DEVICE_H
#define MINIAUDIO_DEVICE_H

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_WEBAUDIO
#define MA_NO_NULL
#include "miniaudio.h"

#include "afv-native/audio/AudioDevice.h"

#include <map>

namespace afv_native
{
    namespace audio
    {
        class MiniAudioDevice : public AudioDevice
        {
        public:
            explicit MiniAudioDevice(
                    const std::string& userStreamName,
                    const std::string& outputDeviceName,
                    const std::string& inputDeviceName,
                    Api audioApi);
            virtual ~MiniAudioDevice();

            bool openOutput() override;
            bool openInput() override;
            void close() override;

            static std::map<int, ma_device_info> getCompatibleInputDevices();
            static std::map<int, ma_device_info> getCompatibleOutputDevices();

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
        };
    }
}

#endif // !MINIAUDIO_DEVICE_H
