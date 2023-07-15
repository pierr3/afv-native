cmake_minimum_required(VERSION 3.10)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

project(afv_native LANGUAGES CXX)

include(GenerateExportHeader)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)

set(AFV_NATIVE_HEADERS
	include/afv-native/atcClient.h
	include/afv-native/atisClient.h
	include/afv-native/Client.h
	include/afv-native/event.h
	include/afv-native/Log.h
	include/afv-native/afv/APISession.h
	include/afv-native/afv/ATCRadioStack.h
	include/afv-native/afv/EffectResources.h
	include/afv-native/afv/params.h
	include/afv-native/afv/RadioSimulation.h
	include/afv-native/afv/RemoteVoiceSource.h
	include/afv-native/afv/RollingAverage.h
	include/afv-native/afv/VoiceCompressionSink.h
	include/afv-native/afv/VoiceSession.h
	include/afv-native/afv/dto/AuthRequest.h
	include/afv-native/afv/dto/CrossCoupleGroup.h
	include/afv-native/afv/dto/PostCallsignResponse.h
	include/afv-native/afv/dto/Station.h
	include/afv-native/afv/dto/StationTransceiver.h
	include/afv-native/afv/dto/Transceiver.h
	include/afv-native/afv/dto/VoiceServerConnectionData.h
	include/afv-native/afv/dto/interfaces/IAudio.h
	include/afv-native/afv/dto/interfaces/IAudioDirect.h
	include/afv-native/afv/dto/interfaces/IAudioVoiceRoom.h
	include/afv-native/afv/dto/domain/RxTransceiver.h
	include/afv-native/afv/dto/domain/TxTransceiver.h
	include/afv-native/afv/dto/voice_server/AudioOnDirect.h
	include/afv-native/afv/dto/voice_server/AudioRxOnTransceivers.h
	include/afv-native/afv/dto/voice_server/AudioTxOnTransceivers.h
	include/afv-native/afv/dto/voice_server/Heartbeat.h
	include/afv-native/audio/audio_params.h
	include/afv-native/audio/AudioDevice.h
	include/afv-native/audio/BiQuadFilter.h
	include/afv-native/audio/FilterSource.h
	include/afv-native/audio/IFilter.h
	include/afv-native/audio/ISampleSink.h
	include/afv-native/audio/ISampleSource.h
	include/afv-native/audio/ISampleStorage.h
	include/afv-native/audio/ITick.h
	include/afv-native/audio/OutputMixer.h
	include/afv-native/audio/PinkNoiseGenerator.h
	include/afv-native/audio/RecordedSampleSource.h
	include/afv-native/audio/SineToneSource.h
	include/afv-native/audio/SinkFrameSizeAdjuster.h
	include/afv-native/audio/SourceFrameSizeAdjuster.h
	include/afv-native/audio/SourceToSinkAdapter.h
	include/afv-native/audio/SpeexPreprocessor.h
	include/afv-native/audio/VHFFilterSource.h
	include/afv-native/audio/WavFile.h
	include/afv-native/audio/WavSampleStorage.h
	include/afv-native/audio/WhiteNoiseGenerator.h
	include/afv-native/audio/miniaudio.h
	include/afv-native/audio/MiniAudioDevice.h
	include/afv-native/cryptodto/Channel.h
	include/afv-native/cryptodto/dto/ICryptoDTO.h
	include/afv-native/cryptodto/params.h
	include/afv-native/cryptodto/SequenceTest.h
	include/afv-native/cryptodto/UDPChannel.h
	include/afv-native/cryptodto/dto/ChannelConfig.h
	include/afv-native/cryptodto/dto/Header.h
	include/afv-native/event/EventTimer.h
	include/afv-native/event/EventCallbackTimer.h
	include/afv-native/http/EventTransferManager.h
	include/afv-native/http/http.h
	include/afv-native/http/Request.h
	include/afv-native/http/RESTRequest.h
	include/afv-native/http/TransferManager.h
	include/afv-native/util/base64.h
	include/afv-native/util/ChainedCallback.h
	include/afv-native/util/monotime.h
	include/afv-native/utility.h
	include/afv-native/atcClientWrapper.h)

add_library(afv_native SHARED
		${AFV_NATIVE_HEADERS}
		src/afv/APISession.cpp
		src/afv/EffectResources.cpp
		src/afv/RadioSimulation.cpp
		src/afv/RemoteVoiceSource.cpp
		src/afv/VoiceCompressionSink.cpp
		src/afv/VoiceSession.cpp
		src/afv/dto/AuthRequest.cpp
		src/afv/dto/PostCallsignResponse.cpp
		src/afv/dto/Station.cpp
		src/afv/dto/Transceiver.cpp
		src/afv/dto/VoiceServerConnectionData.cpp
		src/audio/AudioDevice.cpp
		src/audio/FilterSource.cpp
		src/audio/BiQuadFilter.cpp
		src/audio/OutputMixer.cpp
		src/audio/RecordedSampleSource.cpp
		src/audio/SineToneSource.cpp
		src/audio/SinkFrameSizeAdjuster.cpp
		src/audio/SourceFrameSizeAdjuster.cpp
		src/audio/SpeexPreprocessor.cpp
		src/audio/WavFile.cpp
		src/audio/WavSampleStorage.cpp
		src/audio/MiniAudioDevice.cpp
		src/core/Client.cpp
		src/core/Log.cpp
		src/cryptodto/Channel.cpp
		src/cryptodto/SequenceTest.cpp
		src/cryptodto/UDPChannel.cpp
		src/cryptodto/dto/ChannelConfig.cpp
		src/cryptodto/dto/Header.cpp
		src/event/EventCallbackTimer.cpp
		src/event/EventTimer.cpp
		src/http/EventTransferManager.cpp
		src/http/TransferManager.cpp
		src/http/Request.cpp
		src/http/RESTRequest.cpp
		src/util/base64.cpp
		src/util/monotime.cpp
		extern/simpleSource/SimpleComp.cpp
		extern/simpleSource/SimpleComp.h
		extern/simpleSource/SimpleEnvelope.cpp
		extern/simpleSource/SimpleEnvelope.h
		extern/simpleSource/SimpleGain.h
		extern/simpleSource/SimpleGate.cpp
		extern/simpleSource/SimpleGate.h
		extern/simpleSource/SimpleHeader.h
		extern/simpleSource/SimpleLimit.cpp
		extern/simpleSource/SimpleLimit.h
		src/audio/VHFFilterSource.cpp
		src/afv/ATCRadioStack.cpp
		src/afv/dto/CrossCoupleGroup.cpp
		src/afv/dto/StationTransceiver.cpp
		src/audio/SourceToSinkAdapter.cpp
		src/audio/VHFFilterSource.cpp
		src/core/atcClient.cpp
		src/core/atisClient.cpp
		extern/simpleSource/SimpleLimit.h
		src/atcClientWrapper.cpp)
		
# organise sources in generated projects.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${AFV_NATIVE_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/afv-native PREFIX "Headers" FILES ${AFV_NATIVE_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/extern PREFIX "Third Party" FILES ${AFV_NATIVE_THIRDPARTY_SOURCES})


# Finding all the VCPKG packages
find_package(OpenSSL REQUIRED)
find_package(cpp-jwt CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Libevent CONFIG REQUIRED)
find_package(msgpack-cxx CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Speexdsp does not have find_package
if (UNIX)
	find_library(SPEEXDSP_LIBRARY libspeexdsp.a)
endif()

if (WIN32)
	find_library(SPEEXDSP_LIBRARY speexdsp.lib)
endif()

if(APPLE)
	find_library(CORE_AUDIO CoreAudio)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(AUDIO_UNIT AudioUnit)
	find_library(CORE_FOUNDATION CoreFoundation)
	find_library(CORE_SERVICES CoreServices)

	target_link_libraries(afv_native
		PRIVATE
		${CORE_AUDIO}
		${AUDIO_TOOLBOX}
		${AUDIO_UNIT}
		${CORE_FOUNDATION}
		${CORE_SERVICES})
endif()

if (UNIX)
	target_link_libraries(afv_native PRIVATE libevent::pthreads)
endif()

target_link_libraries(afv_native
		PRIVATE
		OpenSSL::SSL OpenSSL::Crypto
		cpp-jwt::cpp-jwt
		CURL::libcurl
		libevent::core libevent::extra
		msgpack-cxx
		Opus::opus
		nlohmann_json nlohmann_json::nlohmann_json
		Threads::Threads
		${SPEEXDSP_LIBRARY})

target_include_directories(afv_native
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource
		PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_BINARY_DIR}/exports)

if(MSVC)
	# I hate to do this this way, but we must force MSVC to define the standard math macros whereever the afv headers
	# are used
	target_compile_definitions(afv_native PUBLIC _USE_MATH_DEFINES)
endif()

set(AFV_EXPORT_HEADERS 
	include/afv-native/atcClientWrapper.h
	include/afv-native/event.h)

set_target_properties(afv_native
    PROPERTIES
    PUBLIC_HEADER "${AFV_EXPORT_HEADERS}")

install(TARGETS afv_native
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION exports)

generate_export_header(afv_native EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/afv_native_export.h)

set_target_properties(afv_native PROPERTIES OUTPUT_NAME "afv")

if (WIN32)
	file(COPY ${SPEEXDSP_LIBRARY}
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()