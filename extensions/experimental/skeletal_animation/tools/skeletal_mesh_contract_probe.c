#include "skeletal_animation/nt_skeletal_mesh.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void mat4_identity(float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[15] = 1.0F;
}

static void mat4_translate(float x, float y, float z, float out[16]) {
    mat4_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

static int nearly(float a, float b) {
    const float d = fabsf(a - b);
    return d < 0.0001F;
}

static int expect_create_fail(const nt_skeletal_mesh_desc_t *desc, const char *label) {
    char error[256];
    nt_skeletal_mesh_t *mesh = (nt_skeletal_mesh_t *)0x1;
    if (nt_skeletal_mesh_create(desc, &mesh, error, sizeof(error))) {
        fprintf(stderr, "expected create failure for %s\n", label);
        nt_skeletal_mesh_destroy(mesh);
        return 0;
    }
    if (mesh != NULL) {
        fprintf(stderr, "failed create left mesh non-null for %s\n", label);
        return 0;
    }
    return 1;
}

static int expect_int_fail(int value, const char *label) {
    if (value != 0) {
        fprintf(stderr, "expected failure for %s\n", label);
        return 0;
    }
    return 1;
}

static int expect_known_defect_create_success(const nt_skeletal_mesh_desc_t *desc, const char *label) {
    char error[256];
    nt_skeletal_mesh_t *mesh = NULL;
    if (!nt_skeletal_mesh_create(desc, &mesh, error, sizeof(error))) {
        fprintf(stderr, "known defect no longer reproduced for %s: %s\n", label, error);
        return 0;
    }
    printf("KNOWN_DEFECT reproduced: create accepts %s\n", label);
    nt_skeletal_mesh_destroy(mesh);
    return 1;
}

static void build_base_desc(nt_skeletal_mesh_desc_t *desc,
                            const nt_skeletal_mesh_vertex_t *vertices,
                            const uint32_t *indices,
                            const float *inverse_bind,
                            const char *const *joint_names,
                            const nt_skeletal_mesh_socket_t *sockets) {
    memset(desc, 0, sizeof(*desc));
    desc->vertices = vertices;
    desc->vertex_count = 3;
    desc->indices = indices;
    desc->index_count = 3;
    desc->inverse_bind_matrices = inverse_bind;
    desc->joint_names = joint_names;
    desc->joint_count = 2;
    desc->sockets = sockets;
    desc->socket_count = 1;
}

int main(int argc, char **argv) {
    char error[256];
    const char *joint_names[] = {"root", "handslot.l"};
    float inverse_bind[32];
    mat4_identity(inverse_bind);
    mat4_identity(inverse_bind + 16);

    nt_skeletal_mesh_vertex_t vertices[3] = {
        {
            .position = {0.0F, 0.0F, 0.0F},
            .normal = {0.0F, 1.0F, 0.0F},
            .uv = {0.0F, 0.0F},
            .joints = {0, 0, 0, 0},
            .weights = {1.0F, 0.0F, 0.0F, 0.0F},
        },
        {
            .position = {1.0F, 0.0F, 0.0F},
            .normal = {0.0F, 1.0F, 0.0F},
            .uv = {1.0F, 0.0F},
            .joints = {1, 0, 0, 0},
            .weights = {1.0F, 0.0F, 0.0F, 0.0F},
        },
        {
            .position = {0.0F, 1.0F, 0.0F},
            .normal = {0.0F, 1.0F, 0.0F},
            .uv = {0.0F, 1.0F},
            .joints = {0, 0, 0, 0},
            .weights = {1.0F, 0.0F, 0.0F, 0.0F},
        },
    };
    const uint32_t indices[] = {0, 1, 2};
    const nt_skeletal_mesh_socket_t sockets[] = {
        {
            .name = "tool",
            .joint_name = "handslot.l",
            .local_offset = {0.25F, 0.0F, 0.0F},
            .local_rotation_quat = {0.0F, 0.0F, 0.70710678F, 0.70710678F},
            .local_scale = {2.0F, 3.0F, 4.0F},
        },
    };

    nt_skeletal_mesh_desc_t desc;
    build_base_desc(&desc, vertices, indices, inverse_bind, joint_names, sockets);

    if (!expect_create_fail(NULL, "null desc")) {
        return 1;
    }
    if (!expect_int_fail(nt_skeletal_mesh_create(&desc, NULL, error, sizeof(error)), "null out_mesh")) {
        return 1;
    }

    uint32_t bad_indices[] = {0, 7, 2};
    nt_skeletal_mesh_desc_t bad_index_desc = desc;
    bad_index_desc.indices = bad_indices;
    if (!expect_create_fail(&bad_index_desc, "invalid index")) {
        return 1;
    }

    nt_skeletal_mesh_vertex_t bad_vertices[3];
    memcpy(bad_vertices, vertices, sizeof(vertices));
    bad_vertices[1].joints[0] = 9;
    nt_skeletal_mesh_desc_t bad_joint_desc = desc;
    bad_joint_desc.vertices = bad_vertices;
    if (!expect_create_fail(&bad_joint_desc, "missing vertex joint")) {
        return 1;
    }

    nt_skeletal_mesh_socket_t bad_socket = sockets[0];
    bad_socket.joint_name = "missing";
    nt_skeletal_mesh_desc_t bad_socket_desc = desc;
    bad_socket_desc.sockets = &bad_socket;
    if (!expect_create_fail(&bad_socket_desc, "missing socket joint")) {
        return 1;
    }

    nt_skeletal_mesh_vertex_t nan_weight_vertices[3];
    memcpy(nan_weight_vertices, vertices, sizeof(vertices));
    nan_weight_vertices[0].weights[0] = NAN;
    nt_skeletal_mesh_desc_t nan_weight_desc = desc;
    nan_weight_desc.vertices = nan_weight_vertices;
    if (!expect_known_defect_create_success(&nan_weight_desc, "NaN weight")) {
        return 1;
    }

    float inf_inverse_bind[32];
    memcpy(inf_inverse_bind, inverse_bind, sizeof(inverse_bind));
    inf_inverse_bind[0] = INFINITY;
    nt_skeletal_mesh_desc_t inf_bind_desc = desc;
    inf_bind_desc.inverse_bind_matrices = inf_inverse_bind;
    if (!expect_known_defect_create_success(&inf_bind_desc, "infinite inverse-bind matrix")) {
        return 1;
    }

    nt_skeletal_mesh_t *mesh = NULL;
    if (!nt_skeletal_mesh_create(&desc, &mesh, error, sizeof(error))) {
        fprintf(stderr, "create failed: %s\n", error);
        return 1;
    }
    nt_skeletal_mesh_instance_t *instance = NULL;
    if (!nt_skeletal_mesh_instance_create(mesh, &instance, error, sizeof(error))) {
        fprintf(stderr, "instance failed: %s\n", error);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "--known-uaf") == 0) {
        nt_skeletal_mesh_destroy(mesh);
        printf("KNOWN_DEFECT trigger: instance dereferences destroyed mesh, count=%u\n",
               nt_skeletal_mesh_instance_skinned_position_count(instance));
        nt_skeletal_mesh_instance_destroy(instance);
        return 0;
    }
    nt_skeletal_mesh_instance_t *bad_instance = (nt_skeletal_mesh_instance_t *)0x1;
    if (!expect_int_fail(nt_skeletal_mesh_instance_create(NULL, &bad_instance, error, sizeof(error)), "null mesh instance") || bad_instance != NULL) {
        fprintf(stderr, "null mesh instance test left out pointer non-null\n");
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }

    float socket_matrix[16];
    float positions[9];
    if (!expect_int_fail(nt_skeletal_mesh_instance_socket_matrix(instance, "tool", socket_matrix, error, sizeof(error)), "socket before pose") ||
        !expect_int_fail(nt_skeletal_mesh_instance_copy_skinned_positions(instance, positions, 3, error, sizeof(error)), "copy before pose")) {
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }

    float model_matrices[32];
    mat4_identity(model_matrices);
    mat4_translate(0.5F, 2.0F, 0.0F, model_matrices + 16);
    if (!expect_int_fail(nt_skeletal_mesh_instance_update_pose(instance, model_matrices, 1, error, sizeof(error)), "too few matrices")) {
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!nt_skeletal_mesh_instance_update_pose(instance, model_matrices, 2, error, sizeof(error))) {
        fprintf(stderr, "update failed: %s\n", error);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }

    if (nt_skeletal_mesh_instance_copy_skinned_positions(instance, positions, 3, error, sizeof(error)) != 3) {
        fprintf(stderr, "copy positions failed: %s\n", error);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!nearly(positions[3], 1.5F) || !nearly(positions[4], 2.0F) || !nearly(positions[5], 0.0F)) {
        fprintf(stderr, "unexpected skinned vertex: %.5f %.5f %.5f\n", (double)positions[3], (double)positions[4], (double)positions[5]);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!expect_int_fail(nt_skeletal_mesh_instance_copy_skinned_positions(instance, positions, 2, error, sizeof(error)), "small position buffer")) {
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }

    if (!nt_skeletal_mesh_instance_socket_matrix(instance, "tool", socket_matrix, error, sizeof(error))) {
        fprintf(stderr, "socket failed: %s\n", error);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!expect_int_fail(nt_skeletal_mesh_instance_socket_matrix(instance, "missing", socket_matrix, error, sizeof(error)), "missing socket")) {
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!nearly(socket_matrix[12], 0.75F) || !nearly(socket_matrix[13], 2.0F) || !nearly(socket_matrix[14], 0.0F)) {
        fprintf(stderr, "unexpected socket: %.5f %.5f %.5f\n", (double)socket_matrix[12], (double)socket_matrix[13], (double)socket_matrix[14]);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    if (!nearly(socket_matrix[0], 0.0F) || !nearly(socket_matrix[1], 2.0F) ||
        !nearly(socket_matrix[4], -3.0F) || !nearly(socket_matrix[5], 0.0F) ||
        !nearly(socket_matrix[10], 4.0F)) {
        fprintf(stderr, "unexpected socket basis: [%.5f %.5f] [%.5f %.5f] z=%.5f\n",
                (double)socket_matrix[0],
                (double)socket_matrix[1],
                (double)socket_matrix[4],
                (double)socket_matrix[5],
                (double)socket_matrix[10]);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }

    const char *reversed_joint_names[] = {"handslot.l", "root"};
    nt_skeletal_mesh_desc_t reversed_desc = desc;
    reversed_desc.joint_names = reversed_joint_names;
    nt_skeletal_mesh_t *reversed_mesh = NULL;
    nt_skeletal_mesh_instance_t *reversed_instance = NULL;
    if (!nt_skeletal_mesh_create(&reversed_desc, &reversed_mesh, error, sizeof(error)) ||
        !nt_skeletal_mesh_instance_create(reversed_mesh, &reversed_instance, error, sizeof(error)) ||
        !nt_skeletal_mesh_instance_update_pose(reversed_instance, model_matrices, 2, error, sizeof(error)) ||
        nt_skeletal_mesh_instance_copy_skinned_positions(reversed_instance, positions, 3, error, sizeof(error)) != 3 ||
        !nearly(positions[3], 1.5F)) {
        fprintf(stderr, "positional joint-order defect was not reproduced: %s\n", error);
        nt_skeletal_mesh_instance_destroy(reversed_instance);
        nt_skeletal_mesh_destroy(reversed_mesh);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    puts("KNOWN_DEFECT reproduced: joint names do not remap positional matrices");
    nt_skeletal_mesh_instance_destroy(reversed_instance);
    nt_skeletal_mesh_destroy(reversed_mesh);

    model_matrices[16] = NAN;
    if (!nt_skeletal_mesh_instance_update_pose(instance, model_matrices, 2, error, sizeof(error)) ||
        nt_skeletal_mesh_instance_copy_skinned_positions(instance, positions, 3, error, sizeof(error)) != 3 ||
        isfinite(positions[3])) {
        fprintf(stderr, "non-finite model-matrix defect was not reproduced: %s\n", error);
        nt_skeletal_mesh_instance_destroy(instance);
        nt_skeletal_mesh_destroy(mesh);
        return 1;
    }
    puts("KNOWN_DEFECT reproduced: non-finite model matrix reaches skinned output");

    printf("skeletal mesh contract probe passed: positions=%u socket=[%.2f %.2f %.2f] failure_paths=11 known_defects=4\n",
           nt_skeletal_mesh_instance_skinned_position_count(instance),
           (double)socket_matrix[12],
           (double)socket_matrix[13],
           (double)socket_matrix[14]);
    nt_skeletal_mesh_instance_destroy(instance);
    nt_skeletal_mesh_destroy(mesh);
    return 0;
}
