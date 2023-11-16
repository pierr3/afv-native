#include "MiniAudioAudioDevice.h"

#include <algorithm>
#include <memory>
#include <cstring>

using namespace afv_native::audio;
using namespace std;

void logger(void* pUserData, ma_uint32 logLevel, const char* message)
{
    (void)pUserData;
    std::string msg(message);
    msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.cend());
    LOG("MiniAudioAudioDevice", "%s: %s", ma_log_level_to_string(logLevel), msg.c_str());
}

MiniAudioAudioDevice::MiniAudioAudioDevice(
        const std::string& userStreamName,
        const std::string& outputDeviceName,
        const std::string& inputDeviceName,
        AudioDevice::Api audioApi,
        bool splitChannels) :
    AudioDevice(),
    mUserStreamName(userStreamName),
    mOutputDeviceName(outputDeviceName),
    mInputDeviceName(inputDeviceName),
    mInputInitialized(false),
    mOutputInitialized(false),
    mSplitChannels(splitChannels)
{
    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.threadPriority = ma_thread_priority_normal;
    contextConfig.jack.pClientName = "xpilot";
    contextConfig.pulse.pApplicationName = "xpilot";
    ma_result result = ma_context_init(NULL, 0, &contextConfig, &context);
    if(result == MA_SUCCESS) {
        ma_log_register_callback(ma_context_get_log(&context), ma_log_callback_init(logger, NULL));
        LOG("MiniAudioAudioDevice", "Context initialized. Audio Backend: %s", ma_get_backend_name(context.backend));
    }
    else {
        LOG("MiniAudioAudioDevice", "Error initializing context: %s", ma_result_description(result));
    }
}

MiniAudioAudioDevice::~MiniAudioAudioDevice()
{

}

bool MiniAudioAudioDevice::openOutput()
{
    return initOutput();
}

bool MiniAudioAudioDevice::openInput()
{
    return initInput();
}

void MiniAudioAudioDevice::close()
{
    if(mInputInitialized)
        ma_device_uninit(&inputDev);

    if(mOutputInitialized)
        ma_device_uninit(&outputDev);

    ma_context_uninit(&context);

    mInputInitialized = false;
    mOutputInitialized = false;
}

std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleInputDevices()
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    ma_result result = ma_context_init(NULL, 0, NULL, &context);
    if(result == MA_SUCCESS) {
        result = ma_context_get_devices(&context, NULL, NULL, &devices, &deviceCount);
        if(result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice", "Successfully queried %d input devices", deviceCount);
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);

                // log detailed device info
                {
                    ma_device_info detailedDeviceInfo;
                    result = ma_context_get_device_info(&context, ma_device_type_capture, &devices[i].id, &detailedDeviceInfo);
                    if(result == MA_SUCCESS)
                    {
                        LOG("MiniAudioAudioDevice", "Input: %s (Default: %s, Format Count: %d)",
                            devices[i].name,
                            detailedDeviceInfo.isDefault ? "Yes" : "No",
                            detailedDeviceInfo.nativeDataFormatCount);

                        ma_uint32 iFormat;
                        for(iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat)
                        {
                            LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                                ma_get_format_name(detailedDeviceInfo.nativeDataFormats[iFormat].format),
                                detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                                detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
                        }
                    }
                    else {
                        LOG("MiniAudioAudioDevice", "Error getting input device info: %s", ma_result_description(result));
                    }
                }
            }
        }
        else {
            LOG("MiniAudioAudioDevice", "Error querying input devices: %s", ma_result_description(result));
        }
    }
    else {
        LOG("MiniAudioAudioDevice", "Error initializing input device context: %s", ma_result_description(result));
    }

    ma_context_uninit(&context);

    return deviceList;
}

std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleOutputDevices()
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    ma_result result = ma_context_init(NULL, 0, NULL, &context);
    if(result == MA_SUCCESS) {
        result = ma_context_get_devices(&context, &devices, &deviceCount, NULL, NULL);
        if(result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice", "Successfully queried %d output devices", deviceCount);
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);

                // log detailed device info
                {
                    ma_device_info detailedDeviceInfo;
                    result = ma_context_get_device_info(&context, ma_device_type_playback, &devices[i].id, &detailedDeviceInfo);
                    if(result == MA_SUCCESS)
                    {
                        LOG("MiniAudioAudioDevice", "Output: %s (Default: %s, Format Count: %d)",
                            devices[i].name,
                            detailedDeviceInfo.isDefault ? "Yes" : "No",
                            detailedDeviceInfo.nativeDataFormatCount);

                        ma_uint32 iFormat;
                        for(iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat)
                        {
                            LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                                ma_get_format_name(detailedDeviceInfo.nativeDataFormats[iFormat].format),
                                detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                                detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
                        }
                    }
                    else {
                        LOG("MiniAudioAudioDevice", "Error getting output device info: %s", ma_result_description(result));
                    }
                }
            }
        }
        else {
            LOG("MiniAudioAudioDevice", "Error querying output devices: %s", ma_result_description(result));
        }
    }
    else {
        LOG("MiniAudioAudioDevice", "Error initializing output device context: %s", ma_result_description(result));
    }

    ma_context_uninit(&context);

    return deviceList;
}

bool MiniAudioAudioDevice::initOutput()
{
    if(mOutputInitialized)
        ma_device_uninit(&outputDev);

    if(mOutputDeviceName.empty()) {
        LOG("MiniAudioAudioDevice::initOutput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id outputDeviceId;
    if(!getDeviceForName(mOutputDeviceName, false, outputDeviceId)) {
        LOG("MiniAudioAudioDevice::initOutput()", "No device found for %s", mOutputDeviceName.c_str());
        return false; // no device found
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &outputDeviceId;
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = mSplitChannels ? 2 : 1;
    cfg.playback.shareMode = ma_share_mode_shared;
    cfg.sampleRate = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData = this;
    cfg.dataCallback = maOutputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &outputDev);
    if(result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing output device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&outputDev);
    if(result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting output device: %s", ma_result_description(result));
        return false;
    }

    mOutputInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::initInput()
{
    if(mInputInitialized)
        ma_device_uninit(&inputDev);

    if(mInputDeviceName.empty()) {
        LOG("MiniAudioAudioDevice::initInput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id inputDeviceId;
    if(!getDeviceForName(mInputDeviceName, true, inputDeviceId)) {
        LOG("MiniAudioAudioDevice::initInput()", "No device found for %s", mInputDeviceName.c_str());
        return false; // no device found
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &inputDeviceId;
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 1;
    cfg.capture.shareMode = ma_share_mode_shared;
    cfg.sampleRate = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData = this;
    cfg.dataCallback = maInputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &inputDev);
    if(result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing input device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&inputDev);
    if(result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting input device: %s", ma_result_description(result));
        return false;
    }

    mInputInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId)
{
    auto allDevices = forInput ? getCompatibleInputDevices() : getCompatibleOutputDevices();

    if(!allDevices.empty()) {
        for(const auto& devicePair : allDevices) {
            if(devicePair.second.name == deviceName) {
                deviceId = devicePair.second.id;
                return true;
            }
        }
    }

    return false;
}

int MiniAudioAudioDevice::outputCallback(void *outputBuffer, unsigned int nFrames)
{
    if (outputBuffer) {
        std::lock_guard<std::mutex> sourceGuard(mSourcePtrLock);
        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            if (mSource) {
                SourceStatus rv;
                rv = mSource->getAudioFrame(reinterpret_cast<float *>(outputBuffer) + i);
                if (rv != SourceStatus::OK) {
                    ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0, frameSizeBytes);
                    mSource.reset();
                }
            } else {
                // if there's no source, but there is an output buffer, zero it to avoid making horrible buzzing sounds.
                ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0, frameSizeBytes);
            }
        }
    }

    return 0;
}

int MiniAudioAudioDevice::inputCallback(const void *inputBuffer, unsigned int nFrames)
{
    std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
    if (mSink && inputBuffer) {
        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            mSink->putAudioFrame(reinterpret_cast<const float *>(inputBuffer) + i);
        }
    }

    return 0;
}

void MiniAudioAudioDevice::maOutputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    auto device = reinterpret_cast<MiniAudioAudioDevice*>(pDevice->pUserData);
    device->outputCallback(pOutput, frameCount);
}

void MiniAudioAudioDevice::maInputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    auto device = reinterpret_cast<MiniAudioAudioDevice*>(pDevice->pUserData);
    device->inputCallback(pInput, frameCount);
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
    return {};
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleInputDevices();
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
    }
    return returnDevices;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleOutputDevices();
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
    }
    return returnDevices;
}

std::shared_ptr<AudioDevice>
AudioDevice::makeDevice(
        const std::string &userStreamName,
        const std::string &outputDeviceId,
        const std::string &inputDeviceId,
        AudioDevice::Api audioApi,
        bool splitChannels) {
    auto devsp = std::make_shared<MiniAudioAudioDevice>(userStreamName, outputDeviceId, inputDeviceId, audioApi, splitChannels);
    return devsp;
}
