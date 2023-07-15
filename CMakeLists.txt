cmake_minimum_required(VERSION 3.10)

# Setup vcpkg
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

project(afv_native LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_SHARED_LIBS "Build shared library" ON)

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

add_library(afv_native
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/APISession.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/EffectResources.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/RadioSimulation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/RemoteVoiceSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/VoiceCompressionSink.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/VoiceSession.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/AuthRequest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/PostCallsignResponse.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/Station.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/Transceiver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/VoiceServerConnectionData.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioDevice.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/FilterSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/BiQuadFilter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/OutputMixer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RecordedSampleSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/SineToneSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/SinkFrameSizeAdjuster.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/SourceFrameSizeAdjuster.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/SpeexPreprocessor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/WavFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/WavSampleStorage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioDevice.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/core/Client.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/core/Log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cryptodto/Channel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cryptodto/SequenceTest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cryptodto/UDPChannel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cryptodto/dto/ChannelConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cryptodto/dto/Header.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/event/EventCallbackTimer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/event/EventTimer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/http/EventTransferManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/http/TransferManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/http/Request.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/http/RESTRequest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/util/base64.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/util/monotime.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/VHFFilterSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/ATCRadioStack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/CrossCoupleGroup.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/afv/dto/StationTransceiver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/SourceToSinkAdapter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/audio/VHFFilterSource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/core/atcClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/core/atisClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/atcClientWrapper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleComp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleEnvelope.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleGate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleLimit.cpp)

add_library(afv_native::afv_native ALIAS afv_native)
	
# organise sources in generated projects.
#source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${AFV_NATIVE_SOURCES})
#source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/afv-native PREFIX "Headers" FILES ${AFV_NATIVE_HEADERS})
#source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/extern PREFIX "Third Party" FILES ${AFV_NATIVE_THIRDPARTY_SOURCES})

target_include_directories(afv_native
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
	# I hate to do this this way, but we must force MSVC to define the standard math macros whereever the afv headers
	# are used
	target_compile_definitions(afv_native PUBLIC _USE_MATH_DEFINES)
endif()


include(GenerateExportHeader)
generate_export_header(afv_native
    EXPORT_MACRO_NAME AFV_NATIVE_API
    EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/include/afv_native_export.h
)

target_include_directories(afv_native
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(afv_native PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


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

target_link_libraries(afv_native
		PRIVATE
		OpenSSL::SSL OpenSSL::Crypto
		cpp-jwt::cpp-jwt
		CURL::libcurl
		libevent::core libevent::extra libevent::pthreads
		msgpack-cxx
		Opus::opus
		nlohmann_json::nlohmann_json
		Threads::Threads
		${SPEEXDSP_LIBRARY})

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

include(GNUInstallDirs)
install(TARGETS afv_native
    EXPORT afv_native-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT afv_native-targets
    FILE afv_native-targets.cmake
    NAMESPACE afv_native::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/afv_native
)


include(CMakePackageConfigHelpers)

configure_package_config_file(
	${CMAKE_SOURCE_DIR}/cmake/afv_native-config.cmake.in
	${CMAKE_BINARY_DIR}/cmake/afv_native-config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/afv_native
)

write_basic_package_version_file(
	${CMAKE_BINARY_DIR}/cmake/afv_native-config-version.cmake
	VERSION 1.0
	COMPATIBILITY AnyNewerVersion
)

install(
	FILES
		${CMAKE_BINARY_DIR}/cmake/afv_native-config.cmake
		${CMAKE_BINARY_DIR}/cmake/afv_native-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/afv_native
)

#set(AFV_EXPORT_HEADERS 
#	include/afv-native/atcClientWrapper.h
#	include/afv-native/event.h)

if (WIN32)
	file(COPY ${SPEEXDSP_LIBRARY}
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
