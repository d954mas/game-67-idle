#ifndef NT_SKELETAL_MESH_H
#define NT_SKELETAL_MESH_H

/* EXPERIMENTAL: CPU mesh proof only; no renderer or lifecycle guarantee. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nt_skeletal_mesh nt_skeletal_mesh_t;
typedef struct nt_skeletal_mesh_instance nt_skeletal_mesh_instance_t;

typedef struct nt_skeletal_mesh_vertex {
    float position[3];
    float normal[3];
    float uv[2];
    uint16_t joints[4];
    float weights[4];
} nt_skeletal_mesh_vertex_t;

typedef struct nt_skeletal_mesh_material_slot {
    const char *name;
    uint32_t index_offset;
    uint32_t index_count;
    int texture_id;
} nt_skeletal_mesh_material_slot_t;

typedef struct nt_skeletal_mesh_socket {
    const char *name;
    const char *joint_name;
    float local_offset[3];
    float local_rotation_quat[4];
    float local_scale[3];
} nt_skeletal_mesh_socket_t;

typedef struct nt_skeletal_mesh_desc {
    const nt_skeletal_mesh_vertex_t *vertices;
    uint32_t vertex_count;
    const uint32_t *indices;
    uint32_t index_count;
    const float *inverse_bind_matrices;
    const char *const *joint_names;
    uint32_t joint_count;
    const nt_skeletal_mesh_material_slot_t *material_slots;
    uint32_t material_slot_count;
    const nt_skeletal_mesh_socket_t *sockets;
    uint32_t socket_count;
    float asset_to_model[16];
} nt_skeletal_mesh_desc_t;

int nt_skeletal_mesh_create(const nt_skeletal_mesh_desc_t *desc,
                            nt_skeletal_mesh_t **out_mesh,
                            char *error,
                            size_t error_cap);

void nt_skeletal_mesh_destroy(nt_skeletal_mesh_t *mesh);

int nt_skeletal_mesh_instance_create(nt_skeletal_mesh_t *mesh,
                                     nt_skeletal_mesh_instance_t **out_instance,
                                     char *error,
                                     size_t error_cap);

void nt_skeletal_mesh_instance_destroy(nt_skeletal_mesh_instance_t *instance);

int nt_skeletal_mesh_instance_update_pose(nt_skeletal_mesh_instance_t *instance,
                                          const float *model_matrices,
                                          int matrix_count,
                                          char *error,
                                          size_t error_cap);

int nt_skeletal_mesh_instance_socket_matrix(const nt_skeletal_mesh_instance_t *instance,
                                            const char *socket_name,
                                            float *out_column_major_matrix_16,
                                            char *error,
                                            size_t error_cap);

uint32_t nt_skeletal_mesh_instance_skinned_position_count(const nt_skeletal_mesh_instance_t *instance);

int nt_skeletal_mesh_instance_copy_skinned_positions(const nt_skeletal_mesh_instance_t *instance,
                                                     float *out_xyz,
                                                     uint32_t max_positions,
                                                     char *error,
                                                     size_t error_cap);

#ifdef __cplusplus
}
#endif

#endif /* NT_SKELETAL_MESH_H */
