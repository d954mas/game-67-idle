#ifndef FEATURES_AUDIO_MINIAUDIO_CONFIG_H
#define FEATURES_AUDIO_MINIAUDIO_CONFIG_H

#define MA_NO_RESOURCE_MANAGER
#define MA_NO_ENCODING
#define MA_NO_FLAC
#define MA_NO_GENERATION

#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#if defined(_WIN32)
#define MA_ENABLE_WASAPI
#define MA_ENABLE_NULL
#elif defined(__linux__)
#define MA_ENABLE_ALSA
#define MA_ENABLE_PULSEAUDIO
#define MA_ENABLE_NULL
#else
#error "audio-core native miniaudio backend supports only Windows and Linux"
#endif

#endif
