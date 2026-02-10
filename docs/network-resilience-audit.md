# Network Resilience Audit — afv-native

**Date:** 2026-02-10
**Branch:** `develop-trackaudio`
**Scope:** UDP connection lifecycle, heartbeat logic, error handling, disconnect/reconnect paths

---

## Context

This library connects to remote VATSIM voice servers via encrypted UDP (ChaCha20-Poly1305). Users connect from around the world to servers located in a single country, meaning high-latency, lossy links are the norm. Small transient packet loss or latency spikes were causing full disconnections — sometimes silently.

---

## Connection Lifecycle (before changes)

```
ATCClient::connect()
  → APISession::Connect()              [HTTP auth via CURL]
    → sessionStateCallback(Running)
      → VoiceSession::Connect()         [HTTP POST for callsign]
        → setupSession(PostCallsignResponse)
          → UDPChannel::open()          [Poco DatagramSocket, connected UDP]
          → Register heartbeat handler ("HA")
          → Start mHeartbeatTimer       (3 000 ms interval)
          → Start mHeartbeatTimeout     (20 000 ms one-shot)
          → StateCallback → Connected

Steady state:
  TX: mHeartbeatTimer fires → sendDto(Heartbeat) → re-arm timer
  RX: readCallback → Decapsulate → "HA" handler → receivedHeartbeat()
      → reset mHeartbeatTimeout to 20 000 ms

Disconnect triggers:
  A) mHeartbeatTimeout fires → heartbeatTimedOut() → Disconnect(true, reconnect=true)
  B) UDPChannel::errorCallback (ECONNRESET/ECONNREFUSED) → fatal → close()
  C) APISession Disconnected/Error → failSession()
```

---

## Issues Found & Fixes Implemented

### Issue 1 — Hard 20 s heartbeat timeout with no graduated response

**Severity:** Critical
**Files:** `include/afv-native/afv/params.h`, `include/afv-native/afv/VoiceSession.h`, `src/afv/VoiceSession.cpp`

**Problem:** A single one-shot 20 s timer was the sole disconnect mechanism. No tracking of heartbeat success ratio, no warning before disconnect. On a 300 ms+ RTT link with 5–10 % packet loss, losing ~6 consecutive heartbeat responses (entirely plausible with bursty loss) triggered a full disconnect.

**Fix:** Replaced the hard timeout with a counted-miss system:
- Each heartbeat send increments `mConsecutiveMissedHeartbeats`
- Each heartbeat response resets the counter to 0
- At 5 consecutive misses (15 s), a `VoiceSessionState::Degraded` callback warns the caller
- At 10 consecutive misses (30 s), the session disconnects
- Recovery from degraded state re-fires `VoiceSessionState::Connected`
- Added `afvMaxConsecutiveMissedHeartbeats = 10` constant

### Issue 2 — `sendDto` silently swallowed all send failures

**Severity:** Medium
**Files:** `include/afv-native/cryptodto/UDPChannel.h`

**Problem:** `sendDto` returned `void` and caught all exceptions silently. If the socket was broken, heartbeat sends failed without anyone knowing. The session only discovered the problem when the 20 s timeout fired.

**Fix:** Changed `sendDto` return type from `void` to `bool`. All error paths return `false`, successful sends return `true`. Also fixed a signed/unsigned comparison warning in the short-write check. Additionally, `sendHeartbeatCallback` now checks `mChannel.isOpen()` and calls `failSession()` if the channel is unexpectedly closed.

### Issue 3 — Single ICMP error caused immediate disconnect

**Severity:** Critical
**Files:** `include/afv-native/cryptodto/UDPChannel.h`, `src/cryptodto/UDPChannel.cpp`

**Problem:** `errorCallback` treated any `ECONNRESET`, `ECONNREFUSED`, `ECONNABORTED`, or `ENOTCONN` as immediately fatal — closing the socket on a single error. On lossy international links, transient ICMP "port unreachable" or "host unreachable" from middleboxes are common and benign.

**Fix:** Added a threshold system: 3 errors within 10 seconds required before treating as fatal.
- `mConsecutiveFatalErrors` and `mFirstFatalErrorTime` track error bursts
- Errors spread over > 10 s reset the window
- Successful packet receipt (in `readCallback`) resets the counter to 0
- Non-fatal errors still reported via callback with `fatal=false`

### Issue 4 — Audio traffic didn't reset heartbeat timeout

**Severity:** High
**Files:** `include/afv-native/cryptodto/UDPChannel.h`, `src/cryptodto/UDPChannel.cpp`, `src/afv/VoiceSession.cpp`

**Problem:** Only heartbeat responses (`"HA"` DTO) reset the liveness timeout. Audio packets (`AudioRxOnTransceivers`, `AudioOnDirect`) were proof that the server and link were alive, but didn't prevent the timeout from firing.

**Fix:** Added a generic `mPacketReceivedCallback` to `UDPChannel` that fires on every successfully decrypted and dispatched packet. `VoiceSession` registers this callback to reset `mLastHeartbeatReceived` and restart the heartbeat timeout timer on any valid packet.

### Issue 5 — Receive sequence window too small (10 packets)

**Severity:** Medium
**Files:** `include/afv-native/cryptodto/UDPChannel.h`

**Problem:** The `SequenceTest` sliding window was only 10 packets wide. Burst loss exceeding 10 sequence numbers caused `Overflow` — permanently discarding intermediate packets and causing unnecessary audio dropouts.

**Fix:** Increased default `receiveSequenceHistorySize` from 10 to 64 (the maximum supported by the `uint64_t` bitfield). Zero runtime overhead, dramatically better burst-loss tolerance.

### Issue 6 — No "degraded" connection state for caller

**Severity:** Medium
**Files:** `include/afv-native/afv/VoiceSession.h`, `include/afv-native/event.h`, `src/core/atcClient.cpp`, `src/core/Client.cpp`, `src/core/atisClient.cpp`

**Problem:** `VoiceSessionState` had only `Connected`, `Disconnected`, and `Error`. The caller never knew the connection was struggling until it was already dead.

**Fix:** Added `VoiceSessionState::Degraded` to the enum. Added `VoiceServerConnectionDegradedEvent` and `VoiceServerConnectionResumedEvent` event structs, and `ClientEventType::VoiceServerConnectionDegraded` / `ClientEventType::VoiceServerConnectionResumed` enum values. All three client types (`ATCClient`, `Client`, `ATISClient`) now handle the `Degraded` case in their voice state callback.

### Issue 7 — `mIsOpen` data race

**Severity:** Medium
**Files:** `include/afv-native/cryptodto/UDPChannel.h`

**Problem:** `mIsOpen` was a plain `bool` written by `open()`/`close()` (libevent thread) and read by `isOpen()`/`sendDto` (audio thread, other threads). This was a data race.

**Fix:** Changed to `std::atomic<bool> mIsOpen{false}`. The `<atomic>` header was already included.

---

## Files Changed

| File | Changes |
|------|---------|
| `include/afv-native/cryptodto/UDPChannel.h` | `mIsOpen` → atomic; sequence window 10→64; `sendDto` returns bool; added `mPacketReceivedCallback`, `mConsecutiveFatalErrors`, `mFirstFatalErrorTime`; added `registerPacketReceivedCallback()` |
| `src/cryptodto/UDPChannel.cpp` | `errorCallback` requires 3 errors in 10 s; `readCallback` resets error counters and fires packet callback; `close()` clears packet callback; added `registerPacketReceivedCallback` impl |
| `include/afv-native/afv/params.h` | Added `afvMaxConsecutiveMissedHeartbeats = 10` |
| `include/afv-native/afv/VoiceSession.h` | Added `Degraded` state; added `mConsecutiveMissedHeartbeats`, `mDegradedNotified` members; added `onPacketReceived()` |
| `src/afv/VoiceSession.cpp` | Graduated heartbeat tracking; degraded state notification and recovery; channel-closed detection in heartbeat send; packet-received callback registration; `onPacketReceived()` impl |
| `include/afv-native/event.h` | Added `VoiceServerConnectionDegradedEvent`, `VoiceServerConnectionResumedEvent` structs; added corresponding `ClientEventType` entries |
| `src/core/atcClient.cpp` | Handle `VoiceSessionState::Degraded` in `voiceStateCallback` |
| `src/core/Client.cpp` | Handle `VoiceSessionState::Degraded` in `voiceStateCallback` |
| `src/core/atisClient.cpp` | Handle `VoiceSessionState::Degraded` in `voiceStateCallback` |

---

## Not Implemented (Future Work)

### Fast reconnect path
Currently, any disconnect triggers full teardown + re-authentication. A "fast reconnect" that just reopens the UDP channel and re-registers the callsign (skipping full re-auth) would reduce reconnection time from seconds to milliseconds. This is a larger architectural change and should be done as a follow-up.

### Adaptive heartbeat timeout
The heartbeat interval and miss threshold are currently fixed constants. An adaptive system that adjusts based on observed RTT and jitter would be more robust across varying network conditions. This could be built on top of the current counted-miss system by dynamically adjusting `afvMaxConsecutiveMissedHeartbeats`.

---

## Build Verification

All changes compile cleanly with `-Werror` enabled. Build: 32/32 targets, 0 errors, 0 warnings.
