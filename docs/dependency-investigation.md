# Dependency Investigation: libevent vs POCO

## Context

The original audit recommended removing POCO and keeping libevent. After discussion, it became clear that:
- **POCO was chosen deliberately** -- libevent's C API is older and less clean, POCO provides modern C++ abstractions
- **The `sleep(10ms)` busy-wait loop** in `atcClientWrapper.cpp` is an intentional workaround because `event_base_dispatch()` deadlocks on some Windows machines
- **libevent is the weaker dependency**, not POCO

This document investigates both removal options and their trade-offs.

---

## Current State: Two Overlapping Async Systems

| Concern | libevent | POCO |
|---------|----------|------|
| **Event loop** | `event_base_loop()` in wrapper (busy-wait + sleep) | `Poco::Net::SocketReactor` in UDPChannel |
| **Timers** | `EventCallbackTimer` (3 timers) | Not used for timers yet |
| **HTTP** | `EventTransferManager` (curl_multi socket callbacks) | Not used for HTTP |
| **UDP voice** | Not used | `DatagramSocket` + `SocketReactor` |

Both run their own threads. The libevent loop busy-waits with a 10ms sleep; the POCO reactor blocks properly.

---

## Option A: Remove libevent, consolidate on POCO

### What libevent does today (complete touchpoint map)

**Timers (3 instances):**
- `ATCRadioSimulation::mMaintenanceTimer` -- 30s cleanup of stale voice streams
- `ATCRadioSimulation::mVoiceTimeoutTimer` -- 2s heartbeat monitoring
- `ATCClient::mTransceiverUpdateTimer` -- deferred transceiver updates (0ms = next iteration)

All use `EventCallbackTimer` -> `EventTimer` -> `event_new(-1, 0, cb)` + `event_add(timeout)`

**HTTP (curl_multi integration):**
- `EventTransferManager` wraps curl_multi with libevent socket callbacks
- Pattern: curl tells libevent which sockets to watch, libevent calls back when ready, which drives `curl_multi_socket_action()`
- This is the most complex integration (~242 lines)

**Event loop:**
- `atcClientWrapper.cpp` creates `event_base_new()`, runs `event_base_loop(EVLOOP_NONBLOCK)` in a thread with 10ms sleep between iterations
- The sleep is a Windows deadlock workaround

### Migration path

**Timers -> `Poco::Timer`:**
- `Poco::Timer` supports one-shot and periodic callbacks
- Replace `EventCallbackTimer` with a thin wrapper around `Poco::Timer`
- 3 call sites to update
- **Effort: S**

**HTTP -> `curl_multi_poll()`:**
- `curl_multi_poll()` (available since libcurl 7.68, Dec 2019) replaces the entire socket monitoring dance
- Instead of: curl -> callback -> libevent socket event -> callback -> `curl_multi_socket_action()`
- Just: `curl_multi_perform()` + `curl_multi_poll(timeout)` in a loop
- Run this loop in a dedicated `std::thread` (replaces EventTransferManager's libevent integration)
- This is dramatically simpler -- the 242-line `EventTransferManager` shrinks to ~50 lines
- **Effort: M** (rewrite EventTransferManager, test all HTTP paths)

**Event loop -> removed:**
- With timers on `Poco::Timer` and HTTP on its own thread, the libevent event loop has nothing left to do
- `atcClientWrapper.cpp` no longer needs `event_base` or the sleep loop thread
- The busy-wait Windows workaround disappears entirely
- **Effort: S**

**`event_base*` parameter removal:**
- Remove from: `ATCClient`, `ATCRadioSimulation`, `APISession`, `VoiceSession`, `EventCallbackTimer`, `EventTimer`
- ~16 files affected (8 headers, 8 source files)
- **Effort: M** (mechanical but many files, also a public API change)

### Summary

| Component | Replacement | Effort | Risk |
|-----------|-------------|--------|------|
| Timers (3) | `Poco::Timer` | S | Low |
| HTTP (curl_multi) | `curl_multi_poll()` + `std::thread` | M | Medium -- must test all HTTP paths |
| Event loop | Removed entirely | S | Low |
| `event_base*` threading | Delete parameter from constructors | M | Medium -- public API breaking change |
| **Total** | | **M-L** | **Medium** |

### Pros
- Removes the Windows deadlock workaround entirely
- Eliminates the busy-wait loop (proper blocking)
- Consolidates on one async framework (POCO)
- Simpler HTTP integration via `curl_multi_poll()`
- Removes ~500 lines of libevent plumbing
- One fewer vcpkg dependency (libevent + pthreads on Unix)

### Cons
- Public API breaking change (`event_base*` removed from constructors)
- Must verify `curl_multi_poll()` availability in vcpkg's libcurl version
- Medium risk -- HTTP and timer reliability is critical

---

## Option B: Remove POCO, consolidate on libevent (original audit plan)

### What POCO does today

**UDP voice channel** (`UDPChannel.cpp/h`):
- `Poco::Net::DatagramSocket` -- UDP send/receive
- `Poco::Net::SocketReactor` -- async read/error notifications
- `Poco::Thread` -- dedicated reactor thread
- `Poco::Net::SocketAddress` -- address resolution
- `Poco::Exception` -- error handling

**Base64** (`base64.cpp`):
- `Poco::Base64Encoder` / `Poco::Base64Decoder`

### Migration path

**UDPChannel -> libevent:**
- Replace `DatagramSocket` with raw platform sockets + `event_new(EV_READ)`
- Replace `SocketReactor` + `Thread` with libevent socket monitoring on the main `event_base`
- Replace `SocketAddress` with `getaddrinfo()` + `sockaddr_in`
- Replace POCO exceptions with errno checks
- **Effort: M-L** (UDPChannel is the real-time voice data path)

**Base64 -> OpenSSL:**
- Replace with `EVP_EncodeBlock()` / `EVP_DecodeBlock()` (OpenSSL already a dependency)
- **Effort: S**

### Summary

| Component | Replacement | Effort | Risk |
|-----------|-------------|--------|------|
| UDP socket | Platform sockets + libevent | M-L | **High** -- real-time voice path |
| Socket reactor | libevent `EV_READ` events | M | High -- threading model change |
| Base64 | OpenSSL EVP | S | Low |
| **Total** | | **M-L** | **High** |

### Pros
- Removes POCO (4 heavy modules: Foundation, Net, NetSSL, Util)
- `netssl` feature is currently unused -- pure waste

### Cons
- **High risk** -- UDPChannel is the real-time voice data path
- Replaces proven C++ socket abstractions with C-level socket code
- libevent has the Windows deadlock issue that required the sleep workaround
- Would move from clean POCO reactor to libevent's older C API
- Doesn't solve the busy-wait event loop problem

---

## Recommendation

**Option A (remove libevent)** is the better path:
1. It removes the *weaker* dependency -- the one with the Windows workaround
2. `curl_multi_poll()` dramatically simplifies HTTP integration
3. The busy-wait loop disappears entirely
4. Lower risk -- POCO's UDP handling (the critical voice path) stays untouched
5. POCO is the more modern, C++-native dependency

The main caveat is the **public API breaking change** (removing `event_base*` from constructors). This needs coordination with downstream consumers.

---

## Verification

- Build on all 3 platforms (Linux, macOS, Windows) via CI
- Verify `curl_multi_poll()` works with vcpkg's libcurl version
- Test HTTP paths: authentication, token refresh, transceiver updates, station data fetches
- Test timer accuracy: maintenance timer, heartbeat timeout, transceiver update queuing
- Test the Windows deadlock scenario that motivated the sleep workaround -- confirm it's resolved
- Full voice integration test: connect, transmit, receive, reconnect
