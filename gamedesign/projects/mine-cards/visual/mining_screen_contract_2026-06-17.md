# Mining Screen Contract 2026-06-17

Goal: Build the first native Mine Cards Mining screen with a real animated
KayKit/Ozz miner, visible idle rewards, node selection, and first upgrade
pressure.

Non-goal: Do not expand into combat, smithing, card runs, offline progression,
or additional skills while the first screen still fails visual/readability or
game-loop gates.

Proof: Native screenshots and DevAPI scenario evidence listed in
`visual/live_state_acceptance_matrix.json`, plus `node tools/ai.mjs gate` or
equivalent product/game-loop review before calling the slice done.

Stop condition: Any red visual, UI readability, teachability, or core-loop gate
blocks feature expansion unless the lead explicitly accepts that debt.

Likely files: `src/clean_seed_main.c`, `src/mine_cards_model_proof.*`,
`tools/assets/build_mine_cards_text_pack.c`, `CMakeLists.txt`,
`tasks/active/T0001-mine-cards-mining-v0-01-first-slice.md`, and this project
wiki folder.

Coordinate convention: Game-space UI and 2D gameplay coordinates are Y-up.
DevAPI screen bounds remain top-left and are converted at the boundary.
