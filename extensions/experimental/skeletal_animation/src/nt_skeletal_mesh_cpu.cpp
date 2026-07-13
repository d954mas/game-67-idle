#include "skeletal_animation/nt_skeletal_mesh.h"

#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

struct nt_skeletal_mesh {
    std::vector<nt_skeletal_mesh_vertex_t> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> inverse_bind_matrices;
    std::vector<std::string> joint_name_storage;
    std::vector<const char *> joint_names;
    std::vector<nt_skeletal_mesh_material_slot_t> material_slots;
    std::vector<std::string> material_name_storage;
    std::vector<nt_skeletal_mesh_socket_t> sockets;
    std::vector<std::string> socket_name_storage;
    std::vector<std::string> socket_joint_name_storage;
    float asset_to_model[16];
};

struct nt_skeletal_mesh_instance {
    nt_skeletal_mesh_t *mesh = nullptr;
    std::vector<float> skinned_positions;
    std::vector<float> last_model_matrices;
    std::vector<float> skin_matrices;
    std::vector<float> socket_matrices;
    bool has_pose = false;
};

static void set_error(char *error, size_t error_cap, const char *fmt, ...) {
    if (error == nullptr || error_cap == 0U) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    (void)std::vsnprintf(error, error_cap, fmt, args);
    va_end(args);
    error[error_cap - 1U] = '\0';
}

static void mat4_identity(float out[16]) {
    std::memset(out, 0, sizeof(float) * 16U);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[15] = 1.0F;
}

static void mat4_mul(const float a[16], const float b[16], float out[16]) {
    float r[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof(r));
}

static void mat4_transform_point(const float m[16], const float p[3], float out[3]) {
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

static void mat4_from_trs(const float t[3], const float q[4], const float s[3], float out[16]) {
    const float len_sq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
    if (len_sq > 0.0F) {
        const float inv_len = 1.0F / std::sqrt(len_sq);
        x = q[0] * inv_len;
        y = q[1] * inv_len;
        z = q[2] * inv_len;
        w = q[3] * inv_len;
    }

    const float sx = s[0] != 0.0F ? s[0] : 1.0F;
    const float sy = s[1] != 0.0F ? s[1] : 1.0F;
    const float sz = s[2] != 0.0F ? s[2] : 1.0F;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    out[0] = (1.0F - 2.0F * (yy + zz)) * sx;
    out[1] = (2.0F * (xy + wz)) * sx;
    out[2] = (2.0F * (xz - wy)) * sx;
    out[3] = 0.0F;
    out[4] = (2.0F * (xy - wz)) * sy;
    out[5] = (1.0F - 2.0F * (xx + zz)) * sy;
    out[6] = (2.0F * (yz + wx)) * sy;
    out[7] = 0.0F;
    out[8] = (2.0F * (xz + wy)) * sz;
    out[9] = (2.0F * (yz - wx)) * sz;
    out[10] = (1.0F - 2.0F * (xx + yy)) * sz;
    out[11] = 0.0F;
    out[12] = t[0];
    out[13] = t[1];
    out[14] = t[2];
    out[15] = 1.0F;
}

static bool is_zero_matrix(const float m[16]) {
    float zero_sum = 0.0F;
    for (int i = 0; i < 16; ++i) {
        zero_sum += m[i] < 0.0F ? -m[i] : m[i];
    }
    return zero_sum == 0.0F;
}

static int find_joint_index(const nt_skeletal_mesh_t *mesh, const char *joint_name) {
    if (mesh == nullptr || joint_name == nullptr) {
        return -1;
    }
    for (uint32_t i = 0; i < mesh->joint_names.size(); ++i) {
        if (std::strcmp(mesh->joint_names[i], joint_name) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static bool validate_desc(const nt_skeletal_mesh_desc_t *desc, char *error, size_t error_cap) {
    if (desc == nullptr) {
        set_error(error, error_cap, "desc is null");
        return false;
    }
    if (desc->vertices == nullptr || desc->vertex_count == 0U) {
        set_error(error, error_cap, "vertices are required");
        return false;
    }
    if (desc->indices == nullptr || desc->index_count == 0U) {
        set_error(error, error_cap, "indices are required");
        return false;
    }
    if (desc->inverse_bind_matrices == nullptr || desc->joint_names == nullptr || desc->joint_count == 0U) {
        set_error(error, error_cap, "joint names and inverse bind matrices are required");
        return false;
    }
    for (uint32_t i = 0; i < desc->index_count; ++i) {
        if (desc->indices[i] >= desc->vertex_count) {
            set_error(error, error_cap, "index %u references missing vertex %u", i, desc->indices[i]);
            return false;
        }
    }
    for (uint32_t joint = 0; joint < desc->joint_count; ++joint) {
        if (desc->joint_names[joint] == nullptr || desc->joint_names[joint][0] == '\0') {
            set_error(error, error_cap, "joint %u needs a name", joint);
            return false;
        }
    }
    for (uint32_t v = 0; v < desc->vertex_count; ++v) {
        float weight_sum = 0.0F;
        for (int influence = 0; influence < 4; ++influence) {
            const uint16_t joint = desc->vertices[v].joints[influence];
            const float weight = desc->vertices[v].weights[influence];
            if (weight < 0.0F) {
                set_error(error, error_cap, "vertex %u has negative weight", v);
                return false;
            }
            if (weight > 0.0F && joint >= desc->joint_count) {
                set_error(error, error_cap, "vertex %u references missing joint %u", v, joint);
                return false;
            }
            weight_sum += weight;
        }
        if (weight_sum <= 0.0F) {
            set_error(error, error_cap, "vertex %u has no positive skin weight", v);
            return false;
        }
    }
    for (uint32_t socket = 0; socket < desc->socket_count; ++socket) {
        if (desc->sockets == nullptr || desc->sockets[socket].name == nullptr || desc->sockets[socket].joint_name == nullptr) {
            set_error(error, error_cap, "socket %u needs name and joint_name", socket);
            return false;
        }
        bool found = false;
        for (uint32_t joint = 0; joint < desc->joint_count; ++joint) {
            if (std::strcmp(desc->joint_names[joint], desc->sockets[socket].joint_name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            set_error(error, error_cap, "socket %s references missing joint %s", desc->sockets[socket].name, desc->sockets[socket].joint_name);
            return false;
        }
    }
    return true;
}

int nt_skeletal_mesh_create(const nt_skeletal_mesh_desc_t *desc,
                            nt_skeletal_mesh_t **out_mesh,
                            char *error,
                            size_t error_cap) {
    if (out_mesh == nullptr) {
        set_error(error, error_cap, "out_mesh is null");
        return 0;
    }
    *out_mesh = nullptr;
    if (!validate_desc(desc, error, error_cap)) {
        return 0;
    }

    nt_skeletal_mesh_t *mesh = new (std::nothrow) nt_skeletal_mesh_t();
    if (mesh == nullptr) {
        set_error(error, error_cap, "out of memory creating skeletal mesh");
        return 0;
    }

    mesh->vertices.assign(desc->vertices, desc->vertices + desc->vertex_count);
    mesh->indices.assign(desc->indices, desc->indices + desc->index_count);
    mesh->inverse_bind_matrices.assign(desc->inverse_bind_matrices, desc->inverse_bind_matrices + static_cast<size_t>(desc->joint_count) * 16U);
    mesh->joint_name_storage.reserve(desc->joint_count);
    mesh->joint_names.reserve(desc->joint_count);
    for (uint32_t i = 0; i < desc->joint_count; ++i) {
        mesh->joint_name_storage.emplace_back(desc->joint_names[i]);
    }
    for (const std::string &name : mesh->joint_name_storage) {
        mesh->joint_names.push_back(name.c_str());
    }

    if (desc->material_slots != nullptr && desc->material_slot_count > 0U) {
        mesh->material_slots.assign(desc->material_slots, desc->material_slots + desc->material_slot_count);
        mesh->material_name_storage.reserve(desc->material_slot_count);
        for (uint32_t i = 0; i < desc->material_slot_count; ++i) {
            mesh->material_name_storage.emplace_back(desc->material_slots[i].name != nullptr ? desc->material_slots[i].name : "");
            mesh->material_slots[i].name = mesh->material_name_storage.back().c_str();
        }
    }

    if (desc->sockets != nullptr && desc->socket_count > 0U) {
        mesh->sockets.assign(desc->sockets, desc->sockets + desc->socket_count);
        mesh->socket_name_storage.reserve(desc->socket_count);
        mesh->socket_joint_name_storage.reserve(desc->socket_count);
        for (uint32_t i = 0; i < desc->socket_count; ++i) {
            mesh->socket_name_storage.emplace_back(desc->sockets[i].name);
            mesh->socket_joint_name_storage.emplace_back(desc->sockets[i].joint_name);
            mesh->sockets[i].name = mesh->socket_name_storage.back().c_str();
            mesh->sockets[i].joint_name = mesh->socket_joint_name_storage.back().c_str();
        }
    }

    if (is_zero_matrix(desc->asset_to_model)) {
        mat4_identity(mesh->asset_to_model);
    } else {
        std::memcpy(mesh->asset_to_model, desc->asset_to_model, sizeof(mesh->asset_to_model));
    }

    *out_mesh = mesh;
    return 1;
}

void nt_skeletal_mesh_destroy(nt_skeletal_mesh_t *mesh) {
    delete mesh;
}

int nt_skeletal_mesh_instance_create(nt_skeletal_mesh_t *mesh,
                                     nt_skeletal_mesh_instance_t **out_instance,
                                     char *error,
                                     size_t error_cap) {
    if (out_instance == nullptr) {
        set_error(error, error_cap, "out_instance is null");
        return 0;
    }
    *out_instance = nullptr;
    if (mesh == nullptr) {
        set_error(error, error_cap, "mesh is null");
        return 0;
    }
    nt_skeletal_mesh_instance_t *instance = new (std::nothrow) nt_skeletal_mesh_instance_t();
    if (instance == nullptr) {
        set_error(error, error_cap, "out of memory creating skeletal mesh instance");
        return 0;
    }
    instance->mesh = mesh;
    instance->skinned_positions.resize(static_cast<size_t>(mesh->vertices.size()) * 3U);
    instance->last_model_matrices.resize(static_cast<size_t>(mesh->joint_names.size()) * 16U);
    instance->skin_matrices.resize(static_cast<size_t>(mesh->joint_names.size()) * 16U);
    instance->socket_matrices.resize(static_cast<size_t>(mesh->sockets.size()) * 16U);
    *out_instance = instance;
    return 1;
}

void nt_skeletal_mesh_instance_destroy(nt_skeletal_mesh_instance_t *instance) {
    delete instance;
}

int nt_skeletal_mesh_instance_update_pose(nt_skeletal_mesh_instance_t *instance,
                                          const float *model_matrices,
                                          int matrix_count,
                                          char *error,
                                          size_t error_cap) {
    if (instance == nullptr || instance->mesh == nullptr || model_matrices == nullptr) {
        set_error(error, error_cap, "instance, mesh, and model_matrices are required");
        return 0;
    }
    nt_skeletal_mesh_t *mesh = instance->mesh;
    if (matrix_count < static_cast<int>(mesh->joint_names.size())) {
        set_error(error, error_cap, "not enough model matrices: need=%u got=%d", static_cast<unsigned>(mesh->joint_names.size()), matrix_count);
        return 0;
    }
    std::memcpy(instance->last_model_matrices.data(), model_matrices, sizeof(float) * mesh->joint_names.size() * 16U);

    for (uint32_t joint = 0; joint < mesh->joint_names.size(); ++joint) {
        mat4_mul(model_matrices + static_cast<size_t>(joint) * 16U,
                 mesh->inverse_bind_matrices.data() + static_cast<size_t>(joint) * 16U,
                 instance->skin_matrices.data() + static_cast<size_t>(joint) * 16U);
    }

    for (uint32_t v = 0; v < mesh->vertices.size(); ++v) {
        const nt_skeletal_mesh_vertex_t &src = mesh->vertices[v];
        float skinned[3] = {0.0F, 0.0F, 0.0F};
        for (int influence = 0; influence < 4; ++influence) {
            const float weight = src.weights[influence];
            if (weight <= 0.0F) {
                continue;
            }
            const uint16_t joint = src.joints[influence];
            float transformed[3];
            mat4_transform_point(instance->skin_matrices.data() + static_cast<size_t>(joint) * 16U, src.position, transformed);
            skinned[0] += transformed[0] * weight;
            skinned[1] += transformed[1] * weight;
            skinned[2] += transformed[2] * weight;
        }
        float model_position[3];
        mat4_transform_point(mesh->asset_to_model, skinned, model_position);
        instance->skinned_positions[static_cast<size_t>(v) * 3U + 0U] = model_position[0];
        instance->skinned_positions[static_cast<size_t>(v) * 3U + 1U] = model_position[1];
        instance->skinned_positions[static_cast<size_t>(v) * 3U + 2U] = model_position[2];
    }

    for (uint32_t i = 0; i < mesh->sockets.size(); ++i) {
        const nt_skeletal_mesh_socket_t &socket = mesh->sockets[i];
        const int joint = find_joint_index(mesh, socket.joint_name);
        if (joint < 0) {
            set_error(error, error_cap, "socket %s references missing joint %s", socket.name, socket.joint_name);
            return 0;
        }
        float socket_local[16];
        float socket_model[16];
        mat4_from_trs(socket.local_offset, socket.local_rotation_quat, socket.local_scale, socket_local);
        mat4_mul(model_matrices + static_cast<size_t>(joint) * 16U, socket_local, socket_model);
        mat4_mul(mesh->asset_to_model, socket_model, instance->socket_matrices.data() + static_cast<size_t>(i) * 16U);
    }

    instance->has_pose = true;
    return 1;
}

int nt_skeletal_mesh_instance_socket_matrix(const nt_skeletal_mesh_instance_t *instance,
                                            const char *socket_name,
                                            float *out_column_major_matrix_16,
                                            char *error,
                                            size_t error_cap) {
    if (instance == nullptr || instance->mesh == nullptr || socket_name == nullptr || out_column_major_matrix_16 == nullptr) {
        set_error(error, error_cap, "instance, socket_name, and output matrix are required");
        return 0;
    }
    if (!instance->has_pose) {
        set_error(error, error_cap, "instance has no sampled pose");
        return 0;
    }
    const nt_skeletal_mesh_t *mesh = instance->mesh;
    for (uint32_t i = 0; i < mesh->sockets.size(); ++i) {
        if (std::strcmp(mesh->sockets[i].name, socket_name) == 0) {
            std::memcpy(out_column_major_matrix_16, instance->socket_matrices.data() + static_cast<size_t>(i) * 16U, sizeof(float) * 16U);
            return 1;
        }
    }
    set_error(error, error_cap, "socket not found: %s", socket_name);
    return 0;
}

uint32_t nt_skeletal_mesh_instance_skinned_position_count(const nt_skeletal_mesh_instance_t *instance) {
    return instance != nullptr && instance->mesh != nullptr ? static_cast<uint32_t>(instance->mesh->vertices.size()) : 0U;
}

int nt_skeletal_mesh_instance_copy_skinned_positions(const nt_skeletal_mesh_instance_t *instance,
                                                     float *out_xyz,
                                                     uint32_t max_positions,
                                                     char *error,
                                                     size_t error_cap) {
    if (instance == nullptr || out_xyz == nullptr) {
        set_error(error, error_cap, "instance and out_xyz are required");
        return 0;
    }
    if (!instance->has_pose) {
        set_error(error, error_cap, "instance has no sampled pose");
        return 0;
    }
    const uint32_t count = nt_skeletal_mesh_instance_skinned_position_count(instance);
    if (max_positions < count) {
        set_error(error, error_cap, "not enough output positions: need=%u got=%u", count, max_positions);
        return 0;
    }
    std::memcpy(out_xyz, instance->skinned_positions.data(), sizeof(float) * static_cast<size_t>(count) * 3U);
    return static_cast<int>(count);
}
