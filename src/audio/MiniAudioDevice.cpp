#include "afv-native/audio/MiniAudioDevice.h"
#include <cstring>
#include <memory>

using namespace afv_native::audio;
using namespace std;

void logger(void *pUserData, ma_uint32 logLevel, const char *message) {
  (void)pUserData;
  std::string msg(message);
  msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.cend());
  LOG("MiniAudioAudioDevice", "%s: %s", ma_log_level_to_string(logLevel),
      msg.c_str());
}

MiniAudioAudioDevice::MiniAudioAudioDevice(const std::string &userStreamName,
                                           const std::string &outputDeviceName,
                                           const std::string &inputDeviceName,
                                           AudioDevice::Api audioApi,
                                           int outputChannel)
    : AudioDevice(), mUserStreamName(userStreamName),
      mOutputDeviceName(outputDeviceName), mInputDeviceName(inputDeviceName),
      mInputInitialized(false), mOutputInitialized(false),
      mOutputChannel(outputChannel), mAudioApi(audioApi) {
  ma_context_config contextConfig = ma_context_config_init();
  contextConfig.threadPriority = ma_thread_priority_normal;
  contextConfig.jack.pClientName = mUserStreamName.c_str();
  contextConfig.pulse.pApplicationName = mUserStreamName.c_str();

  ma_result result;

  if (audioApi == -1) {
    result = ma_context_init(NULL, 0, &contextConfig, &context);
  } else {
    try {
      ma_backend backends[1] = {static_cast<ma_backend>(audioApi)};

      result = ma_context_init(backends, 1, &contextConfig, &context);
    } catch (std::exception &e) {
      LOG("MiniAudioAudioDevice",
          "Error initializing audio api, api unknown: %s", e.what());

      throw new std::exception();
    }
  }

  if (result == MA_SUCCESS) {
    ma_log_register_callback(ma_context_get_log(&context),
                             ma_log_callback_init(logger, NULL));
    LOG("MiniAudioAudioDevice", "Context initialized. Audio Backend: %s",
        ma_get_backend_name(context.backend));
  } else {
    LOG("MiniAudioAudioDevice", "Error initializing context: %s",
        ma_result_description(result));
    throw new std::exception();
  }
}

MiniAudioAudioDevice::~MiniAudioAudioDevice() {}

bool MiniAudioAudioDevice::openOutput() { return initOutput(); }

bool MiniAudioAudioDevice::openInput() { return initInput(); }

void MiniAudioAudioDevice::close() {
  if (mInputInitialized)
    ma_device_uninit(&inputDev);

  if (mOutputInitialized)
    ma_device_uninit(&outputDev);

  ma_context_uninit(&context);

  mInputInitialized = false;
  mOutputInitialized = false;
}

int MiniAudioAudioDevice::playWav(const std::string path) {
  if (!mOutputInitialized) {
    return 0;
  }

  ma_result result;
  ma_sound sound;

  return 1;
};

std::map<int, ma_device_info>
MiniAudioAudioDevice::getCompatibleInputDevices(unsigned int api) {
  std::map<int, ma_device_info> deviceList;

  ma_device_info *devices;
  ma_uint32 deviceCount;
  ma_context context;

  ma_result result;
  if (api == -1) {
    result = ma_context_init(NULL, 0, NULL, &context);
  } else {
    try {
      ma_backend backends[1] = {static_cast<ma_backend>(api)};

      result = ma_context_init(backends, 1, NULL, &context);
    } catch (std::exception &e) {
      LOG("MiniAudioAudioDevice",
          "Error querying input devices due to wrong audio api: %s", e.what());

      return deviceList;
    }
  }

  if (result == MA_SUCCESS) {
    result =
        ma_context_get_devices(&context, NULL, NULL, &devices, &deviceCount);
    if (result == MA_SUCCESS) {
      LOG("MiniAudioAudioDevice", "Successfully queried %d input devices",
          deviceCount);
      for (ma_uint32 i = 0; i < deviceCount; i++) {
        deviceList.emplace(i, devices[i]);

        // log detailed device info
        {
          ma_device_info detailedDeviceInfo;
          result =
              ma_context_get_device_info(&context, ma_device_type_capture,
                                         &devices[i].id, &detailedDeviceInfo);
          if (result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice",
                "Input: %s (Default: %s, Format Count: %d)", devices[i].name,
                detailedDeviceInfo.isDefault ? "Yes" : "No",
                detailedDeviceInfo.nativeDataFormatCount);

            ma_uint32 iFormat;
            for (iFormat = 0;
                 iFormat < detailedDeviceInfo.nativeDataFormatCount;
                 ++iFormat) {
              LOG("MiniAudioAudioDevice",
                  "   --> Format: %s, Channels: %d, Sample Rate: %d",
                  ma_get_format_name(
                      detailedDeviceInfo.nativeDataFormats[iFormat].format),
                  detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                  detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
            }
          } else {
            LOG("MiniAudioAudioDevice", "Error getting input device info: %s",
                ma_result_description(result));
          }
        }
      }
    } else {
      LOG("MiniAudioAudioDevice", "Error querying input devices: %s",
          ma_result_description(result));
    }
  } else {
    LOG("MiniAudioAudioDevice", "Error initializing input device context: %s",
        ma_result_description(result));
  }

  ma_context_uninit(&context);

  return deviceList;
}

std::map<int, ma_device_info>
MiniAudioAudioDevice::getCompatibleOutputDevices(unsigned int api) {
  std::map<int, ma_device_info> deviceList;

  ma_device_info *devices;
  ma_uint32 deviceCount;
  ma_context context;

  ma_result result;
  if (api == -1) {
    result = ma_context_init(NULL, 0, NULL, &context);
  } else {
    try {
      ma_backend backends[1] = {static_cast<ma_backend>(api)};

      result = ma_context_init(backends, 1, NULL, &context);
    } catch (std::exception &e) {
      LOG("MiniAudioAudioDevice",
          "Error querying input devices due to wrong audio api: %s", e.what());

      return deviceList;
    }
  }

  if (result == MA_SUCCESS) {
    result =
        ma_context_get_devices(&context, &devices, &deviceCount, NULL, NULL);
    if (result == MA_SUCCESS) {
      LOG("MiniAudioAudioDevice", "Successfully queried %d output devices",
          deviceCount);
      for (ma_uint32 i = 0; i < deviceCount; i++) {
        deviceList.emplace(i, devices[i]);

        // log detailed device info
        {
          ma_device_info detailedDeviceInfo;
          result =
              ma_context_get_device_info(&context, ma_device_type_playback,
                                         &devices[i].id, &detailedDeviceInfo);
          if (result == MA_SUCCESS) {
            LOG("MiniAudioAudioDevice",
                "Output: %s (Default: %s, Format Count: %d)", devices[i].name,
                detailedDeviceInfo.isDefault ? "Yes" : "No",
                detailedDeviceInfo.nativeDataFormatCount);

            ma_uint32 iFormat;
            for (iFormat = 0;
                 iFormat < detailedDeviceInfo.nativeDataFormatCount;
                 ++iFormat) {
              LOG("MiniAudioAudioDevice",
                  "   --> Format: %s, Channels: %d, Sample Rate: %d",
                  ma_get_format_name(
                      detailedDeviceInfo.nativeDataFormats[iFormat].format),
                  detailedDeviceInfo.nativeDataFormats[iFormat].channels,
                  detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
            }
          } else {
            LOG("MiniAudioAudioDevice", "Error getting output device info: %s",
                ma_result_description(result));
          }
        }
      }
    } else {
      LOG("MiniAudioAudioDevice", "Error querying output devices: %s",
          ma_result_description(result));
    }
  } else {
    LOG("MiniAudioAudioDevice", "Error initializing output device context: %s",
        ma_result_description(result));
  }

  ma_context_uninit(&context);

  return deviceList;
}

bool MiniAudioAudioDevice::initOutput() {
  if (mOutputInitialized)
    ma_device_uninit(&outputDev);

  if (mOutputDeviceName.empty()) {
    LOG("MiniAudioAudioDevice::initOutput()", "Device name is empty");
    return false; // bail early if the device name is empty
  }

  ma_device_id outputDeviceId;
  if (!getDeviceForName(mOutputDeviceName, false, outputDeviceId)) {
    LOG("MiniAudioAudioDevice::initOutput()", "No device found for %s",
        mOutputDeviceName.c_str());
    return false; // no device found
  }

  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.pDeviceID = &outputDeviceId;
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = 1;
  cfg.playback.shareMode = ma_share_mode_shared;

  cfg.sampleRate = sampleRateHz;
  cfg.periodSizeInFrames = frameSizeSamples;
  cfg.pUserData = this;
  cfg.dataCallback = maOutputCallback;

  if (mOutputChannel != 0) {
    ma_channel my_channels[1];

    if (mOutputChannel == 1) {
      my_channels[0] = MA_CHANNEL_LEFT;
    }

    if (mOutputChannel == 2) {
      my_channels[0] = MA_CHANNEL_RIGHT;
    }

    cfg.playback.pChannelMap = my_channels;
    cfg.playback.channelMixMode = ma_channel_mix_mode_simple;
  }
  ma_result result;

  result = ma_device_init(&context, &cfg, &outputDev);
  if (result != MA_SUCCESS) {
    LOG("MiniAudioAudioDevice", "Error initializing output device: %s",
        ma_result_description(result));
    return false;
  }

  result = ma_device_start(&outputDev);
  if (result != MA_SUCCESS) {
    LOG("MiniAudioAudioDevice", "Error starting output device: %s",
        ma_result_description(result));
    return false;
  }

  mOutputInitialized = true;
  return true;
}

bool MiniAudioAudioDevice::initInput() {
  if (mInputInitialized)
    ma_device_uninit(&inputDev);

  if (mInputDeviceName.empty()) {
    LOG("MiniAudioAudioDevice::initInput()", "Device name is empty");
    return false; // bail early if the device name is empty
  }

  ma_device_id inputDeviceId;
  if (!getDeviceForName(mInputDeviceName, true, inputDeviceId)) {
    LOG("MiniAudioAudioDevice::initInput()", "No device found for %s",
        mInputDeviceName.c_str());
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
  cfg.notificationCallback = maNotificationCallback;

  ma_result result;

  result = ma_device_init(&context, &cfg, &inputDev);
  if (result != MA_SUCCESS) {
    LOG("MiniAudioAudioDevice", "Error initializing input device: %s",
        ma_result_description(result));
    return false;
  }

  result = ma_device_start(&inputDev);
  if (result != MA_SUCCESS) {
    LOG("MiniAudioAudioDevice", "Error starting input device: %s",
        ma_result_description(result));
    return false;
  }

  mInputInitialized = true;
  return true;
}

bool MiniAudioAudioDevice::getDeviceForName(const std::string &deviceName,
                                            bool forInput,
                                            ma_device_id &deviceId) {
  auto allDevices = forInput ? getCompatibleInputDevices(mAudioApi)
                             : getCompatibleOutputDevices(mAudioApi);

  if (!allDevices.empty()) {
    for (const auto &devicePair : allDevices) {
      if (devicePair.second.name == deviceName) {
        deviceId = devicePair.second.id;
        return true;
      }
    }
  }

  return false;
}

int MiniAudioAudioDevice::outputCallback(void *outputBuffer,
                                         unsigned int nFrames) {
  if (outputBuffer) {
    std::lock_guard<std::mutex> sourceGuard(mSourcePtrLock);
    for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
      if (mSource) {
        SourceStatus rv;
        rv =
            mSource->getAudioFrame(reinterpret_cast<float *>(outputBuffer) + i);
        if (rv != SourceStatus::OK) {
          ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0,
                   frameSizeBytes);
          mSource.reset();
        }
      } else {
        // if there's no source, but there is an output buffer, zero it to avoid
        // making horrible buzzing sounds.
        ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0,
                 frameSizeBytes);
      }
    }
  }

  return 0;
}

int MiniAudioAudioDevice::inputCallback(const void *inputBuffer,
                                        unsigned int nFrames) {
  std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
  if (mSink && inputBuffer) {
    for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
      mSink->putAudioFrame(reinterpret_cast<const float *>(inputBuffer) + i);
    }
  }

  return 0;
}

void MiniAudioAudioDevice::maOutputCallback(ma_device *pDevice, void *pOutput,
                                            const void *pInput,
                                            ma_uint32 frameCount) {
  auto device = reinterpret_cast<MiniAudioAudioDevice *>(pDevice->pUserData);
  device->outputCallback(pOutput, frameCount);
}

void MiniAudioAudioDevice::maInputCallback(ma_device *pDevice, void *pOutput,
                                           const void *pInput,
                                           ma_uint32 frameCount) {
  auto device = reinterpret_cast<MiniAudioAudioDevice *>(pDevice->pUserData);
  device->inputCallback(pInput, frameCount);
}

void MiniAudioAudioDevice::maNotificationCallback(
    const ma_device_notification *pNotification) {
  auto device = reinterpret_cast<MiniAudioAudioDevice *>(
      pNotification->pDevice->pUserData);

  if (pNotification->type == ma_device_notification_type_stopped) {
  }
};

std::map<unsigned int, std::string>
MiniAudioAudioDevice::getAvailableBackends() {
  ma_backend enabledBackends[MA_BACKEND_COUNT];
  size_t enabledBackendCount;

  std::map<unsigned int, std::string> output;

  ma_result result = ma_get_enabled_backends(enabledBackends, MA_BACKEND_COUNT,
                                             &enabledBackendCount);
  if (result != MA_SUCCESS) {
    LOG("MiniAudioAudioDevice", "Error getting available backends: %s",
        ma_result_description(result));
    return output;
  }

  LOG("MiniAudioAudioDevice", "Successfully queried %d backends.",
      enabledBackendCount);

  for (int i = 0; i < enabledBackendCount; i++) {
    output.insert(std::make_pair(static_cast<unsigned int>(enabledBackends[i]),
                                 ma_get_backend_name(enabledBackends[i])));
  }

  return output;
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
  return MiniAudioAudioDevice::getAvailableBackends();
}

map<int, AudioDevice::DeviceInfo>
AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
  auto allDevices = MiniAudioAudioDevice::getCompatibleInputDevices(api);
  map<int, AudioDevice::DeviceInfo> returnDevices;
  for (const auto &p : allDevices) {
    returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
  }
  return returnDevices;
}

map<int, AudioDevice::DeviceInfo>
AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
  auto allDevices = MiniAudioAudioDevice::getCompatibleOutputDevices(api);
  map<int, AudioDevice::DeviceInfo> returnDevices;
  for (const auto &p : allDevices) {
    returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
  }
  return returnDevices;
}

std::shared_ptr<AudioDevice>
AudioDevice::makeDevice(const std::string &userStreamName,
                        const std::string &outputDeviceId,
                        const std::string &inputDeviceId,
                        AudioDevice::Api audioApi, int outputChannel) {

  try {
    return std::make_shared<MiniAudioAudioDevice>(
        userStreamName, outputDeviceId, inputDeviceId, audioApi, outputChannel);
  } catch (std::exception &e) {
    return nullptr;
  }
}
