# Dress art provenance

Shipping candidates are generated raster art owned by `web-dressup`. Raw
plates, prompts, references and immutable originals live in Canvas project
`canvas://runway-awakening-mvp-art-bbe99d`.

## Runway Awakening assets

### `body_base.png`

- origin: `ai`
- tool/model: Canvas recipe `grp_62500edb`, Codex image generation,
  `gpt-image-2`, high quality
- source element: `el_2561725b`; prepared Canvas element: `el_21bd0ba9`
- prompt: stored verbatim on the Canvas recipe and export manifest
- role: locked bald paper-doll base, front view, neutral pose
- preparation: flat-magenta source; chroma removal with the installed imagegen
  helper, 12/80 soft matte and despill
- license: studio-owned generated asset; commercial project use
- shipping export: Canvas Lanczos, `512w`, transparent PNG
- sha256: `636176f1666d3d50e6e188bc322f61a46868a7c375a6a70f7b1e7727498d06ea`

### `top_tee.png` (`Crescent Orbit Dress`, temporary research plate)

- origin: `ai`
- tool/model: Canvas worn recipe `grp_cd570898` plus garment-extraction recipe
  `grp_67e00f38`, Codex image generation, `gpt-image-2`, high quality
- worn element: `el_a19bc883`
- garment-only element: `el_54fa3f80`
- prepared Canvas element: `el_f30c81d4`
- prompt: stored verbatim on both Canvas recipes and export manifests
- role: Moon main-focus garment, registered to `body_base.png`
- preparation: doll removed by identity-preserving image edit; flat-magenta
  source keyed with the installed imagegen helper, 12/80 soft matte and despill
- license: studio-owned generated asset; commercial project use
- shipping export: Canvas Lanczos, `512w`, transparent PNG
- sha256: `7baa65cdaf51bba774592fe0e67b0217f2fdaad633c8175dd57acb28aa287f46`
- verdict: prototype-only; body-peel v2 rejects the semantic mask at support
  `0.813 < 0.850`, so this RGB must be replaced before release

### `top_hoodie.png` (`Verdant Halo Dress`, prototype-only after alpha re-audit)

- origin: `ai`
- source-first decision: the shared library searches for an emerald/cyan
  magical petal dress returned `0` matches, so project-owned generation was
  used
- tool/model: Codex image generation, `gpt-image-2`, high quality; exact worn
  and semantic-mask prompts are stored on Canvas recipe `grp_cfb0e869`
- Canvas project: `canvas://runway-awakening-cycle-4-bloom-flame-production-06e161`
- worn element: `el_793447ca`; binary mask element: `el_32742969`
- magenta plate: `el_4a57f985`; key-matte result: `el_1ed4f614`
- role: Bloom main-focus garment, registered to `body_base.png`
- preparation: locked-body worn edit; separate semantic mask; Canvas 2-color
  quantize and brightness bake to exact `{0,255}` mask; enclosed decorative
  holes filled without changing the external silhouette; `body_peel.py` v2
  strict `main` gate; Canvas `alpha --method matte`
- historical body-peel v2 evidence reported `support_ratio=0.9991`, but this
  acceptance is revoked: the opaque worn background made 88.73% of the frame a
  false RGB-diff seed, and the semantic mask admitted visible hand/arm pixels
- verdict: prototype-only until a body-peel v3 compatible locked pair and clean
  semantic mask pass without skin remnants
- visual proof: composited locked-body, dark-background and white-background
  proofs under `tmp/runway_art_cycle4/bloom/proofs_final/`
- license: studio-owned generated asset; commercial project use
- shipping export: Canvas source-scale transparent PNG, `512x896`
- sha256: `44b0cf0cf6e33879300e4698f8f375678c6b807b6240ddbebe967748106eaf65`

### `top_blazer.png` (`Solar Fang Armor Dress`, prototype-only after alpha re-audit)

- origin: `ai`
- source-first decision: the shared library searches for a coral/gold/black
  magical armor dress returned `0` matches, so project-owned generation was
  used
- tool/model: Codex image generation, `gpt-image-2`, high quality; exact worn
  and semantic-mask prompts are stored on Canvas recipe `grp_c275701f`
- Canvas project: `canvas://runway-awakening-cycle-4-bloom-flame-production-06e161`
- worn element: `el_01cf6eb2`; binary mask element: `el_006f5788`
- magenta plate: `el_075bde38`; key-matte result: `el_92254560`
- role: Flame main-focus garment, registered to `body_base.png`
- preparation: locked-body worn edit; separate semantic mask; Canvas 2-color
  quantize and brightness bake to exact `{0,255}` mask; `body_peel.py` v2
  strict `main` gate; Canvas `alpha --method matte`
- historical body-peel v2 evidence reported `support_ratio=1.0`, but this
  acceptance is revoked: the opaque worn background made 89.56% of the frame a
  false RGB-diff seed; the final matte also introduced extra alpha pixels
- verdict: prototype-only until regenerated inputs pass body-peel v3 and alpha
  component cleanup
- visual decision: the central solar frame is an intentional skin-revealing
  armor cutout; the accepted proof is the registered body composite, not the
  isolated layer silhouette
- visual proof: composited locked-body, dark-background and white-background
  proofs under `tmp/runway_art_cycle4/flame/proofs_final/`
- license: studio-owned generated asset; commercial project use
- shipping export: Canvas source-scale transparent PNG, `512x896`
- sha256: `23508c87cda14e97ef3325a3d2a67fbec9338c181e3f69563da509119e988dcc`

### `acc_hat.png` (`Verdant Bloom Crown`)

- origin: `ai`
- source-first decision: the shared AI Studio asset search for a magical floral
  crown/halo returned `0` matches. Reviewed CC0 crown and flower assets on
  [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Crown_icon_transparent.png)
  and [OpenGameArt](https://opengameart.org/content/flowers-0) were generic icons
  or pixel art and did not match the registered painterly Bloom capsule, so
  project-owned generation was used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; the exact generation
  prompt is stored on Canvas element `el_6edadd18`
- Canvas project:
  `canvas://runway-awakening-registered-accent-capsule-f7eab9`
- raw element: `el_6edadd18`; Canvas matte element: `el_0f997e79`; locked body
  reference: `el_f0f2f5e2`
- registered worn group: `grp_e2c33118`; semantic group: `grp_b1551531`
- role: Bloom accent, replacing the prototype red hat
- preparation: isolated raster crown generated on a flat magenta plate, cut
  non-destructively in Canvas, then registered at `x=126`, `y=-20`, `w=260`,
  `h=148` over the exact `512x896` locked body. Canvas rendered the worn and
  semantic plates; `body_peel.py` v3 produced the shipping RGBA directly. No
  post-peel chroma matte was applied
- body-peel v3 evidence: `alpha_mismatch_ratio=0.0`,
  `exposed_drift_ratio=0.0`, `support_ratio=1.0`, `rgb_preserved=true`,
  `semantic_px=7014`, `512x896`
- protected overlap evidence: alpha pixels in the face/eyes ROI
  `(205,92)-(307,180)` = `0`; shipping alpha bbox = `(160,15)-(354,92)`
- proofs: `tmp/runway_accent_pilot/proofs/on_body.png`, `on_dark.png`,
  `on_white.png`, and `checkerboard.png`; machine report:
  `tmp/runway_accent_pilot/body_peel_v3_report.json`
- license: studio-owned generated asset; commercial project use
- raw sha256: `f378a861dbe03e314028be63ae0954c88f7ffff858793b83af81669a3b0b23cc`
- worn sha256: `0dd2cbaf11562fafe31b5607930391f84f52afa7f2a052db825566d44efb191f`
- semantic sha256: `df85e9e0a1b356ca677052f7da94f1f1c0e5507f85258380ffd7144569206fc0`
- shipping sha256: `c0a0d7fdb54211acc4dfff45c34f2792ffd0f1d233865370a074e66703bed308`

### `acc_glasses.png` (`Crescent Astral Diadem`)

- origin: `ai`
- source-first decision: the shared AI Studio search returned `0` matches.
  Reviewed CC0 lunar material such as the
  [Crescent Moon UI icon](https://ideatogame.com/graphic/crescent-moon-celestial-icon-cdbd52)
  and [Wuxia token pack](https://orhstudio.itch.io/wuxia-tokens-icons) was
  generic UI or pixel art rather than a registered painterly fashion accessory,
  so project-owned generation was used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; exact prompt and refs
  are stored on Canvas element `el_68146656`
- Canvas project:
  `canvas://runway-awakening-registered-accent-capsule-f7eab9`
- raw element: `el_68146656`; Canvas matte: `el_fdb45c14`; locked body copy:
  `el_b7163763`; worn group: `grp_ef0a4515`; semantic group: `grp_9cca2217`
- role: Moon accent, replacing the prototype glasses
- preparation: isolated celestial raster generated on flat magenta, Canvas
  matte, then registered at `x=128`, `y=-46`, `w=255`, `h=200`. Canvas rendered
  exact `512x896` worn and semantic plates; `body_peel.py` v3 produced the
  shipping RGBA directly with no post-peel matte
- body-peel v3 evidence: `alpha_mismatch_ratio=0.0`,
  `exposed_drift_ratio=0.0`, `support_ratio=1.0`, `rgb_preserved=true`,
  `semantic_px=4892`
- protected overlap: face/eyes ROI `(205,92)-(307,180)` = `0` alpha pixels;
  shipping bbox = `(153,13)-(360,85)`
- proofs: `tmp/runway_accent_pilot/moon_proofs/`; report:
  `tmp/runway_accent_pilot/moon_body_peel_v3_report.json`
- license: studio-owned generated asset; commercial project use
- raw sha256: `fb9018ee0867a28917d6531d49c3dbb4e32fa32bec323348c04e1dd285871d3e`
- worn sha256: `22d3d9716aecd20d443cecf3d021c97b15e51e5d306b5308e96869c1d2f38fe2`
- semantic sha256: `8eefce6c1c77846db8c77f7eb58a9f3c00269c0e479778eec81739627d307cf6`
- shipping sha256: `19f346443a92c663e2d416dace376ba40711fd230ab4a86c8272400eb1ddd6c2`

### `acc_bag.png` (`Solar Fang Waist Sigil`)

- origin: `ai`
- source-first decision: the shared AI Studio search returned `0` matches.
  Reviewed CC0 fantasy icon packs, including the
  [Ultimate Fantasy RPG Icon Pack](https://gaspardani87.itch.io/fantasy-rpg-icons),
  did not contain a registered painterly black/gold/coral solar waist ornament,
  so project-owned generation was used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; exact prompt and refs
  are stored on Canvas element `el_ecce8d91`
- Canvas project:
  `canvas://runway-awakening-registered-accent-capsule-f7eab9`
- raw element: `el_ecce8d91`; Canvas matte: `el_1b294df9`; locked body copy:
  `el_e7e35310`; worn group: `grp_d9654140`; semantic group: `grp_eed97b2c`
- role: Flame accent, replacing the prototype bag
- preparation: isolated solar waist raster generated on flat magenta, Canvas
  matte, then registered at `x=159`, `y=270`, `w=200`, `h=185`. Two wider
  placements were rejected during on-body review because they crossed the arm
  silhouette. Canvas rendered exact `512x896` worn and semantic plates;
  `body_peel.py` v3 produced the shipping RGBA directly with no post-peel matte
- body-peel v3 evidence: `alpha_mismatch_ratio=0.0`,
  `exposed_drift_ratio=0.0`, `support_ratio=1.0`, `rgb_preserved=true`,
  `semantic_px=6715`
- protected overlap: face/eyes ROI `(205,92)-(307,180)` = `0` alpha pixels;
  shipping bbox = `(175,309)-(340,437)`
- proofs: `tmp/runway_accent_pilot/flame_proofs/`; report:
  `tmp/runway_accent_pilot/flame_body_peel_v3_report.json`
- license: studio-owned generated asset; commercial project use
- raw sha256: `0e732051a2d174bd004abf50791ccf846c2b3f631e376743be33f4ce63eef3b2`
- worn sha256: `f6194119344f99e7d9cd9fd96e9107fdd926d7b55ee439131344fbc6359a9b22`
- semantic sha256: `1fd7e9f68e3b0c02acab04500a3c9595740eb511a228a65df090e20b694b6f27`
- shipping sha256: `9626a1307176008da6aae21113ccd8977f907819b0dfb9661425ee58d2ed78ff`

### Registered magical accent completion

- origin: `ai`
- source-first decision: the AI Studio storage query for registered magical
  shoulder, choker and vine-waist fashion accessories returned `0` matches.
  Generic CC0 fantasy icons cannot match the locked paper-doll registration or
  the existing painterly Essence capsule, so project-owned generation was used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; no game file,
  `body_base.png`, prototype garment or private reference was sent to the
  generation service. The exact prompt, source-first note, disclosure, license
  and immutable source sheet live on Canvas element `el_f817ef3d`
- Canvas project:
  `canvas://runway-awakening-registered-accent-capsule-f7eab9`
- source preparation: three explicitly authored regions were sliced in Canvas.
  Flame and Moon routed cleanly through the Canvas key matte. Bloom's automatic
  router flagged soft detail, so `alpha-dual-generate` was run as required; its
  generated pair passed the numerical gate but its exported artifact
  `tmp/accents_registered/canvas_exports_dual/bloom_verdant_garland_alpha.png`
  retained an opaque black plate under visual inspection and was rejected. The
  earlier non-destructive matte (`el_05933538`) remained the visually clean
  candidate and is the accepted Bloom source
- registration: each isolated cut was placed locally over the locked
  `512x896` body, with the canonical face/eyes rectangle reserved for the doll.
  The registered worn and semantic plates pass `body_peel.py` v3; its direct
  RGBA is the shipping file. No post-peel matte or resize is applied
- common body-peel evidence: `support_ratio=1.0`,
  `alpha_mismatch_ratio=0.0`, `exposed_drift_ratio=0.0`,
  `rgb_preserved=true`; protected face/eyes ROI `(205,92)-(307,180)` has zero
  alpha pixels; no shipping layer touches a canvas edge and no alpha component
  smaller than five pixels survives
- proofs and reports:
  `tmp/accents_registered/<asset>/proofs/{on_body,on_dark,on_white,checkerboard}.png`,
  `tmp/accents_registered/<asset>/body_peel_v3_report.json`, and
  `tmp/accents_registered/summary.json`; runtime-order overlap proofs with
  representative full outfits live under
  `tmp/accents_registered/full_outfit_proofs/`
- license: studio-owned generated asset; commercial project use

| Shipping file | Label | Essence | Canvas slice / accepted matte / shipping | Registration `(x,y,w,h)` | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| `acc_scarf.png` | Ember Mantle | Flame | `el_95071ab3` / `el_49bde027` / `el_691ad4d4` | `141,90,230,257` | `2aec56d610d3adc6644f138402fb60c0b086c90aa96b85fc7cba776fb8828eea` |
| `acc_moon.png` | Orbit Veil | Moon | `el_b0f65a8f` / `el_23f3225c` / `el_d6792b05` | `116,132,280,178` | `04be5d00f6129b9651883cb8427d5f114a0a54ff24e9aecbf911109390266983` |
| `acc_bloom.png` | Verdant Garland | Bloom | `el_1a3bb29e` / `el_05933538` / `el_1e2eaa9e` | `106,370,300,164` | `a3cebb4c31f36379a61fc020c7a118cd9c0b003dfa18dc36550400c1a296c4d7` |

### Registered magical bottom capsule

- origin: `ai`
- source-first decision: the AI Studio asset-store query for registered
  magical-fashion bottoms returned `0` matches; reusable CC0 garments did not
  match the locked paper-doll proportions or this capsule's painterly finish,
  so project-owned generation was used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; the locked
  `body_base.png` was supplied as style/proportion reference under the lead's
  explicit external-generation permission
- Canvas project:
  `canvas://runway-awakening-registered-bottom-capsule-ca7b80`
- source sheet: `el_86cad0eb`; locked body reference: `el_2edffe80`; full
  prompt, source-first note, reference, license and immutable original are
  stored on the source element
- preparation: the accepted six source-sheet regions were sliced and keyed
  non-destructively in Canvas. Each isolated cut was registered over the exact
  `512x896` locked body. The locked hand/forearm silhouette is carved from the
  bottom layer only in the side hand-height band so hands render in front of
  wide skirts without copying body pixels into the garment. The resulting worn
  and semantic plates pass `body_peel.py` v3; its direct RGBA is the shipping
  file. Disconnected alpha dust below eight pixels is removed before the worn
  plate is authored; the final six files have zero `<8 px` alpha components
  and touch no canvas edge. No post-peel matte or resize is applied
- common body-peel evidence: `alpha_mismatch_ratio=0.0`,
  `exposed_drift_ratio=0.0`, `rgb_preserved=true`; all source art contains
  clothing only, with no skin, mannequin or body remnants
- proofs and machine reports:
  `tmp/bottoms_registered/<asset>/proofs/{on_body,on_dark,on_white,checkerboard}.png`
  and `tmp/bottoms_registered/<asset>/body_peel_v3_report.json`
- license: studio-owned generated asset; commercial project use

| Shipping file | Label | Canvas slice / matte / shipping | Registration `(x,y,w,h)` | v3 support | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| `bot_jeans.png` | Celestial Tide Trousers | `el_4ee98a1d` / `el_8a98c9da` / `el_bbbab471` | `73,370,328,540` | `1.0` | `ad34d1a424e487a584490bd3b99fc90957d7618872a7a3cfe2a575dbc9644659` |
| `bot_skirt.png` | Verdant Petal Skirt | `el_a649e057` / `el_59031fe8` / `el_dd7c6318` | `90,360,374,360` | `1.0` | `5292ee1dc315f14d786fd5849fe2b192b2599e7cd289a2ef79c6ef39d1c114a4` |
| `bot_shorts.png` | Astral Comet Shorts | `el_c673a09c` / `el_b70132b4` / `el_08ff955e` | `104,370,285,360` | `1.0` | `67db6ad8ff65569f012403fd04017e32a1113ae49bceff677f389a3728ed56da` |
| `bot_cargo.png` | Solar Fang Cargo | `el_a022c9d8` / `el_a9362740` / `el_1d26737a` | `98,370,400,525` | `1.0` | `e80a8b5fb948e591bef42da28de8f1efb25c4e6862d66e4e3ec4e3540bd6ec54` |
| `bot_moonveil.png` | Moonveil Skirt | `el_b59e0f55` / `el_20b1a6a9` / `el_638b8d72` | `91,360,328,444` | `1.0` | `076a0ce2ff353bee8a1f9678861ce344563b5df7ff669f1508ecbafd78e70ff4` |
| `bot_phoenix.png` | Phoenix Regalia Skirt | `el_a0cb7779` / `el_b97da1a0` / `el_2eb57ebd` | `99,365,360,358` | `1.0` | `22814c0f26f98c05382048875a56994666fdcd0d40d44a5cb8942f5e7f72ff65` |

### Registered magical shoe capsule

- origin: `ai`
- source-first decision: the AI Studio asset-store query for registered
  magical-fashion shoes returned `0` matches. The free CC0 search found generic
  catalog pages and assets such as the pixel-art
  [Wuxia Footwear Icons](https://orhstudio.itch.io/wuxia-footwear-icons) and
  Kenney's 3D [Blocky Characters](https://kenney.nl/assets/blocky-characters),
  but no front-view painterly footwear layers matching this paper-doll pose and
  proportions. Project-owned generation was therefore used
- tool/model: OpenAI built-in `image_gen`, `gpt-image-2`; no game file or
  `body_base.png` was sent to the generation service. The exact prompt, source
  decision, license and immutable source sheet are stored on Canvas element
  `el_fb3fbb2f`
- Canvas project:
  `canvas://runway-awakening-registered-shoe-capsule-2780ef`
- preparation: one six-pair source sheet was manually regioned, sliced and
  keyed through journaled Canvas operations. Each pair was split into its left
  and right shoe locally, dust components below eight pixels were removed, and
  the halves were registered over the locked `512x896` body without uploading
  that body. The local worn and semantic plates pass `body_peel.py` v3; its
  direct RGBA output is the shipping file. Final transparent files, QA metrics,
  hashes and registration metadata were added back to Canvas without the body
- common body-peel evidence: `support_ratio=1.0`,
  `alpha_mismatch_ratio=0.0`, `exposed_drift_ratio=0.0`,
  `rgb_preserved=true`; protected face/eyes ROI `(205,92)-(307,180)` has zero
  alpha pixels; no shipping layer touches a canvas edge
- proofs and machine reports:
  `tmp/shoes_registered/<asset>/proofs/{on_body,on_dark,on_white,checkerboard}.png`,
  `tmp/shoes_registered/<asset>/body_peel_v3_report.json`, and
  `tmp/shoes_registered/summary.json`
- license: studio-owned generated asset; commercial project use

| Shipping file | Label | Canvas slice / matte / shipping | Registration left; right `(x,y,w,h)` | v3 support | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| `shoe_sneak.png` | Lunar High-Tops | `el_97193aa7` / `el_9c1b0e7b` / `el_9bb5e6a8` | `202,806,60,86`; `250,804,60,88` | `1.0` | `24e648709a13c6efae18e785689234e695ac359303f5a74914bfb7f88111db18` |
| `shoe_boot.png` | Verdant Vine Boots | `el_316ed146` / `el_21905d3d` / `el_cf41ae5d` | `205,756,54,136`; `253,755,54,137` | `1.0` | `cd8f24ef4c5d8987f570ad16e9b8e87396d37e714da61cad200e8b74c6dc7fbf` |
| `shoe_heel.png` | Solar Spike Heels | `el_27ae73d8` / `el_fa2673fc` / `el_eed1dc24` | `206,805,52,87`; `254,807,52,85` | `1.0` | `e4c99db9fff2752cea1c2956fd0d12b7b1b7c65c8797f73b8dbc54ba276afa53` |
| `shoe_sandal.png` | Crystal Orbit Sandals | `el_f7f74e4c` / `el_96cdf5ee` / `el_9af9d19e` | `207,797,50,95`; `255,797,50,95` | `1.0` | `deb1be514cf42f772b823b6eb00a15a2b174a6ba6e2baa7f28bae257aff28be3` |
| `shoe_eclipse.png` | Eclipse Platforms | `el_d7f73cfa` / `el_c06b7c61` / `el_496e6993` | `203,782,58,110`; `251,780,58,112` | `1.0` | `3046d4750296febeeb70e7542938e0504f8a99b6309cb8f1a663e7301dbdc798` |
| `shoe_phoenix.png` | Phoenix Wing Boots | `el_80f86d00` / `el_aa74f9e7` / `el_e463af55` | `205,801,54,91`; `253,797,54,95` | `1.0` | `59d9e3cc28ff4cc42171b2f1f3c1bcab606fa5e03a192e5992031f1f4601ba47` |

## Runtime contract

- Body and garment layers share the same front-view aspect and registration.
- Garment RGB must be copied byte-for-byte from the locked worn plate. Generated
  doll-removal art is semantic-mask guidance only, never shipping RGB.
- `body_peel.py` v3 requires an explicit mask channel, rejects alpha/background
  mismatch and exposed-body drift, and its direct RGBA output is the shipping
  candidate. The magenta plate is a diagnostic only; never pass accepted output
  through a second chroma-matte step because that changes RGB and grows alpha.
- Shipping thumbnails use the garment layer or runtime body+layer composition;
  new `_full` duplicate PNGs are forbidden.
- Every additional accepted garment must record its Canvas recipe/element,
  preparation route, license and SHA-256 here before release.

## Legacy quarantine

All other dress PNGs are 2026-07-09 prototype fixtures from the earlier
xAI/Grok worn-peel experiment. They are useful only until their magical
replacements land and are not approved for the Poki release. The old pink room,
ordinary streetwear and `_full` images must not be used as final-product proof.
