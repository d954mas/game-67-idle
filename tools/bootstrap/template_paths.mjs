// Single source of truth for the template <-> game copy model.
//
//   TEMPLATE = UNIVERSAL (pipeline/skills/docs/knowledge) + SEED (a runnable,
//   game-agnostic base: pack-packing, a starter font, text + mesh rendering).
//
//   new_game     : copy UNIVERSAL + SEED + the engine submodule into a fresh game.
//   sync_to_template : copy UNIVERSAL (only) from a finished game back to the
//                      template, so reusable improvements propagate to the next game.
//
// SEED is copied into a new game once and then OWNED by that game (each game
// customizes its own main/build_packs/shaders) — it is NOT synced back. Only
// UNIVERSAL flows both ways. Everything matched by GAME_ONLY is never in the
// template (game logic, pulled assets, per-game tasks/design/tools).
//
// Keep this list authoritative; new_game.mjs, sync_to_template.mjs and
// export_base.mjs all read it.

// Reusable across every game — flows game -> template on sync.
export const UNIVERSAL = [
  // process + skills + docs + knowledge
  ".codex/skills",
  "AI_PIPELINE.md",
  "AI_PIPELINE_HISTORY.md",
  "docs/ai-pipeline",
  "gamedesign/README.md",
  "gamedesign/knowledge",
  "gamedesign/sources",
  "tasks/README.md",
  "tasks/guides",
  // the whole reusable tool tree (pipeline, asset library tooling, taskboard,
  // product gate, codegen, bootstrap) EXCEPT per-game tool dirs (see GAME_ONLY)
  "tools",
];

// A runnable, game-agnostic starting point. Copied into each new game, then the
// game owns and evolves its copy. Per the lead: the template ALREADY has the pack
// builder, and every new game starts with a font + text + mesh rendering.
export const SEED = [
  "CMakeLists.txt",
  "CMakePresets.json",
  "src/clean_seed_main.c", // generic: loads <game>.ntpack, shows text, ready for meshes
  "src/build_packs.c",     // generic pack builder (font + slug_text + mesh_inst + white texture)
  "src/game_audio.c", "src/game_audio.h",
  "src/game_devapi_ui.c", "src/game_devapi_ui.h",
  "src/game_storage.c", "src/game_storage.h",
  "src/devapi",
  "state",                 // game-state schema codegen source (seed schema)
  "assets/fonts",          // starter OFL font — every game has text from the start
  "assets/shaders",        // common/ + slug_text + sprite + mesh_inst (text + instanced mesh)
  "assets/meshes",         // a starter mesh (e.g. cube) so the mesh path is proven on copy
  ".gitignore",
  ".clang-format",
];

// The engine is a git submodule; new_game wires it rather than copying a checkout.
export const ENGINE_SUBMODULE = "external/neotolis-engine";

// Never part of the template — game-specific. Matched against repo-relative paths.
export const GAME_ONLY = [
  /(^|[\\/])gamedesign[\\/]projects[\\/]/, // per-game GDD/design
  /(^|[\\/])tasks[\\/](active|epics|archive)[\\/]/, // per-game work items
  /(^|[\\/])tools[\\/][a-z0-9]+(-[a-z0-9]+)*[\\/]/, // tools/<game-id>/ (reset removes them)
  /(^|[\\/])assets[\\/](source|catalog|licenses|previews|runtime|packs)[\\/]/, // pulled/built game assets
  /(^|[\\/])src[\\/](?!clean_seed_main|build_packs|game_audio|game_devapi_ui|game_storage|generated|devapi)/, // game src modules
  /\.ntpack$/, // built packs
];

// The library is shared and lives OUTSIDE any project (YandexDisk). Games and the
// template both reference it; it is never copied by new_game/sync.
export const SHARED_LIBRARY = "C:\\Users\\ROG\\YandexDisk\\gamedev\\assets\\ai_pipeline_assets";

export function isGameOnly(relPath) {
  const p = relPath.replace(/\\/g, "/");
  return GAME_ONLY.some((re) => re.test(p) || re.test(relPath));
}
