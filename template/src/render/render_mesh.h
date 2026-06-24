#ifndef GAME_RENDER_MESH_H
#define GAME_RENDER_MESH_H

#include "graphics/nt_gfx.h"
#include "resource/nt_resource.h"
#include "world/world.h"

// Render system for instanced meshes. Owns BOTH the coloured material (mesh_inst)
// and the textured material (mesh_tex + u_texture), the mesh renderer + ECS, and a
// follow camera; draws the World's mesh entities. Separate from game logic — it
// only reads the World's state.
void render_mesh_init(nt_resource_t mesh_vs, nt_resource_t mesh_fs, nt_resource_t tex_vs, nt_resource_t tex_fs, nt_resource_t texture);
void render_mesh_spawn_player(World *w, nt_resource_t cube_mesh, const float color[4]); // coloured cube
void render_mesh_spawn_prop(World *w, nt_resource_t cube_mesh);                         // textured cube
void render_mesh_draw(World *w, nt_buffer_t frame_ubo);
void render_mesh_restore_gpu(void);

#endif /* GAME_RENDER_MESH_H */
