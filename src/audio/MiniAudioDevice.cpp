#include "afv-native/audio/MiniAudioDevice.h"
#include "afv-native/Log.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

using namespace afv_native::audio;
using namespace std;

void logger(void *pUserData, ma_uint32 logLevel, const char *message) {
    (void) pUserData;
    std::string msg(message);
    msg.erase(std::find_if(msg.rbegin(), msg.rend(),
                           [](unsigned char ch) {
                               return ch != '\n';
                           })
                  .base(),
              msg.end());
    LOG("MiniAudioAudioDevice", "%s: %s", ma_log_level_to_string(logLevel), msg.c_str());
}

MiniAudioAudioDevice::MiniAudioAudioDevice(const std::string &userStreamName, const std::string &outputDeviceId, const std::string &inputDeviceId, AudioDevice::Api audioApi, bool makeStereo):
    AudioDevice(), mUserStreamName(userStreamName), mOutputDeviceId(outputDeviceId), mInputDeviceId(inputDeviceId), mInputInitialized(false), mOutputInitialized(false), mAudioApi(audioApi), mStereo(makeStereo) {
    ma_context_config contextConfig      = ma_context_config_init();
    contextConfig.threadPriority         = ma_thread_priority_normal;
    contextConfig.jack.pClientName       = mUserStreamName.c_str();
    contextConfig.pulse.pApplicationName = mUserStreamName.c_str();

    ma_result result;

    if (audioApi == -1) {
        LOG("MiniAudioAudioDevice", "Cannot initialize audio device with unknown audio api.");
        throw new std::runtime_error("MiniAudioAudioDevice: Cannot initialize audio device with unknown audio api.");
        return;
    } else {
        try {
            ma_backend backends[1] = {static_cast<ma_backend>(audioApi)};

            result = ma_context_init(backends, 1, &contextConfig, &context);
        } catch (std::exception &e) {
            LOG("MiniAudioAudioDevice", "Error initializing audio api, api unknown: %s", e.what());

            throw new std::exception();
        }
    }

    if (result == MA_SUCCESS) {
        ma_log_register_callback(ma_context_get_log(&context), ma_log_callback_init(logger, NULL));
        LOG("MiniAudioAudioDevice", "Context initialized. Audio Backend: %s",
            ma_get_backend_name(context.backend));

    } else {
        LOG("MiniAudioAudioDevice", "Error initializing context: %s", ma_result_description(result));
        throw new std::exception();
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
    mHasClosedManually = true;

    if (mInputInitialized) {
        ma_device_uninit(&inputDev);
    }

    if (mOutputInitialized) {
        ma_device_uninit(&outputDev);
    }

    ma_context_uninit(&context);

    mInputInitialized  = false;
    mOutputInitialized = false;
}

std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleInputDevices(unsigned int api) {
    std::map<int, ma_device_info> deviceList;

    ma_device_info *devices;
    ma_uint32       deviceCount;
    ma_context      context;

    ma_result result;
    if (api == -1) {
        result = ma_context_init(NULL, 0, NULL, &context);
    } else {
        try {
            ma_backend backends[1] = {static_cast<ma_backend>(api)};

            result = ma_context_init(backends, 1, NULL, &context);
        } catch (std::exception &e) {
            LOG("MiniAudioAudioDevice", "Error querying input devices due to wrong audio api: %s", e.what());

            return deviceList;
        }
    }

    if (result == MA_SUCCESS) {
        result = ma_context_get_devices(&context, NULL, NULL, &devices, &deviceCount);
        if (result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice", "Successfully queried %d input devices", deviceCount);
            for (ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);

                // log detailed device info
                {
                    ma_device_info detailedDeviceInfo;
                    result = ma_context_get_device_info(&context, ma_device_type_capture,
                                                        &devices[i].id, &detailedDeviceInfo);
                    if (result == MA_SUCCESS) {
                        LOG("MiniAudioAudioDevice", "Input: %s (Default: %s, Format Count: %d)",
                            devices[i].name, detailedDeviceInfo.isDefault ? "Yes" : "No",
                            detailedDeviceInfo.nativeDataFormatCount);

                        // ma_uint32 iFormat;
                        // for (iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat) {
                        //     LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                        //         ma_get_format_name(detailedDeviceInfo.nativeDataFormats[iFormat].format),
                        //         detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                        //         detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
                        // }
                    } else {
                        LOG("MiniAudioAudioDevice", "Error getting input device info: %s", ma_result_description(result));
                    }
                }
            }
        } else {
            LOG("MiniAudioAudioDevice", "Error querying input devices: %s", ma_result_description(result));
        }
    } else {
        LOG("MiniAudioAudioDevice", "Error initializing input device context: %s", ma_result_description(result));
    }

    ma_context_uninit(&context);

    return deviceList;
}

std::map<int, ma_device_info> MiniAudioAudioDevice::getCompatibleOutputDevices(unsigned int api) {
    std::map<int, ma_device_info> deviceList;

    ma_device_info *devices;
    ma_uint32       deviceCount;
    ma_context      context;

    ma_result result;
    if (api == -1) {
        result = ma_context_init(NULL, 0, NULL, &context);
    } else {
        try {
            ma_backend backends[1] = {static_cast<ma_backend>(api)};

            result = ma_context_init(backends, 1, NULL, &context);
        } catch (std::exception &e) {
            LOG("MiniAudioAudioDevice", "Error querying output devices due to wrong audio api: %s", e.what());

            return deviceList;
        }
    }

    if (result == MA_SUCCESS) {
        result = ma_context_get_devices(&context, &devices, &deviceCount, NULL, NULL);
        if (result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice", "Successfully queried %d output devices", deviceCount);
            for (ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);

                // log detailed device info
                {
                    ma_device_info detailedDeviceInfo;
                    result = ma_context_get_device_info(&context, ma_device_type_playback,
                                                        &devices[i].id, &detailedDeviceInfo);
                    if (result == MA_SUCCESS) {
                        LOG("MiniAudioAudioDevice", "Output: %s (Default: %s, Format Count: %d)",
                            devices[i].name, detailedDeviceInfo.isDefault ? "Yes" : "No",
                            detailedDeviceInfo.nativeDataFormatCount);

                        // ma_uint32 iFormat;
                        // for (iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat) {
                        //     LOG("MiniAudioAudioDevice", "   --> Format: %s, Channels: %d, Sample Rate: %d",
                        //         ma_get_format_name(detailedDeviceInfo.nativeDataFormats[iFormat].format),
                        //         detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                        //         detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
                        // }
                    } else {
                        LOG("MiniAudioAudioDevice", "Error getting output device info: %s", ma_result_description(result));
                    }
                }
            }
        } else {
            LOG("MiniAudioAudioDevice", "Error querying output devices: %s", ma_result_description(result));
        }
    } else {
        LOG("MiniAudioAudioDevice", "Error initializing output device context: %s", ma_result_description(result));
    }

    ma_context_uninit(&context);

    return deviceList;
}

bool MiniAudioAudioDevice::initOutput() {
    if (mOutputInitialized) {
        ma_device_uninit(&outputDev);
    }

    if (mOutputDeviceId.empty()) {
        LOG("MiniAudioAudioDevice::initOutput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id outputDeviceId;
    if (!getDeviceForId(mOutputDeviceId, false, outputDeviceId)) {
        LOG("MiniAudioAudioDevice::initOutput()", "No device found for %s",
            mOutputDeviceId.c_str());
        return false; // no device found
    }

    ma_device_config cfg   = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &outputDeviceId;
    cfg.playback.format    = ma_format_f32;
    cfg.playback.channels  = mStereo ? 2 : 1;
    cfg.playback.shareMode = ma_share_mode_shared;

    cfg.sampleRate         = sampleRateHz;
    cfg.periodSizeInFrames = frameSizeSamples;
    cfg.pUserData          = this;
    cfg.dataCallback       = maOutputCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &outputDev);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing output device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&outputDev);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting output device: %s", ma_result_description(result));
        return false;
    }

    mOutputInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::initInput() {
    if (mInputInitialized) {
        ma_device_uninit(&inputDev);
    }

    if (mInputDeviceId.empty()) {
        LOG("MiniAudioAudioDevice::initInput()", "Device name is empty");
        return false; // bail early if the device name is empty
    }

    ma_device_id inputDeviceId;
    if (!getDeviceForId(mInputDeviceId, true, inputDeviceId)) {
        LOG("MiniAudioAudioDevice::initInput()", "No device found for %s",
            mInputDeviceId.c_str());
        return false; // no device found
    }

    ma_device_config cfg     = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID    = &inputDeviceId;
    cfg.capture.format       = ma_format_f32;
    cfg.capture.channels     = 1;
    cfg.capture.shareMode    = ma_share_mode_shared;
    cfg.sampleRate           = sampleRateHz;
    cfg.periodSizeInFrames   = frameSizeSamples;
    cfg.pUserData            = this;
    cfg.dataCallback         = maInputCallback;
    cfg.notificationCallback = maNotificationCallback;

    ma_result result;

    result = ma_device_init(&context, &cfg, &inputDev);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error initializing input device: %s", ma_result_description(result));
        return false;
    }

    result = ma_device_start(&inputDev);
    if (result != MA_SUCCESS) {
        LOG("MiniAudioAudioDevice", "Error starting input device: %s", ma_result_description(result));
        return false;
    }

    mInputInitialized = true;
    return true;
}

bool MiniAudioAudioDevice::getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId) {
    auto allDevices = forInput ? getCompatibleInputDevices(mAudioApi) : getCompatibleOutputDevices(mAudioApi);

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

bool MiniAudioAudioDevice::getDeviceForId(const std::string &inDeviceId, bool forInput, ma_device_id &deviceId) {
    auto allDevices = forInput ? getCompatibleInputDevices(mAudioApi) : getCompatibleOutputDevices(mAudioApi);

    if (!allDevices.empty()) {
        for (const auto &devicePair: allDevices) {
            if (getDeviceId(devicePair.second.id, mAudioApi, devicePair.second.name) == inDeviceId) {
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
                // if there's no source, but there is an output buffer, zero it
                // to avoid making horrible buzzing sounds.
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

void MiniAudioAudioDevice::maNotificationCallback(const ma_device_notification *pNotification) {
    LOG("MiniAudioAudioDevice", "Notification Callback!");
    auto device =
        reinterpret_cast<MiniAudioAudioDevice *>(pNotification->pDevice->pUserData);
    device->notificationCallback(pNotification);
};

std::map<int, std::string> MiniAudioAudioDevice::getAvailableBackends() {
    ma_backend enabledBackends[MA_BACKEND_COUNT];
    size_t     enabledBackendCount;

    std::map<int, std::string> output;

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
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
    return MiniAudioAudioDevice::getAvailableBackends();
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleInputDevices(api);
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(
            p.first, AudioDevice::DeviceInfo(p.second.name, p.second.isDefault ? true : false,
                                             MiniAudioAudioDevice::getDeviceId(
                                                 p.second.id, api, p.second.name)));
    }
    return returnDevices;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioAudioDevice::getCompatibleOutputDevices(api);
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(
            p.first, AudioDevice::DeviceInfo(p.second.name, p.second.isDefault ? true : false,
                                             MiniAudioAudioDevice::getDeviceId(
                                                 p.second.id, api, p.second.name)));
    }
    return returnDevices;
}

std::shared_ptr<AudioDevice> AudioDevice::makeDevice(const std::string &userStreamName, const std::string &outputDeviceId, const std::string &inputDeviceId, AudioDevice::Api audioApi, bool makeStereo) {
    try {
        return std::make_shared<MiniAudioAudioDevice>(userStreamName, outputDeviceId, inputDeviceId, audioApi, makeStereo);
    } catch (std::exception &e) {
        return nullptr;
    }
}
void afv_native::audio::MiniAudioAudioDevice::notificationCallback(const ma_device_notification *pNotification) {
    std::lock_guard<std::mutex> funcGuard(mNotificationFuncLock);
    if (!mNotificationFunc || !mInputInitialized || !mOutputInitialized) {
        return;
    }

    if (!pNotification->pDevice || !pNotification->pDevice->pContext ||
        pNotification->pDevice->pContext == NULL) {
        return;
    }

    auto device =
        reinterpret_cast<MiniAudioAudioDevice *>(pNotification->pDevice->pUserData);

    if (pNotification->type == ma_device_notification_type_stopped) {
        if (mHasClosedManually) {
            mHasClosedManually = false; // This is a clean exit, we don't emit anything
            return;
        }

        mNotificationFunc(mUserStreamName, 0);
    }
}
std::string afv_native::audio::MiniAudioAudioDevice::getDeviceId(const ma_device_id &deviceId, const AudioDevice::Api &api, const std::string &deviceName) {
    if (api >= MA_BACKEND_COUNT || api == -1) {
        LOG("MiniAudioAudioDevice", "Error getting device ID for audio api, api unknown: %d", api);
        return deviceName;
    }

    const auto miniAudioApi = static_cast<ma_backend>(api);

    if (miniAudioApi == ma_backend_wasapi) {
#ifdef WIN32
        // Determine the length of the converted string
        int length = WideCharToMultiByte(CP_UTF8, 0, deviceId.wasapi, -1, nullptr, 0, nullptr, nullptr);
        if (length == 0) {
            // Conversion failed
            return "";
        }

        // Allocate a buffer to hold the converted string
        std::string result(length - 1, '\0'); // Length includes the null terminator, which we don't need

        // Perform the conversion
        WideCharToMultiByte(CP_UTF8, 0, deviceId.wasapi, -1, &result[0], length, nullptr, nullptr);
        return result;
#endif
    }

    if (miniAudioApi == ma_backend_dsound) {
        return deviceName;
    }

    if (miniAudioApi == ma_backend_winmm) {
        return std::to_string(deviceId.winmm);
    }

    if (miniAudioApi == ma_backend_coreaudio) {
        return deviceId.coreaudio;
    }

    if (miniAudioApi == ma_backend_sndio) {
        return deviceId.sndio;
    }

    if (miniAudioApi == ma_backend_audio4) {
        return deviceId.audio4;
    }

    if (miniAudioApi == ma_backend_oss) {
        return deviceId.oss;
    }

    if (miniAudioApi == ma_backend_pulseaudio) {
        return deviceId.pulse;
    }

    if (miniAudioApi == ma_backend_alsa) {
        return deviceId.alsa;
    }

    if (miniAudioApi == ma_backend_jack) {
        return std::to_string(deviceId.jack);
    }

    if (miniAudioApi == ma_backend_aaudio) {
        return std::to_string(deviceId.aaudio);
    }

    if (miniAudioApi == ma_backend_opensl) {
        return std::to_string(deviceId.opensl);
    }

    if (miniAudioApi == ma_backend_webaudio) {
        return deviceId.webaudio;
    }

    if (miniAudioApi == ma_backend_null) {
        return std::to_string(deviceId.nullbackend);
    }

    return deviceName;
}
