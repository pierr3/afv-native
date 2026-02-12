# afv-native Comprehensive Audit Plan

## Context

afv-native is a production C++17 audio/networking library for ATC voice communication (VATSIM). It targets Linux (x64/ARM64), macOS (universal), and Windows (x64). The codebase is mature but accumulated technical debt — heavyweight/redundant dependencies, no automated tests, several bugs, and thread safety gaps. This audit identifies concrete, prioritized improvements across stability, code quality, dependencies, developer experience, and cross-platform support.

**This document is a reference audit only — no implementation at this time.**

---

## 1. Critical Fixes (Bugs & Correctness)

### 1.1 Dead code in PTT guard logic
- **File:** [atcClient.cpp:394-395](src/core/atcClient.cpp#L394-L395)
- **Bug:** `if (mTxUpdatePending) { if (!mTxUpdatePending) {` — inner condition is always false. The log message and `queueTransceiverUpdate()` never execute. When PTT is pressed during a pending transceiver update, the update is never re-queued.
- **Fix:** Remove the contradictory inner `if` or change the logic to match the intent (likely should be `if (mTxUpdatePending && !mRadiosInSync)` or similar)
- **Effort:** S

### 1.2 `throw new std::exception()` throws a pointer
- **File:** [MiniAudioDevice.cpp:56](src/audio/MiniAudioDevice.cpp#L56)
- **Bug:** `throw new std::exception()` allocates on the heap and throws a pointer. No catch site will match `std::exception*`, causing `std::terminate()` — an unconditional crash instead of recoverable error.
- **Fix:** Change to `throw std::runtime_error("Failed to initialize miniaudio context")`
- **Effort:** S

### 1.3 EventBus thread safety race condition
- **File:** [EventBus.h:92-94](include/afv-native/event/EventBus.h#L92-L94)
- **Bug:** `OnEvent()` calls `GetStream<T>().OnEvent(event)` without holding `mutex_`. `GetStream()` may modify the `streams_` map, and `EventStream::OnEvent()` iterates `handlers_` concurrently with `AddHandler`/`RemoveHandler` which do lock. Events are fired from audio thread, UDP reactor thread, and libevent thread simultaneously.
- **Fix:** Lock `mutex_` in `OnEvent()`, or use a shared_mutex (read lock for OnEvent, write lock for Add/Remove), or copy handlers under lock before iterating
- **Effort:** M — must avoid deadlocks with existing lock ordering

### 1.4 Unhandled error in APISession
- **File:** [APISession.cpp:124](src/afv/APISession.cpp#L124)
- **Issue:** `// FIXME: report error upstream` — errors silently swallowed
- **Fix:** Propagate via the existing `StateCallback` mechanism
- **Effort:** S

### 1.5 Raw `new[]` for UDPChannel receive buffer
- **File:** [UDPChannel.h:66](include/afv-native/cryptodto/UDPChannel.h#L66), [UDPChannel.cpp](src/cryptodto/UDPChannel.cpp)
- **Issue:** `unsigned char *mDatagramRxBuffer = new unsigned char[65536]` with manual `delete[]` in destructor. Not exception-safe.
- **Fix:** Replace with `std::vector<unsigned char>` or `std::unique_ptr<unsigned char[]>`
- **Effort:** S

---

## 2. Dependency Audit

### Current dependency map

| Dependency | Used in | Verdict |
|---|---|---|
| **opus** | RemoteVoiceSource, VoiceCompressionSink | **Keep** — core codec |
| **speexdsp** | SpeexPreprocessor, RemoteVoiceSource (jitter buffer) | **Keep** — lightweight, essential |
| **openssl** | Channel.cpp (ChaCha20-Poly1305) | **Keep** — protocol requirement |
| **curl** | TransferManager, EventTransferManager | **Keep** — deeply integrated with libevent multi |
| **libevent** | ATCClient, timers, HTTP integration, event loop | **Keep** — backbone of async architecture |
| **nlohmann-json** | APISession, VoiceSession, RESTRequest | **Keep** — header-only, zero issues |
| **msgpack** (+boost feature) | 15 DTO headers | **Keep but remove boost feature** (see 2.2) |
| **cpp-jwt** (vcpkg) | APISession | **Keep** |
| **cpp-jwt** (extern/) | Duplicate of vcpkg version | **Remove** vendored copy |
| **poco** (net, netssl, util) | UDPChannel.cpp, base64.cpp — only 2 files | **Remove** — massively overweight (see 2.1) |
| **sdl2** | Test client only | **Conditionalize** |
| **miniaudio** (vendored header) | MiniAudioDevice | **Keep** — single-header, properly vendored |
| **compressor, simpleSource** (extern/) | Audio DSP effects | **Keep** — tiny, no vcpkg equivalent |

### 2.1 Remove POCO (High Priority)
- **Why:** POCO pulls in Foundation + Net + NetSSL + Util (4 heavy modules) for just a UDP socket + reactor and base64 encoding. The `netssl` feature isn't even used anywhere. It's the single most impactful dependency removal.
- **Migration:**
  - **UDPChannel** ([UDPChannel.h](include/afv-native/cryptodto/UDPChannel.h), [UDPChannel.cpp](src/cryptodto/UDPChannel.cpp)): Replace `Poco::Net::DatagramSocket` + `Poco::Net::SocketReactor` + `Poco::Thread` with platform sockets integrated into libevent (already a dependency). Use `evutil_socket_t` + `event_new(EV_READ)` for async reads, matching the pattern in `EventTransferManager`. Replace `Poco::Net::SocketAddress` with `getaddrinfo()` + `sockaddr_in`. Replace POCO exception handling with errno checks.
  - **base64** ([base64.cpp](src/util/base64.cpp)): Replace `Poco::Base64Encoder`/`Poco::Base64Decoder` with OpenSSL's `EVP_EncodeBlock`/`EVP_DecodeBlock` (OpenSSL is already a dependency) or a small standalone implementation.
  - **CMakeLists.txt / vcpkg.json:** Remove `poco` entirely
- **Effort:** M-L
- **Risk:** UDPChannel is the real-time voice path — needs thorough testing under packet loss, reconnection, and load

### 2.2 Remove boost transitive dependency via msgpack
- **Why:** msgpack's `boost` feature flag pulls in several boost modules (system, config, smart_ptr, etc.). The codebase only uses `MSGPACK_DEFINE_MAP` macros which likely don't require boost.
- **Action:** Remove `"features": ["boost"]` from vcpkg.json, verify all 15 DTO headers still compile
- **Effort:** S (may be a one-line change if boost isn't actually needed)

### 2.3 Remove vendored cpp-jwt duplicate
- **Why:** `extern/cpp-jwt/` duplicates the vcpkg-provided copy. The `include_directories(extern/)` in CMakeLists.txt creates include path ambiguity.
- **Action:** Delete `extern/cpp-jwt/`, verify build uses vcpkg version
- **Effort:** S

### 2.4 Conditionalize sdl2
- **Why:** Only used by the test client, not the library itself.
- **Action:** Move to a separate vcpkg feature or test-only dependency
- **Effort:** S

---

## 3. Developer Experience

### 3.1 Add unit test framework (Catch2)
- **Why:** Zero automated tests is a critical gap for a production library. Many components are independently testable: `SequenceTest`, `Channel` encrypt/decrypt round-trip, `BiQuadFilter`, `base64`, DTO serialization, `RollingAverage`, audio generators.
- **Action:** Add Catch2 via vcpkg, create `tests/` directory, add `BUILD_TESTING` CMake option, write initial test suite targeting highest-value components
- **Priority test targets:**
  1. `SequenceTest` (packet ordering correctness)
  2. `Channel::Encapsulate`/`Decapsulate` round-trip (crypto correctness)
  3. `base64` encode/decode
  4. DTO msgpack serialization round-trips
  5. `BiQuadFilter` coefficient calculations
- **Effort:** M

### 3.2 Add CMake presets + compile_commands.json
- **Why:** Essential for IDE support (clangd, VS Code intellisense), static analysis, and developer onboarding.
- **Action:** Add `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` to CMakeLists.txt, create `CMakePresets.json` with debug/release/ASAN configurations
- **Effort:** S

### 3.3 Add clang-tidy configuration
- **Why:** Would have caught bugs like the PTT dead code (1.1) and `throw new` (1.2) automatically.
- **Action:** Create `.clang-tidy` with checks: `bugprone-*`, `modernize-*`, `performance-*`, `readability-magic-numbers`, `cppcoreguidelines-owning-memory`
- **Effort:** S (config creation), M (triaging initial warnings)

### 3.4 Add ASAN/UBSAN CI build
- **Why:** The codebase has raw pointers, manual memory management, and cross-thread access. Sanitizers catch buffer overflows, use-after-free, and UB that testing alone won't find.
- **Action:** Add a sanitizer CMake preset, add CI workflow step that builds and runs testclient under ASAN
- **Effort:** S

---

## 4. Code Quality Improvements

### 4.1 Replace raw `new`/`delete` with RAII throughout
- **Files:** `OutputDeviceState.cpp` (5 raw `new[]`), `VHFFilterSource.cpp`, `WavSampleStorage.cpp`, `SourceFrameSizeAdjuster.cpp`, `SinkFrameSizeAdjuster.cpp`, `SourceToSinkAdapter.cpp`, `atcClientWrapper.cpp` (7 instances), `atcClientFlat.cpp`
- **Action:** Replace with `std::vector`, `std::unique_ptr`, or `std::array` as appropriate
- **Effort:** M (many files, each change is small)

### 4.2 Extract magic numbers into named constants
- **Key locations:** `ATCRadioSimulation.cpp` (HF threshold `30000000`, crackle factors `350.0`/`0.00776652`/`3.7`, VU meter thresholds), `VHFFilterSource.cpp` (compressor params, filter frequencies), `RemoteVoiceSource.cpp` (500ms flush threshold)
- **Action:** Define named constants with documentation near usage
- **Effort:** M

### 4.3 Resolve BiQuadFilter TODOs
- **File:** [BiQuadFilter.cpp:50,67,84](src/audio/BiQuadFilter.cpp#L50)
- **Issue:** 3x `// TODO: should we square root this value?` — the current `pow(10, dbGain/40)` is correct for amplitude from dB.
- **Action:** Remove TODOs, add clarifying comment
- **Effort:** S

### 4.4 Fix FIXME: multi-voicestream per callsign
- **Files:** [ATCRadioSimulation.cpp:545](src/afv/ATCRadioSimulation.cpp#L545), [RadioSimulation.cpp:370](src/afv/RadioSimulation.cpp#L370)
- **Issue:** Stream map uses callsign as key — if one callsign sends multiple streams, only the last survives.
- **Action:** Change key to callsign+streamId composite, update cleanup logic
- **Effort:** M

### 4.5 Refactor `_process_radio()` (194 lines)
- **File:** [ATCRadioSimulation.cpp](src/afv/ATCRadioSimulation.cpp)
- **Action:** Decompose into `calculateRadioGains()`, `applyRadioEffects()`, `handleRxStateTransitions()`, `mixToOutput()`
- **Prerequisite:** Unit tests (3.1) should exist first so refactoring doesn't break audio output
- **Effort:** M

---

## 5. Longer-Term Architecture Improvements

### 5.1 Consolidate dual event systems
- **Issue:** Every event fires through BOTH `ChainedCallback<void(ClientEventType, void*, void*)>` (legacy, type-unsafe `void*` casts) AND `EventBus` (modern, typed). Dual dispatch is error-prone and doubles event code.
- **Action:** Gradually deprecate `ChainedCallback`, migrate flat C API to use `EventBus`, remove duplicate dispatch
- **Effort:** L

### 5.2 Fix wrapper busy-wait event loop
- **File:** [atcClientWrapper.cpp:74-83](src/atcClientWrapper.cpp#L74-L83)
- **Issue:** `while (!exit) { event_base_loop(NONBLOCK); sleep(10ms); }` wastes CPU. Also uses namespace-level globals (only one client instance per process).
- **Action:** Replace with `event_base_dispatch()` (blocks until events), move globals into instance
- **Effort:** M

### 5.3 Abstract libevent coupling
- **Issue:** `event_base*` propagates through the entire API — forces consumers to create and run a libevent loop.
- **Action:** Abstract behind an event loop interface, provide libevent as default. Consider for a v2.0.
- **Effort:** L

---

## Recommended Implementation Order

| Phase | Items | Scope |
|---|---|---|
| **Phase 1: Critical fixes** | 1.1 PTT bug, 1.2 throw-new, 1.4 APISession error, 1.5 RAII buffer | S — a few hours |
| **Phase 2: Dev foundation** | 3.1 Test framework, 3.2 CMake presets, 3.3 clang-tidy, 3.4 ASAN CI | M — a few days |
| **Phase 3: Dep cleanup** | 2.1 Remove POCO, 2.2 Remove boost from msgpack, 2.3 Remove vendored cpp-jwt, 2.4 Conditionalize sdl2 | M-L — 1-2 weeks |
| **Phase 4: Code quality** | 4.1 RAII throughout, 4.2 Magic numbers, 4.3 BiQuadFilter TODOs, 1.3 EventBus thread safety | M — 1 week |
| **Phase 5: Refactoring** | 4.4 Multi-voicestream FIXME, 4.5 Refactor _process_radio | M — 1 week |
| **Phase 6: Architecture** | 5.1 Event consolidation, 5.2 Wrapper cleanup, 5.3 libevent abstraction | L — ongoing |

---

## Verification

- **After critical fixes:** Build on all 3 platforms (CI), run testclient, verify PTT works during transceiver updates, verify miniaudio init failure is catchable
- **After test framework:** Run `ctest` in CI, verify all initial tests pass on all platforms
- **After POCO removal:** Full integration test — connect to voice server, transmit/receive audio, test reconnection under packet loss, verify no regressions on Linux/macOS/Windows
- **After RAII changes:** Run under ASAN, verify no leaks or use-after-free
- **After refactors:** A/B compare audio output before/after to verify no DSP changes
