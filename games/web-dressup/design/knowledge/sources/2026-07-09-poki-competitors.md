---
type: Source Note
title: Poki dress-up and fashion competitor research
description: Category map and competitive analysis of dress-up/fashion games on Poki for web-dressup.
tags: [research, poki, competitors, dress-up]
game_id: web-dressup
status: accepted
date: 2026-07-09
---

# Poki Competitors — Dress-up / Fashion (2026-07-09)

Sources: Poki category pages and game pages (`poki.com/en/dress-up`,
`/fashion`, individual `/en/g/...` pages), Vortella public post-mortem stats
(Poki Medium / secondary reports). Vote/like counts are public Poki UI snapshots
and change over time.

## 1. Category snapshot

- Poki lists **65+ free dress-up games**.
- Adjacent categories: Fashion, Beauty, Make-up, Games for Girls, Princess.
- Poki’s own taxonomy of dress-up play styles:
  1. **Mix and match outfits** (example: Vortella’s)
  2. **Makeover** (hair/makeup then clothes; example: Glam Girl)
  3. **Pick favorite / left-or-right** (example: Star Blogger)
- Themes called out by Poki: princess/fantasy, anime/K-pop, wedding, seasonal,
  animals/pets.
- All listed titles emphasize **browser + mobile**, no download.

## 2. Most popular dress-up (Poki “most popular” list)

| Rank (Poki list) | Game | Rating (approx) | Public engagement (approx) | Core loop |
|------------------|------|-----------------|----------------------------|-----------|
| 1 | **Vortella's Dress Up** | 4.3 (~1.1M votes) | ~909K likes | Freeplay → **real multiplayer** fashion shows / competition |
| 2 | **SnapStyle Dress Up** | 4.6 (~850K votes) | ~760K likes | Classic mix-and-match + **photo / decorate** |
| 3 | **Fashion Tour Simulator** | 4.4 (~29K votes) | ~25K likes | Travel/runway styling for judges (scripted) |
| 4 | **Fashion Legends** | 4.3 (~203K votes) | ~166K likes | **Runner + collect clothes** on catwalk + theme match |
| 5 | **Jane's Fashion Studio** | 4.3 (~64K votes) | ~54K likes | Boutique/story/challenge modes + social feed flavour |

Also frequently featured in category UI: Beauty Salon, Anime Dress Up,
Star Blogger, Glam Girl, Fashion Dress Up Star, Wonder High, Tictoc* fashion
series, KPop/Wedding/Mermaid themed one-shots.

## 3. Competitor dossiers

### A. Vortella's Dress Up (Devortel) — **#1 threat / different category**

- Poki copy: multiplayer dress-up; clothes, makeup, hair, accessories; compete
  in fashion shows; marketed as DTI-with-friends **exclusive on Poki**.
- Controls note on page: mouse drag / WASD to **move** (world/space presence).
- Release Dec 2024; still updated (Jun 2026 snapshot).
- Public success metrics (May 2025 reporting): ~16M plays/mo, ~460k DAU,
  ~12m40s avg session, large homepage tile runs, multiplayer+competition drove
  session length from ~6m freeplay toward ~12m.
- Product lesson from makers: kill text modals; freeplay first; Poki pushed MP
  competition as growth ladder.

**Implication for us:** Do **not** build real multiplayer. Own single-player
quality + Fake Show. Players who want live rivals already have Vortella.

### B. SnapStyle Dress Up (PlayCap) — **closest classic freeplay rival**

- Loop: hairstyles, makeup, clothes, accessories → **take a photo and decorate**.
- Highest rating among top-5 (~4.6).
- Pure creative sandbox + share/photo end beat.
- No multiplayer / no DTI competition framing.

**Implication:** M0 freeplay must feel at least this free. Photo/share is a
proven Poki end-beat; we can mirror with screenshot later.

### C. Fashion Legends (Jungle Tavern) — **hybrid runner**

- Walk/drag on a long catwalk; pick up clothes/gems; match assignment theme;
  upgrades, skins, themes.
- Combines dress-up with **endless runner skill**.
- Different fantasy: "collect while moving," not deep closet lab.

**Implication:** Validates **theme prompts** on Poki without multiplayer.
We do **not** need a runner unless we want that genre hybrid.

### D. Fashion Tour Simulator (Go Panda) — **scripted runway career**

- Assemble outfits/accessories, global runways, impress judges, ranks.
- Single-player progression / locations framing.
- Same studio mass-produces Tictoc fashion + "Funny *" makeover titles.

**Implication:** Fake judges + score already exist on Poki; quality bar is
often low—opportunity for cleaner 2D art + better freeplay.

### E. Jane's Fashion Studio (Orenji Spark) — **story + challenge boutique**

- Style multiple characters, Story Mode, Challenge Mode, in-game social feed
  (posts/compliments) as flavour.
- Single-player narrative dressing.

**Implication:** Soft social simulation without multiplayer is acceptable on
Poki. Our Fake Show is a tighter, less story-heavy version of that idea.

### F. Star Blogger: Left or Right (WeLoPlay) — **binary choice dresser**

- Infinite left/right outfit choices; hair/eyes/clothes; "grow the feed."
- Very low control complexity; high click rate.

**Implication:** Opposite of deep mix-and-match. We win on **combinatorial**
depth, not on binary taps.

### G. Long-tail makeover / theme dolls

Examples: Glam Girl, Beauty Salon, Diva Hair/Makeup, Wedding/Mermaid/KPop/
Princess/Tictoc series, Box Monster, Wonder High, Kigurumi, etc.

- Pattern: short session, fixed character, spa→dress sequence, seasonal skin.
- Strength: instant theme recognition in tile art.
- Weakness: little replay once catalog exhausted; often weak dual-layout craft.

## 4. Pattern matrix (what wins on Poki)

| Pattern | Who uses it | Session driver | Our stance |
|---------|-------------|----------------|------------|
| Live multiplayer fashion | Vortella | Social + competition | **Avoid** (locked) |
| Deep freeplay closet | SnapStyle, Vortella freeplay | Creative flow | **M0 core** |
| Photo / share end | SnapStyle | Completion + identity | M1–M3 polish |
| Theme prompt | Fashion Legends, Tour, Jane challenge | Directed creativity | **M1–M2** |
| Fake/scripted score | Tour, Jane, many makeovers | Dopamine without MP | **Fake Show** |
| Runner hybrid | Fashion Legends | Skill + collect | Out of scope v1 |
| Binary left/right | Star Blogger | Fast clicks | Not our core |
| Story spa makeover | Glam Girl, Tictoc*, Beauty Salon | One-shot narrative | Avoid as boot path |
| World movement to clothes | Vortella (move controls) | Presence | **No map v1** |

## 5. White space for web-dressup

Gaps in the Poki catalog relative to our locks:

1. **Single-player DTI-loop** that is *opt-in* (not always-on MP, not only
   scripted career levels).
2. **Serious dual-orientation freeplay lab** (many titles are phone-ish or
   desktop-ish, few feel excellent in both).
2. **No lobby wait** fake podium with clear restyle CTA.
4. **No gacha / no energy** honest freeplay (aligns with portal ads model).
5. Strong **2D layer craft** with maximalist mixing (DTI creative energy without
   3D clipping culture).

Not white space: "another cute makeover doll" or "Vortella clone with MP."

## 6. Positioning sentence (for pitch / listing later)

> Free 2D dress-up lab: mix endless looks, then optionally enter a Fashion Show
> with themed runway vibes — all solo, no waiting, works on phone and desktop.

## 7. Practical takeaways for build order

1. Beat **SnapStyle-class freeplay** first (M0).
2. Add **theme + Fake Show** to differentiate from pure dolls without fighting
   Vortella (M2).
3. Skip runner/map/MP until metrics demand a new lane.
4. Tile art and first frame must read as **fashion/dress-up** in category
   browsers (compete with princess/K-pop noise).
5. Measure against Poki norms: instant play, mobile, session length, return
   after content drops.

## 8. Watch list (re-check before submit)

- Vortella feature updates (codes, modes, chat).
- New Poki exclusives in Fashion / Dress-up.
- Fashion Legends / runner hybrids if they dominate tiles.
- Any new single-player "theme show" that occupies our white space.
