# rb-dark-rpg audio assets

Runtime MP3 assets for the current game audio cues.

All files in `music/` and `sfx/` are CC0-sourced derivatives normalized for the
web and native encoded-audio backend:

- MP3 container/codec
- 44100 Hz
- mono SFX, stereo music

Sources:

- Generated interface tones by OpenAI Codex for rb-dark-rpg, CC0:
  created locally with `ffmpeg aevalsrc` after sourced foley clicks tested too
  woody for UI/dialogue.
- `50 RPG sound effects` by Kenney, OpenGameArt, CC0:
  `https://opengameart.org/content/50-rpg-sound-effects`
- `Dark Forest Theme` by cynicmusic, OpenGameArt, CC0:
  `https://opengameart.org/content/dark-forest-theme`
- `RPG Sound Pack` by artisticdude, OpenGameArt, CC0:
  `https://opengameart.org/content/rpg-sound-pack`

Conversion command shape:

```powershell
ffmpeg -y -i <source> -filter:a loudnorm=I=-17:TP=-1.5:LRA=11 -ac 1 -ar 44100 -codec:a libmp3lame -q:a 4 <target>.mp3
```

Music uses `loudnorm=I=-16:TP=-1.5:LRA=11` and stereo output.
The committed music file is additionally attenuated with `volume=0.45`.

Detailed source mapping and hashes live in `manifest.json` and
`../packs/rb-dark-rpg-audio-cc0/assets.jsonl`.
