# Audio timing fixture

`write_ui_click_mp3.c` stores a text representation of an MP3 encoded from
`assets/audio/sfx/ui_click.wav`. The build materializes it outside the source
tree so the native decoder test can compare the compressed cue against its PCM
master without committing a duplicate binary blob.

Source: Kenney UI Audio, `Audio/click1.ogg`, CC0 1.0.

Transform:

```text
ffmpeg -hide_banner -loglevel error -y -i assets/audio/sfx/ui_click.wav -ac 1 -ar 44100 -codec:a libmp3lame -q:a 4 -map_metadata -1 -id3v2_version 0 -write_xing 1 ui_click.mp3
```

Materialized SHA-256:
`E81DEF031DF6F910E2E0508AAE062C1DC56E6DA62B6FA13AD9969E7AD65AED72`.
