# Vendored miniaudio

- Upstream: https://github.com/mackron/miniaudio
- Version/tag: `0.11.25` (2026-03-04)
- License: public domain or MIT-0; this repository uses the included `LICENSE`.
- Integration: upstream `miniaudio.c` is compiled once through
  `src/audio_miniaudio_impl.c`; adapter TUs include `miniaudio.h` with the same
  `src/audio_miniaudio_config.h` feature macros.

Pinned file SHA-256 values:

```text
AC7AF4DE748B7E26B777F37E01CEE313A308A7296A3EB080E2906B320CC55C89  miniaudio.h
AB1984BB9804FFD7B0303813595D0B345A8A86C34DA1DAFFC353A14B34102A65  miniaudio.c
457F1B500E0ADF6BC059EDDDFA78A2F62012E7C3BB43476C20E0BD23B25BA0EB  LICENSE
```

The configured native surface keeps the engine/node graph and WAV+MP3
decoders, and disables the resource manager, encoding, FLAC, and generation.
Windows enables WASAPI+NULL; Linux enables ALSA+PulseAudio+NULL. NULL is a
probe fallback only and is reported as unavailable in production.
