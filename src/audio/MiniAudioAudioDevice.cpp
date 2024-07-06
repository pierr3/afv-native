#define MA_NO_CUSTOM
#define MA_NO_NULL

#include "MiniAudioAudioDevice.h"
#include "afv-native/audio/MiniAudioDevice.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include "afv-native/Log.h"

using namespace afv_native::audio;
using namespace std;

void logger(void *pUserData, ma_uint32 logLevel, const char *message) {
    (void) pUserData;
    std::string msg(message);
    msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.cend());
    LOG("MiniAudioAudioDevice", "%s: %s", ma_log_level_to_string(logLevel), msg.c_str());
}

MiniAudioAudioDevice::MiniAudioAudioDevice(const std::string &userStreamName, const std::string &deviceName, AudioDevice::Api audioApi, bool splitChannels):
    AudioDevice(), mUserStreamName(userStreamName), mDeviceName(deviceName), mDeviceInitialized(false), mSplitChannels(splitChannels) {
    ma_context_config contextConfig      = ma_context_config_init();
    contextConfig.threadPriority         = ma_thread_priority_normal;
    contextConfig.jack.pClientName       = mUserStreamName.c_str();
    contextConfig.pulse.pApplicationName = mUserStreamName.c_str();
    ma_result result = ma_context_init(NULL, 0, &contextConfig, &context);
    if (result == MA_SUCCESS) {
        ma_log_register_callback(ma_context_get_log(&context), ma_log_callback_init(logger, NULL));
        LOG("MiniAudioAudioDevice", "Context initialized. Audio Backend: %s",
            ma_get_backend_name(context.backend));
    } else {
        LOG("MiniAudioAudioDevice", "Error initializing context: %s", ma_result_description(result));
    }
}

MiniAudioAudioDevice::~MiniAudioAudioDevice() {
}

bool MiniAudioAudioDevice::openOutput() {
    return initOutput();
}

bool MiniAudioAudioDevice::openInput() {
    return initInput();
}

void MiniAudioAudioDevice::close() {
    if (mDeviceInitialized) {
        ma_device_uninit(&audioDevice);
    }
    ma_context_uninit(&context);
    mDeviceInitialized = false;
}

std::map<AudioDevice::Api, std::string> MiniAudioAudioDevice::getAvailableBackends() {
    ma_backend enabledBackends[MA_BACKEND_COUNT];
    size_t     enabledBackendCount;

    std::map<unsigned int, std::string> output;

    ma_result result = ma_get_enabled_backends(enabledBackends, MA_BACKEND_COUNT, &enabledBackendCount);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error getting available backends: %s", ma_result_description(result));
        return output;
    }

    LOG("MiniAudioAudioDevice", "Successfully queried %d backends.", enabledBackendCount);

    for (int i = 0; i < enabledBackendCount; i++) {
        output.insert(std::make_pair(static_cast<unsigned int>(enabledBackends[i]), ma_get_backend_name(enabledBackends[i])));
    }

    return output;
};

static std::map<int, ma_device_info> cachedInputDevices;
std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleInputDevices() {
    std::map<int, ma_device_info> deviceList;

    ma_device_info *devices;
    ma_uint32       deviceCount;
    ma_context      maContext;

    ma_result result = ma_context_init(NULL, 0, NULL, &maContext);
    if (result == MA_SUCCESS) {
        result = ma_context_get_devices(&maContext, NULL, NULL, &devices, &deviceCount);
        if (result == MA_SUCCESS) {
            // Check if there's a change in the available devices
            bool devicesChanged = false;
            if (cachedInputDevices.size() != deviceCount) {
                devicesChanged = true;
            } else {
                for (ma_uint32 i = 0; i < deviceCount; i++) {
                    if (cachedInputDevices.find(i) == cachedInputDevices.end() ||
                        memcmp(&cachedInputDevices[i].id, &devices[i].id, sizeof(ma_device_id)) != 0) {
                        devicesChanged = true;
                        break;
                    }
                }
            }

            if (devicesChanged) {
                LOG("MiniAudioAudioDevice", "Input devices have changed: %d devices found.", deviceCount);
                for (ma_uint32 i = 0; i < deviceCount; i++) {
                    deviceList.emplace(i, devices[i]);

                    // log detailed device info
                    {
                        ma_device_info detailedDeviceInfo;
                        result = ma_context_get_device_info(&maContext, ma_device_type_capture,
                                                            &devices[i].id, &detailedDeviceInfo);
                        if (result == MA_SUCCESS) {
                            LOG("MiniAudioAudioDevice", "Input: %s (Default: %s, Format Count: %d)",
                                devices[i].name, detailedDeviceInfo.isDefault ? "Yes" : "No",
                                detailedDeviceInfo.nativeDataFormatCount);

                            ma_uint32 iFormat;
                            for (iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat) {
                                LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                                    ma_get_format_name(detailedDeviceInfo
                                                           .nativeDataFormats[iFormat]
                                                           .format),
                                    detailedDeviceInfo
                                        .nativeDataFormats[iFormat]
                                        .channels,
                                    detailedDeviceInfo
                                        .nativeDataFormats[iFormat]
                                        .sampleRate);
                            }
                        } else {
                            LOG("MiniAudioAudioDevice", "Error getting input device info: %s", ma_result_description(result));
                        }
                    }
                }

                // Update the cache
                cachedInputDevices = deviceList;
            } else {
                deviceList = cachedInputDevices;
            }
        } else {
            LOG("MiniAudioAudioDevice", "Error querying input devices: %s", ma_result_description(result));
        }
    } else {
        LOG("MiniAudioAudioDevice", "Error initializing input device context: %s", ma_result_description(result));
    }

    ma_context_uninit(&maContext);

    LOG("MiniAudioAudioDevice", "Returning %d input devices.", deviceList.size());

    return deviceList;
}

static std::map<int, ma_device_info> cachedOutputDevices;
std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleOutputDevices() {
    std::map<int, ma_device_info> deviceList;

    ma_device_info *devices;
    ma_uint32       deviceCount;
    ma_context      maContext;

    ma_result result = ma_context_init(NULL, 0, NULL, &maContext);
    if (result == MA_SUCCESS) {
        result = ma_context_get_devices(&maContext, &devices, &deviceCount, NULL, NULL);
        if (result == MA_SUCCESS) {
            // Check if there's a change in the available devices
            bool devicesChanged = false;
            if (cachedOutputDevices.size() != deviceCount) {
                devicesChanged = true;
            } else {
                for (ma_uint32 i = 0; i < deviceCount; i++) {
                    if (cachedOutputDevices.find(i) == cachedOutputDevices.end() ||
                        memcmp(&cachedOutputDevices[i].id, &devices[i].id, sizeof(ma_device_id)) != 0) {
                        devicesChanged = true;
                        break;
                    }
                }
            }

            if (devicesChanged) {
                LOG("MiniAudioAudioDevice", "Output devices have changed: %d devices found.", deviceCount);
                for (ma_uint32 i = 0; i < deviceCount; i++) {
                    deviceList.emplace(i, devices[i]);

                    // log detailed device info
                    {
                        ma_device_info detailedDeviceInfo;
                        result = ma_context_get_device_info(&maContext, ma_device_type_playback,
                                                            &devices[i].id, &detailedDeviceInfo);
                        if (result == MA_SUCCESS) {
                            LOG("MiniAudioAudioDevice", "Output: %s (Default: %s, Format Count: %d)",
                                devices[i].name, detailedDeviceInfo.isDefault ? "Yes" : "No",
                                detailedDeviceInfo.nativeDataFormatCount);

                            ma_uint32 iFormat;
                            for (iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat) {
                                LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                                    ma_get_format_name(detailedDeviceInfo
                                                           .nativeDataFormats[iFormat]
                                                           .format),
                                    detailedDeviceInfo
                                        .nativeDataFormats[iFormat]
                                        .channels,
                                    detailedDeviceInfo
                                        .nativeDataFormats[iFormat]
                                        .sampleRate);
                            }
                        } else {
                            LOG("MiniAudioAudioDevice", "Error getting output device info: %s", ma_result_description(result));
                        }
                    }
                }

                // Update the cache
                cachedOutputDevices = deviceList;
            } else {
                deviceList = cachedOutputDevices;
            }
        } else {
            LOG("MiniAudioAudioDevice", "Error querying output devices: %s", ma_result_description(result));
        }
    } else {
        LOG("MiniAudioAudioDevice", "Error initializing output device context: %s", ma_result_description(result));
    }
    ma_context_uninit(&maContext);

    LOG("MiniAudioAudioDevice", "Returning %d output devices.", deviceList.size());

    return deviceList;
}

bool MiniAudioAudioDevice::initOutput() {
    if (mDeviceInitialized) {
        ma_device_uninit(&audioDevice);
    }

    if (mDeviceName.empty()) {
        LOG("MiniAudioAudioDevice::initOutput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id deviceId;
    if (!getDeviceForName(mDeviceName, false, deviceId)) {
        LOG("MiniAudioAudioDevice::initOutput()", "No device found for %s",
            mDeviceName.c_str());
        return false; // no device found
    }

    ma_device_config cfg   = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &deviceId;
    cfg.playback.format    = ma_format_f32;
    cfg.playback.channels  = mSplitChannels ? 2 : 1;
    cfg.playback.shareMode = ma_share_mode_shared;
    cfg.sampleRate         = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData          = this;
    cfg.dataCallback       = maOutputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &audioDevice);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing output device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&audioDevice);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting output device: %s", ma_result_description(result));
        return false;
    }

    mDeviceInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::initInput() {
    if (mDeviceInitialized) {
        ma_device_uninit(&audioDevice);
    }

    if (mDeviceName.empty()) {
        LOG("MiniAudioAudioDevice::initInput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id deviceId;
    if (!getDeviceForName(mDeviceName, true, deviceId)) {
        LOG("MiniAudioAudioDevice::initInput()", "No device found for %s",
            mDeviceName.c_str());
        return false; // no device found
    }

    ma_device_config cfg   = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID  = &deviceId;
    cfg.capture.format     = ma_format_f32;
    cfg.capture.channels   = 1;
    cfg.capture.shareMode  = ma_share_mode_shared;
    cfg.sampleRate         = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData          = this;
    cfg.dataCallback       = maInputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &audioDevice);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing input device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&audioDevice);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting input device: %s", ma_result_description(result));
        return false;
    }

    mDeviceInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId) {
    auto allDevices = forInput ? getCompatibleInputDevices() : getCompatibleOutputDevices();

    if (!allDevices.empty()) {
        for (const auto &devicePair: allDevices) {
            if (devicePair.second.name == deviceName) {
                deviceId = devicePair.second.id;
                return true;
            }
        }
    }

    return false;
}

int MiniAudioAudioDevice::outputCallback(void *outputBuffer, unsigned int nFrames) {
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

int MiniAudioAudioDevice::inputCallback(const void *inputBuffer, unsigned int nFrames) {
    std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
    if (mSink && inputBuffer) {
        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            mSink->putAudioFrame(reinterpret_cast<const float *>(inputBuffer) + i);
        }
    }

    return 0;
}

void MiniAudioAudioDevice::maOutputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto device = reinterpret_cast<MiniAudioAudioDevice *>(pDevice->pUserData);
    device->outputCallback(pOutput, frameCount);
}

void MiniAudioAudioDevice::maInputCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto device = reinterpret_cast<MiniAudioAudioDevice *>(pDevice->pUserData);
    device->inputCallback(pInput, frameCount);
}

void MiniAudioAudioDevice::notificationCallback(const ma_device_notification *pNotification) {
    std::lock_guard<std::mutex> funcGuard(mNotificationFuncLock);
    if (!mNotificationFunc || !mDeviceInitialized) {
        return;
    }

    if (!pNotification->pDevice || !pNotification->pDevice->pContext ||
        pNotification->pDevice->pContext == NULL) {
        return;
    }

    // auto device = reinterpret_cast<MiniAudioAudioDevice *>(pNotification->pDevice->pUserData);

    // if (pNotification->type == ma_device_notification_type_stopped) {
    //     if (mHasClosedManually) {
    //         mHasClosedManually = false; // This is a clean exit, we don't
    //         emit anything return;
    //     }

    //     // mNotificationFunc(mUserStreamName, 0);
    // }
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
    auto allApis = MiniAudioAudioDevice::getAvailableBackends();
    return allApis;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleInputDevices();
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(
            p.first, AudioDevice::DeviceInfo(p.second.name, p.second.name, p.second.isDefault));
    }
    LOG("AudioDevice", "Returning %d input devices.", returnDevices.size());
    return returnDevices;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleOutputDevices();
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(
                                           p.second.name, p.second.name, p.second.isDefault));
    }
    LOG("AudioDevice", "Returning %d output devices.", returnDevices.size());
    return returnDevices;
}

std::shared_ptr<AudioDevice> AudioDevice::makeDevice(const std::string &userStreamName, const std::string &deviceName, AudioDevice::Api audioApi, bool splitChannels) {
    auto devsp = std::make_shared<MiniAudioAudioDevice>(userStreamName, deviceName, audioApi, splitChannels);
    return devsp;
}
