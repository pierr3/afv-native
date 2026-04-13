// Single translation unit that compiles miniaudio's implementation.
// Feature flags must match those in MiniAudioDevice.h so the public API
// matches the compiled implementation.
#define MA_NO_WINMM
#define MA_NO_WEBAUDIO
#define MA_NO_NULL
#define MA_NO_CUSTOM
#define MA_NO_DSOUND
#define MA_NO_JACK
#define MINIAUDIO_IMPLEMENTATION
#include "afv-native/audio/miniaudio.h"
