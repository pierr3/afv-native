#include "MiniAudioDevice.h"

#include <QDebug>
#include <memory>
#include <cstring>

using namespace afv_native::audio;
using namespace std;

MiniAudioDevice::MiniAudioDevice(
        const std::string& userStreamName,
        const std::string& outputDeviceName,
        const std::string& inputDeviceName,
        AudioDevice::Api audioApi) :
    AudioDevice(),
    mUserStreamName(userStreamName),
    mOutputDeviceName(outputDeviceName),
    mInputDeviceName(inputDeviceName),
    mInputInitialized(false),
    mOutputInitialized(false)
{
    ma_context_init(NULL, 0, NULL, &context);

    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.threadPriority = ma_thread_priority_normal;
    contextConfig.jack.pClientName = "xpilot";
    contextConfig.pulse.pApplicationName = "xpilot";
}

MiniAudioDevice::~MiniAudioDevice()
{

}

bool MiniAudioDevice::openOutput()
{
    return initOutput();
}

bool MiniAudioDevice::openInput()
{
    return initInput();
}

void MiniAudioDevice::close()
{
    if(mInputInitialized)
        ma_device_uninit(&inputDev);

    if(mOutputInitialized)
        ma_device_uninit(&outputDev);

    ma_context_uninit(&context);

    mInputInitialized = false;
    mOutputInitialized = false;
}

std::map<int, ma_device_info> MiniAudioDevice::getCompatibleInputDevices()
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        auto result = ma_context_get_devices(&context, NULL, NULL, &devices, &deviceCount);
        if(result == MA_SUCCESS) {
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);
            }
        }
    }

    ma_context_uninit(&context);

    return deviceList;
}

std::map<int, ma_device_info> MiniAudioDevice::getCompatibleOutputDevices()
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        auto result = ma_context_get_devices(&context, &devices, &deviceCount, NULL, NULL);
        if(result == MA_SUCCESS) {
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);
            }
        }
    }

    ma_context_uninit(&context);

    return deviceList;
}

bool MiniAudioDevice::initOutput()
{
    if(mOutputInitialized)
        ma_device_uninit(&outputDev);

    if(mOutputDeviceName.empty())
        return false; // bail early if the device name is empty

    ma_device_id outputDeviceId;
    if(!getDeviceForName(mOutputDeviceName, false, outputDeviceId))
        return false; // no device found

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &outputDeviceId;
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 1;
    cfg.playback.shareMode = ma_share_mode_shared;
    cfg.sampleRate = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData = this;
    cfg.dataCallback = maOutputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &outputDev);
    if(result != MA_SUCCESS) {
        return false;
    }

    result = ma_device_start(&outputDev);
    if(result != MA_SUCCESS) {
        return false;
    }

    mOutputInitialized = true;
    return true;
}

bool MiniAudioDevice::initInput()
{
    if(mInputInitialized)
        ma_device_uninit(&inputDev);

    if(mInputDeviceName.empty())
        return false; // bail early if the device name is empty

    ma_device_id inputDeviceId;
    if(!getDeviceForName(mInputDeviceName, true, inputDeviceId))
        return false; // no device found

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
        return false;
    }

    result = ma_device_start(&inputDev);
    if(result != MA_SUCCESS) {
        return false;
    }

    mInputInitialized = true;
    return true;
}

bool MiniAudioDevice::getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId)
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

int MiniAudioDevice::outputCallback(void *outputBuffer, unsigned int nFrames)
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

int MiniAudioDevice::inputCallback(const void *inputBuffer, unsigned int nFrames)
{
    std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
    if (mSink && inputBuffer) {
        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            mSink->putAudioFrame(reinterpret_cast<const float *>(inputBuffer) + i);
        }
    }

    return 0;
}

void MiniAudioDevice::maOutputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    auto device = reinterpret_cast<MiniAudioDevice*>(pDevice->pUserData);
    device->outputCallback(pOutput, frameCount);
}

void MiniAudioDevice::maInputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    auto device = reinterpret_cast<MiniAudioDevice*>(pDevice->pUserData);
    device->inputCallback(pInput, frameCount);
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
    return {};
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioDevice::getCompatibleInputDevices();
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
    }
    return returnDevices;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioDevice::getCompatibleOutputDevices();
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
        AudioDevice::Api audioApi) {
    auto devsp = std::make_shared<MiniAudioDevice>(userStreamName, outputDeviceId, inputDeviceId, audioApi);
    return devsp;
}
