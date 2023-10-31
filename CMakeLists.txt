cmake_minimum_required(VERSION 3.15)

# Setup vcpkg
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

project(
    afv_native
    VERSION 1.19.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if (APPLE)
	#disable runtime linking to prevent issues with backends that are not actually implemented
	add_definitions(-DMA_NO_RUNTIME_LINKING)
endif()
if (WIN32)
	add_definitions(-DMA_NO_JACK)
	add_definitions(-DMA_NO_WINMM)
endif()

add_library(${PROJECT_NAME} SHARED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extern/)

target_sources(${PROJECT_NAME} PRIVATE 
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
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleComp.h
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleEnvelope.h
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleGate.h
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleLimit.h
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleComp.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleEnvelope.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleGate.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/extern/simpleSource/SimpleLimit.cpp)


if(MSVC)
	# I hate to do this this way, but we must force MSVC to define the standard math macros whereever the afv headers
	# are used
	target_compile_definitions(afv_native PUBLIC _USE_MATH_DEFINES)
endif()

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
	target_link_libraries(afv_native PRIVATE libevent::pthreads)
endif()

if (WIN32)
	find_library(SPEEXDSP_LIBRARY speexdsp.lib)
endif()

target_link_libraries(afv_native
		PRIVATE
		OpenSSL::SSL OpenSSL::Crypto
		cpp-jwt::cpp-jwt
		CURL::libcurl
		libevent::core libevent::extra
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

# BUILD_INTERFACE specifies where to find includes during build time
# here we set the include directory to be our src include directory
# as well as CMAKE_CURRENT_BINARY_DIR, which is where the generated
# calc_exported.h file is located.
# the command must be included in double quotes so the two directories,
# separated by a ';' can both be used (cmake needs it to be a string)
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/;${CMAKE_CURRENT_BINARY_DIR}/include>"
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# generates header to provide export macros for library
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} 
						EXPORT_MACRO_NAME AFV_NATIVE_API
						EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}_export.h)

# specify the target to install (calculator library defined above)
# set the export name <name>-config (does not need to match target name)
# also specify where the .dylib/.so/.dll+.lib file should be installed
install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-config
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
    EXPORT ${PROJECT_NAME}-config
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/${PROJECT_NAME}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# copy the generated export file to the include install location
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}_export.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

if (WIN32)
	file(COPY ${SPEEXDSP_LIBRARY}
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()