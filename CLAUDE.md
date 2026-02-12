# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AFV-Native is a C++17 library implementing the Audio for VATSIM (AFV) client protocol. It provides voice communication for virtual aviation, supporting both pilot clients (2+ radios) and ATC clients (many simultaneous frequencies). The library outputs a shared library (.so/.dylib/.dll) consumed by flight simulator plugins and ATC software.

## Build Commands

**Prerequisites:** CMake 3.29.2+, vcpkg (included as git submodule)

```bash
# First-time setup
git submodule update --recursive --init
./vcpkg/bootstrap-vcpkg.sh    # or bootstrap-vcpkg.bat on Windows

# Standard build
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/

# macOS universal binary (arm64 + x86_64)
./build-mac.sh

# macOS arm64 only
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DVCPKG_TARGET_TRIPLET=arm64-osx
cmake --build build/

# Linux static library
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release -DAFV_STATIC=1
cmake --build build/

# Linux ARM64 cross-compile
cmake -S . -B build/ -DCMAKE_BUILD_ARM64_LINUX=1
cmake --build build/

# Windows (MSVC)
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --config Release
```

**No automated test suite exists.** Testing is manual via the `afv-native-testclient/` ImGui GUI application (connects to VATSIM with real credentials).

## Architecture

### Layered Design

```
Client API Layer         Client.h (pilot), ATCClient.h (ATC), atcClientFlat.h (C FFI)
        │
Protocol Layer           APISession (REST/JWT auth), VoiceSession (voice server handshake)
        │
Radio Simulation         RadioSimulation (pilot), ATCRadioSimulation (ATC)
        │
Audio Processing         OutputMixer, VHFFilterSource, BiQuadFilter, SpeexPreprocessor,
                         SimpleCompressorEffect, RemoteVoiceSource, VoiceCompressionSink
        │
I/O Layer                MiniAudioDevice (audio), UDPChannel (network), TransferManager (HTTP)
```

### Key Modules

- **`src/core/`** — Client entry points. `Client.cpp` is the pilot client, `atcClient.cpp` is the ATC client, `atisClient.cpp` is the ATIS variant. `atcClientWrapper.cpp` and `atcClientFlat.cpp` provide C-compatible API wrappers for FFI consumers.

- **`src/afv/`** — AFV protocol. `APISession` handles REST auth + JWT token refresh + station queries. `VoiceSession` manages voice server connection lifecycle. `RadioSimulation` (pilot, ~600 lines) and `ATCRadioSimulation` (ATC, ~1000 lines) are the most complex audio logic — they manage per-frequency RX/TX state, mixing, and effects.

- **`src/audio/`** — Pull-based audio pipeline using `ISampleSource`/`ISampleSink` interfaces. Sources are chained through filters (BiQuad, VHF, Speex). `MiniAudioDevice` wraps vendored miniaudio for platform audio I/O. `SourceFrameSizeAdjuster`/`SinkFrameSizeAdjuster`/`SourceToSinkAdapter` bridge mismatched frame sizes between pipeline stages.

- **`src/cryptodto/`** — CryptoDTO encrypted UDP protocol. `Channel` handles ChaCha20-Poly1305 AEAD encryption. `UDPChannel` wraps POCO UDP sockets. `SequenceTest` validates packet ordering.

- **`src/http/`** — Async HTTP via libcurl. `TransferManager`/`PollingTransferManager` manage request queues.

- **`extern/`** — Vendored DSP code: ChunkWare SimpleCompressor (MIT) and a small compressor library.

### Three Public API Surfaces

1. **C++ Pilot API** (`Client.h`) — Simple interface for 2+ radio pilot clients
2. **C++ ATC API** (`ATCClient.h`) — Full-featured ATC with cross-coupling, per-frequency hardware simulation, ad-hoc sound playback, loopback/sidetone
3. **C Flat API** (`atcClientFlat.h`) — `extern "C"` wrapper with opaque handles, suitable for .NET P/Invoke, JNI, etc.

### Threading Model

- **Main thread:** Application/API calls
- **Audio thread:** miniaudio real-time callback (strict timing)
- **Network thread:** POCO UDP reactor for CryptoDTO receive
- **HTTP thread:** Async transfer manager

### Audio Pipeline Pattern

Audio flows through a pull-based chain: `AudioDevice` pulls from `OutputMixer`, which pulls from multiple `RemoteVoiceSource` instances (each with Opus decoding + jitter buffer), through `VHFFilterSource` (radio effects/crackle), `BiQuadFilter` (EQ), and `SimpleCompressorEffect`. Microphone input goes through `SpeexPreprocessor` (noise suppression/AGC) into `VoiceCompressionSink` (Opus encoding) and out via `UDPChannel`.

## Dependencies (vcpkg)

opus, speexdsp, openssl, curl, nlohmann-json, cpp-jwt, msgpack (with boost), poco (Foundation/Net/Util), sdl2 (test client only). miniaudio is vendored in `include/afv-native/audio/miniaudio.h`.

## Build System Notes

- `CMAKE_COMPILE_WARNING_AS_ERROR` is ON — all warnings are errors
- MSVC requires `_USE_MATH_DEFINES` (set automatically via CMakeLists.txt)
- macOS links CoreAudio, AudioToolbox, AudioUnit system frameworks
- Debug builds get "d" suffix on library name
- Post-build Release step runs `compile_licenses.py` to generate `ALL_LICENSES.txt`
- Export macro `AFV_NATIVE_API` is auto-generated via `GenerateExportHeader`
- speexdsp uses `find_library` instead of `find_package` (no cmake config available)
