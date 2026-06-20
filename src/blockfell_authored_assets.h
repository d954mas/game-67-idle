#ifndef BLOCKFELL_AUTHORED_ASSETS_H
#define BLOCKFELL_AUTHORED_ASSETS_H

#include <stdint.h>

typedef struct BlockfellAssetVertex {
    float pos[3];
    float normal[3];
    float color[4];
} BlockfellAssetVertex;

typedef struct BlockfellAssetMesh {
    const BlockfellAssetVertex *vertices;
    uint16_t vertex_count;
    const uint16_t *indices;
    uint16_t index_count;
} BlockfellAssetMesh;

static const BlockfellAssetVertex BF_HERO_CUIRASS_VERTICES[] = {
    {{-0.34F, 0.18F, -0.26F}, {-0.55F, -0.30F, -0.55F}, {0.10F, 0.18F, 0.38F, 1.0F}},
    {{0.34F, 0.18F, -0.26F}, {0.55F, -0.30F, -0.55F}, {0.13F, 0.24F, 0.50F, 1.0F}},
    {{0.28F, 0.92F, -0.20F}, {0.50F, 0.40F, -0.55F}, {0.22F, 0.44F, 0.82F, 1.0F}},
    {{-0.28F, 0.92F, -0.20F}, {-0.50F, 0.40F, -0.55F}, {0.18F, 0.34F, 0.70F, 1.0F}},
    {{-0.30F, 0.22F, 0.24F}, {-0.50F, -0.25F, 0.55F}, {0.12F, 0.20F, 0.40F, 1.0F}},
    {{0.30F, 0.22F, 0.24F}, {0.50F, -0.25F, 0.55F}, {0.16F, 0.28F, 0.54F, 1.0F}},
    {{0.24F, 0.86F, 0.18F}, {0.45F, 0.35F, 0.55F}, {0.36F, 0.62F, 0.98F, 1.0F}},
    {{-0.24F, 0.86F, 0.18F}, {-0.45F, 0.35F, 0.55F}, {0.28F, 0.52F, 0.92F, 1.0F}},
};

static const uint16_t BF_BOX_INDICES[] = {
    0, 1, 2, 0, 2, 3,
    1, 5, 6, 1, 6, 2,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0,
};

static const BlockfellAssetVertex BF_HERO_CAPE_VERTICES[] = {
    {{-0.38F, 0.94F, -0.28F}, {0.0F, 0.18F, -0.98F}, {0.05F, 0.08F, 0.22F, 0.95F}},
    {{0.38F, 0.94F, -0.28F}, {0.0F, 0.18F, -0.98F}, {0.07F, 0.10F, 0.28F, 0.95F}},
    {{0.50F, 0.08F, -0.64F}, {0.0F, 0.18F, -0.98F}, {0.02F, 0.04F, 0.14F, 0.95F}},
    {{-0.50F, 0.08F, -0.64F}, {0.0F, 0.18F, -0.98F}, {0.03F, 0.05F, 0.17F, 0.95F}},
};

static const uint16_t BF_QUAD_INDICES[] = {0, 1, 2, 0, 2, 3};

static const BlockfellAssetVertex BF_HERO_CREST_VERTICES[] = {
    {{0.0F, 1.56F, 0.0F}, {0.0F, 0.78F, 0.30F}, {0.80F, 0.92F, 1.0F, 1.0F}},
    {{-0.24F, 1.34F, -0.18F}, {-0.45F, 0.32F, -0.55F}, {0.44F, 0.62F, 0.76F, 1.0F}},
    {{0.24F, 1.34F, -0.18F}, {0.45F, 0.32F, -0.55F}, {0.68F, 0.82F, 0.96F, 1.0F}},
    {{0.18F, 1.34F, 0.16F}, {0.45F, 0.32F, 0.55F}, {0.52F, 0.70F, 0.88F, 1.0F}},
    {{-0.18F, 1.34F, 0.16F}, {-0.45F, 0.32F, 0.55F}, {0.46F, 0.64F, 0.82F, 1.0F}},
};

static const uint16_t BF_PYRAMID_INDICES[] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 1, 4, 3, 1, 3, 2};

static const BlockfellAssetVertex BF_ENEMY_MASK_VERTICES[] = {
    {{-0.28F, 0.94F, -0.24F}, {-0.34F, 0.25F, -0.80F}, {0.70F, 0.20F, 0.12F, 1.0F}},
    {{0.28F, 0.94F, -0.24F}, {0.34F, 0.25F, -0.80F}, {0.96F, 0.36F, 0.16F, 1.0F}},
    {{0.0F, 1.26F, -0.18F}, {0.0F, 0.64F, -0.64F}, {0.92F, 0.66F, 0.26F, 1.0F}},
    {{0.0F, 0.78F, -0.34F}, {0.0F, -0.25F, -0.80F}, {0.28F, 0.06F, 0.05F, 1.0F}},
};

static const uint16_t BF_ENEMY_MASK_INDICES[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};

static const BlockfellAssetVertex BF_ENEMY_HORN_VERTICES[] = {
    {{-0.42F, 1.16F, -0.12F}, {-0.70F, 0.42F, -0.20F}, {0.78F, 0.70F, 0.52F, 1.0F}},
    {{-0.78F, 1.34F, -0.18F}, {-0.90F, 0.25F, -0.20F}, {0.96F, 0.90F, 0.70F, 1.0F}},
    {{-0.46F, 1.00F, 0.06F}, {-0.70F, -0.20F, 0.35F}, {0.58F, 0.48F, 0.34F, 1.0F}},
    {{0.42F, 1.16F, -0.12F}, {0.70F, 0.42F, -0.20F}, {0.78F, 0.70F, 0.52F, 1.0F}},
    {{0.78F, 1.34F, -0.18F}, {0.90F, 0.25F, -0.20F}, {0.96F, 0.90F, 0.70F, 1.0F}},
    {{0.46F, 1.00F, 0.06F}, {0.70F, -0.20F, 0.35F}, {0.58F, 0.48F, 0.34F, 1.0F}},
};

static const uint16_t BF_ENEMY_HORN_INDICES[] = {0, 1, 2, 3, 5, 4};

static const BlockfellAssetVertex BF_RUNE_SPIRE_VERTICES[] = {
    {{-0.30F, 0.18F, -0.30F}, {-0.58F, -0.22F, -0.58F}, {0.20F, 0.23F, 0.28F, 1.0F}},
    {{0.30F, 0.18F, -0.30F}, {0.58F, -0.22F, -0.58F}, {0.28F, 0.31F, 0.36F, 1.0F}},
    {{0.30F, 0.18F, 0.30F}, {0.58F, -0.22F, 0.58F}, {0.18F, 0.22F, 0.27F, 1.0F}},
    {{-0.30F, 0.18F, 0.30F}, {-0.58F, -0.22F, 0.58F}, {0.24F, 0.28F, 0.32F, 1.0F}},
    {{-0.18F, 1.22F, -0.18F}, {-0.50F, 0.54F, -0.50F}, {0.42F, 0.50F, 0.58F, 1.0F}},
    {{0.18F, 1.22F, -0.18F}, {0.50F, 0.54F, -0.50F}, {0.46F, 0.56F, 0.64F, 1.0F}},
    {{0.18F, 1.22F, 0.18F}, {0.50F, 0.54F, 0.50F}, {0.36F, 0.46F, 0.54F, 1.0F}},
    {{-0.18F, 1.22F, 0.18F}, {-0.50F, 0.54F, 0.50F}, {0.40F, 0.50F, 0.60F, 1.0F}},
};

static const BlockfellAssetVertex BF_RUNE_GLYPH_VERTICES[] = {
    {{0.0F, 1.38F, -0.04F}, {0.0F, 0.30F, -0.86F}, {0.18F, 0.96F, 1.0F, 0.95F}},
    {{0.24F, 1.10F, -0.04F}, {0.20F, 0.10F, -0.86F}, {0.12F, 0.76F, 1.0F, 0.95F}},
    {{0.0F, 0.82F, -0.04F}, {0.0F, -0.20F, -0.86F}, {0.18F, 0.96F, 0.78F, 0.95F}},
    {{-0.24F, 1.10F, -0.04F}, {-0.20F, 0.10F, -0.86F}, {0.12F, 0.76F, 1.0F, 0.95F}},
};

static const BlockfellAssetVertex BF_GATE_KEYSTONE_VERTICES[] = {
    {{-0.70F, 1.76F, -0.18F}, {-0.62F, 0.16F, -0.58F}, {0.20F, 0.24F, 0.28F, 1.0F}},
    {{0.70F, 1.76F, -0.18F}, {0.62F, 0.16F, -0.58F}, {0.26F, 0.31F, 0.36F, 1.0F}},
    {{0.46F, 2.34F, -0.14F}, {0.44F, 0.54F, -0.48F}, {0.34F, 0.42F, 0.48F, 1.0F}},
    {{-0.46F, 2.34F, -0.14F}, {-0.44F, 0.54F, -0.48F}, {0.30F, 0.38F, 0.44F, 1.0F}},
    {{-0.56F, 1.80F, 0.18F}, {-0.48F, 0.18F, 0.58F}, {0.14F, 0.18F, 0.22F, 1.0F}},
    {{0.56F, 1.80F, 0.18F}, {0.48F, 0.18F, 0.58F}, {0.18F, 0.22F, 0.27F, 1.0F}},
    {{0.36F, 2.28F, 0.16F}, {0.38F, 0.52F, 0.48F}, {0.28F, 0.36F, 0.42F, 1.0F}},
    {{-0.36F, 2.28F, 0.16F}, {-0.38F, 0.52F, 0.48F}, {0.24F, 0.32F, 0.38F, 1.0F}},
};

static const BlockfellAssetVertex BF_CHEST_LOCK_VERTICES[] = {
    {{-0.42F, 0.38F, -0.38F}, {-0.35F, 0.20F, -0.75F}, {0.94F, 0.68F, 0.16F, 1.0F}},
    {{0.42F, 0.38F, -0.38F}, {0.35F, 0.20F, -0.75F}, {1.0F, 0.82F, 0.30F, 1.0F}},
    {{0.34F, 0.66F, -0.42F}, {0.30F, 0.45F, -0.75F}, {0.98F, 0.76F, 0.22F, 1.0F}},
    {{-0.34F, 0.66F, -0.42F}, {-0.30F, 0.45F, -0.75F}, {0.82F, 0.54F, 0.12F, 1.0F}},
    {{-0.18F, 0.50F, -0.46F}, {-0.20F, 0.20F, -0.88F}, {0.18F, 0.13F, 0.08F, 1.0F}},
    {{0.18F, 0.50F, -0.46F}, {0.20F, 0.20F, -0.88F}, {0.24F, 0.16F, 0.08F, 1.0F}},
    {{0.12F, 0.26F, -0.48F}, {0.12F, -0.35F, -0.88F}, {0.12F, 0.08F, 0.05F, 1.0F}},
    {{-0.12F, 0.26F, -0.48F}, {-0.12F, -0.35F, -0.88F}, {0.10F, 0.07F, 0.04F, 1.0F}},
};

static const uint16_t BF_CHEST_LOCK_INDICES[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
};

static const BlockfellAssetVertex BF_CAMP_STANDARD_VERTICES[] = {
    {{-0.06F, 0.18F, 0.0F}, {-0.40F, 0.0F, -0.20F}, {0.18F, 0.10F, 0.06F, 1.0F}},
    {{0.06F, 0.18F, 0.0F}, {0.40F, 0.0F, -0.20F}, {0.24F, 0.14F, 0.08F, 1.0F}},
    {{0.06F, 1.78F, 0.0F}, {0.40F, 0.30F, -0.20F}, {0.26F, 0.16F, 0.10F, 1.0F}},
    {{-0.06F, 1.78F, 0.0F}, {-0.40F, 0.30F, -0.20F}, {0.20F, 0.12F, 0.08F, 1.0F}},
    {{0.06F, 1.54F, -0.04F}, {0.0F, 0.10F, -0.90F}, {0.62F, 0.06F, 0.08F, 1.0F}},
    {{0.82F, 1.36F, -0.04F}, {0.20F, 0.10F, -0.90F}, {0.82F, 0.12F, 0.12F, 1.0F}},
    {{0.06F, 1.08F, -0.04F}, {0.0F, -0.20F, -0.90F}, {0.36F, 0.03F, 0.05F, 1.0F}},
};

static const uint16_t BF_CAMP_STANDARD_INDICES[] = {0, 1, 2, 0, 2, 3, 4, 5, 6};

static const BlockfellAssetMesh BF_MESH_HERO_CUIRASS = {BF_HERO_CUIRASS_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_HERO_CAPE = {BF_HERO_CAPE_VERTICES, 4U, BF_QUAD_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_HERO_CREST = {BF_HERO_CREST_VERTICES, 5U, BF_PYRAMID_INDICES, 18U};
static const BlockfellAssetMesh BF_MESH_ENEMY_MASK = {BF_ENEMY_MASK_VERTICES, 4U, BF_ENEMY_MASK_INDICES, 12U};
static const BlockfellAssetMesh BF_MESH_ENEMY_HORNS = {BF_ENEMY_HORN_VERTICES, 6U, BF_ENEMY_HORN_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_RUNE_SPIRE = {BF_RUNE_SPIRE_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_RUNE_GLYPH = {BF_RUNE_GLYPH_VERTICES, 4U, BF_QUAD_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_GATE_KEYSTONE = {BF_GATE_KEYSTONE_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_CHEST_LOCK = {BF_CHEST_LOCK_VERTICES, 8U, BF_CHEST_LOCK_INDICES, 12U};
static const BlockfellAssetMesh BF_MESH_CAMP_STANDARD = {BF_CAMP_STANDARD_VERTICES, 7U, BF_CAMP_STANDARD_INDICES, 9U};

#endif
