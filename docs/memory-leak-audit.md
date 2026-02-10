# Memory Leak Audit — afv-native

**Date:** 2026-02-10
**Branch:** `develop-trackaudio`
**Scope:** Full codebase audit for memory leaks, with focus on a reported ~30 minute growth on Linux

---

## Summary

Found **4 confirmed memory leaks** and fixed all of them. The most impactful was the `VHFFilterSource` limiter leak — it accumulates proportionally to user voice traffic, which directly matches the "30 minutes into a session" symptom.

---

## Confirmed Leaks (Fixed)

### Leak 1 — `VHFFilterSource` never deletes `limiter`

**File:** `src/audio/VHFFilterSource.cpp:62-64`
**Impact:** Proportional to traffic volume. Leaks one `SimpleLimit` object per VHFFilterSource destroyed.

A `VHFFilterSource` is created for each incoming voice stream (per callsign, per frequency). Both `compressor` and `limiter` are heap-allocated in the constructor (`new SimpleComp()` and `new SimpleLimit()`), but only `compressor` was deleted in the destructor. `limiter` leaked every time.

On a busy frequency with ~50 unique callsigns over 30 minutes, this is 50+ leaked `SimpleLimit` objects (each with internal state buffers), accumulating steadily.

**Before:**
```cpp
VHFFilterSource::~VHFFilterSource() {
    delete compressor;
};
```

**After:**
```cpp
VHFFilterSource::~VHFFilterSource() {
    delete compressor;
    delete limiter;
};
```

---

### Leak 2 — `MiniAudioAudioDevice` empty destructor

**File:** `src/audio/MiniAudioDevice.cpp:60-61`
**Impact:** Leaks `ma_context` + `ma_device` structs if the shared_ptr is dropped without an explicit `close()` call.

The destructor was completely empty. All cleanup lived in `close()`, but `close()` was only called explicitly — not from the destructor. If any code path dropped the `shared_ptr<AudioDevice>` without calling `close()` first, the miniaudio context and devices leaked.

On Linux (PulseAudio/ALSA), a `ma_context` holds thread handles, pipe file descriptors, and audio server connections — several KB of OS resources per leak.

**Fix:** The destructor now calls `close()`, and `close()` is made idempotent (early-returns if already closed) to prevent double-uninit.

---

### Leak 3 — `initOutput`/`initInput` device leak on start failure

**File:** `src/audio/MiniAudioDevice.cpp:250-253, 296-299`
**Impact:** Leaks an initialized `ma_device` if `ma_device_start()` fails after `ma_device_init()` succeeds.

If the audio backend intermittently fails to start a device (common on Linux when PulseAudio is busy), the device was left initialized but `mOutputInitialized`/`mInputInitialized` stayed `false`. Subsequent calls to `initOutput()`/`initInput()` would skip the uninit guard and overwrite the struct, orphaning the previous device.

**Fix:** Added `ma_device_uninit()` calls on the start-failure path before returning `false`.

---

### Leak 4 — `throw new std::exception()` heap-allocated exceptions

**File:** `src/audio/MiniAudioDevice.cpp:35, 45, 56`
**Impact:** Small one-time leak per failed device init (not a steady leak).

The constructor used `throw new std::runtime_error(...)` and `throw new std::exception()` — allocating exceptions on the heap. The factory catch (`catch (std::exception &e)`) catches by reference and cannot catch a pointer, so these exceptions propagate uncaught and their heap memory is never freed.

**Fix:** Changed all three to throw by value: `throw std::runtime_error(...)`.

---

## Items Investigated and Cleared

| Component | Status | Notes |
|-----------|--------|-------|
| Incoming stream maps (`mHeadsetIncomingStreams`, `mSpeakerIncomingStreams`) | OK | Maintenance timer runs every 30 s, purges entries older than 60 s (`compressedSourceCacheTimeoutMs`). Working correctly. |
| Opus encoder/decoder lifecycle | OK | Properly paired create/destroy in constructors and destructors. |
| Jitter buffer lifecycle | OK | `jitter_buffer_init` paired with `jitter_buffer_destroy` in destructor. Destroy callback set to `::free` for packet data. |
| Speex preprocessor | OK | `speex_preprocess_state_init` paired with `speex_preprocess_state_destroy`. |
| EVP_CIPHER_CTX (OpenSSL) | OK | `EVP_CIPHER_CTX_new` freed on both success and abort paths via goto. |
| libevent timers (`event_new`) | OK | All paired with `event_del` + `event_free` in destructors. |
| CURL handles | OK | `curl_easy_init` cleaned in both destructor and `reset()`. |
| UDPChannel rx buffer | OK | `new[]` in constructor, `delete[]` in destructor. |
| OutputDeviceState buffers | OK | 5x `new[]` in constructor, 5x `delete[]` in destructor. |

---

## Files Changed

| File | Change |
|------|--------|
| `src/audio/MiniAudioDevice.cpp` | Destructor calls `close()`; `close()` made idempotent; `throw new` → `throw` (3 sites); `ma_device_uninit` on start failure (2 sites) |
| `src/audio/VHFFilterSource.cpp` | Added `delete limiter` in destructor |

---

## Build Verification

All changes compile cleanly with `-Werror`. Build: 4/4 recompiled targets, 0 errors.

---

## Recommendation for Further Investigation

If the leak persists after these fixes, the next step would be to run the application under Valgrind (Linux) or AddressSanitizer (`-fsanitize=address`) to get a precise allocation stack trace. The `TransferManager` raw-pointer ownership model (storing `Request*` without shared ownership) is a latent use-after-free risk but is unlikely to manifest as a steady memory growth.
