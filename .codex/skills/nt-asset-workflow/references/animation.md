# Animation Clips

Humanoid skeletal clips for a game character: walk, jump, punch, wave, idle.

## Order

1. Library first: `node ai_studio/assets/catalog/search.mjs --query "<action> animation" --json`.
   Mixamo-skeleton packs cover most locomotion and combat basics.
2. A gap the library cannot fill (a specific action or gesture): generate it
   with Kimodo, then retarget onto the character.
3. Signature stylized moves (squash and stretch, exact gameplay timing, cycles
   that must loop seamlessly) stay hand-animated; Kimodo is realistic mocap-like
   motion and does not guarantee loops.

## Kimodo in three commands

```powershell
node ai_studio/assets/tools/model/kimodo/cli.mjs doctor
node ai_studio/assets/tools/model/kimodo/cli.mjs generate --prompt "A person jumps forward with both feet." --frames 90
node ai_studio/assets/tools/model/kimodo/cli.mjs retarget --motion <run_dir from generate> --character <character.glb> --map rgpoly --clip-name jump
```

- Prompts are plain English, one action per clip, 2-300 frames at 30 fps.
  Several `--prompt` flags run in one session; change `--seed` for another take.
- Open `preview.png` after generate and `sheet.png` after retarget before
  handing anything on: a prompt can come back as a different action.
- `metrics.json` reports foot slide and the largest per-frame bone rotation.
- A rig without a map needs `maps/<id>.json` first (bone pairs, legs, arms,
  hinges); both rigs must rest in a T-pose.
- Proportion fixes (big heads, wide bodies) go through `--offset Bone=x,y,z`.

Outputs stay under `tmp/ai_studio/assets/kimodo/`. A keeper enters the game
through asset intake with `origin=ai` and its `provenance.json`; the clip is also
bound by the character's own license. Setup, licenses and limits:
`ai_studio/assets/tools/model/kimodo/README.md`.
