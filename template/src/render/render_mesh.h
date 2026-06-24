#ifndef GAME_RENDER_MESH_H
#define GAME_RENDER_MESH_H

#include "graphics/nt_gfx.h"
#include "resource/nt_resource.h"
#include "world/world.h"

// Render system for instanced coloured meshes. Owns the mesh material + the mesh
// renderer + the ECS components; draws the World's mesh entities with a follow
// camera. Separate from game logic — it only reads the World's state.
void render_mesh_init(nt_resource_t mesh_vs, nt_resource_t mesh_fs);
void render_mesh_spawn_player(World *w, nt_resource_t cube_mesh, const float color[4]);
void render_mesh_draw(World *w, nt_buffer_t frame_ubo);
void render_mesh_restore_gpu(void);

#endif /* GAME_RENDER_MESH_H */
