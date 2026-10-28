#ifndef FEATURES_AUDIO_CAPTURE_H
#define FEATURES_AUDIO_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#if defined(AUDIO_CORE_CAPTURE_ENABLED) && AUDIO_CORE_CAPTURE_ENABLED
/* Native development capture. Main thread only; playback continues unchanged.
   PCM is interleaved float32 stereo at 48 kHz, after the master and bus gains.
   End quiesces the producer; drain the remaining PCM before closing the encoder.
   Overflow is sticky until the next begin and invalidates the whole take. */
bool audio_capture_begin(void);
bool audio_capture_end(void);
uint32_t audio_capture_read(float *stereo, uint32_t capacity_frames);
bool audio_capture_failed(void);
#endif

#endif
