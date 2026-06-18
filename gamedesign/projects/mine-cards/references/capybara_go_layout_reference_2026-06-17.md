# Capybara Go Layout Reference - 2026-06-17

Status: quick visual/layout reference after lead correction.

Sources checked:

- FriendlyGameDev Capybara Go review screenshot montage:
  `https://friendlygamedev.com/game-reviews/capybara-go/`
- AppGrowing Capybara Go UI screenshot collage:
  `https://appgrowing.net/blog/en/capybara/`
- IndoGamers Capybara Go character-card screenshot:
  `https://indogamers.com/mobile/45000/21102024/capybara-go-game-petualangan-roguelike-yang-lucu-dari-habby-kini-tersedia-di-android-dan-ios`

Observed screen grammar:

- The character/action area is a persistent top or central hero stage. The
  player always sees the avatar and what it is doing.
- Progression mechanics live below or around the hero as dense cards, tabs,
  upgrade choices, equipment, pets, or rewards.
- The bottom UI is not one big scenic illustration. It is a compact idle/RPG
  control surface with many future systems visible.
- Rewards and decisions are shown as immediate cards/prompts, not hidden in a
  long textual log.
- The screen reads as mobile-first: large hero, chunky cards, short labels,
  repeated upgrade slots, clear selected state.

Translation to Mine Cards:

- Use a top `Miner Action Stage`: fixed 3D miner position, current activity,
  progress, and the latest reward/callout.
- Use a lower `Idle Mechanics Board`: Mining selected, future activities
  visible as cards/tabs, first upgrade card, reward rows, and locked systems.
- Use a reduced Capybara Go-like structure, not the full reference scope:
  one fixed top action stage, then a dense lower board with many future idle
  mechanics visible as locked/preview slots. The player should see the miner on
  every main activity screen and understand what the character is currently
  doing.
- Keep Melvor as economy/progression reference, but use Capybara Go for the
  first-screen composition.
- Mine Cards PSDs remain visual identity input: dark RPG shell, top HUD,
  bottom tabs, chunky pixel labels, and fixed Minecraft-like hero composition.

Current build mismatch:

- `build/captures/mine_cards_melvor_shell_v001.png` still puts the character
  in a left panel and squeezes mechanics beside it.

Native baseline after correction:

- `build/captures/mine_cards_capybara_layout_v005.png` now uses the intended
  split: top miner action stage, lower activity/mechanics board, right upgrade
  card, bottom navigation.
- `build/captures/mine_cards_capybara_layout_v005_720x480.png` covers the
  smaller native-window stress pass for this layout.
- Current runtime asset proof uses framebuffer capture because generic Windows
  BitBlt capture can return a white OpenGL client area:
  `build/captures/mine_cards_runtime_framebuffer_v011.png` and
  `build/captures/mine_cards_runtime_framebuffer_v011_720x480.png`.

Remaining mismatch:

- The UI is still procedural runtime scaffold, not final generated/artist UI art
  from the Mine Cards PSD identity.
- The stage target is a placeholder sprite; it needs real stone/copper source
  art so the top stage reads as miner-versus-node, not just miner-plus-bar.
- The lower board shows future activities, but it is not yet a full mechanics
  hub with synergy states, reward prompts, and equipment/pet-style slots.
