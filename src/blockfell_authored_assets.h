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

static const BlockfellAssetVertex BF_HERO_BODY_VERTICES[] = {
    {{-0.36F, 0.10F, -0.22F}, {-0.56F, -0.22F, -0.58F}, {0.10F, 0.16F, 0.34F, 1.0F}},
    {{0.36F, 0.10F, -0.22F}, {0.56F, -0.22F, -0.58F}, {0.13F, 0.24F, 0.50F, 1.0F}},
    {{0.44F, 0.82F, -0.28F}, {0.62F, 0.24F, -0.56F}, {0.24F, 0.46F, 0.84F, 1.0F}},
    {{-0.44F, 0.82F, -0.28F}, {-0.62F, 0.24F, -0.56F}, {0.18F, 0.34F, 0.72F, 1.0F}},
    {{-0.28F, 0.10F, 0.24F}, {-0.46F, -0.20F, 0.56F}, {0.10F, 0.18F, 0.36F, 1.0F}},
    {{0.28F, 0.10F, 0.24F}, {0.46F, -0.20F, 0.56F}, {0.14F, 0.26F, 0.52F, 1.0F}},
    {{0.34F, 0.78F, 0.20F}, {0.54F, 0.26F, 0.58F}, {0.32F, 0.58F, 0.96F, 1.0F}},
    {{-0.34F, 0.78F, 0.20F}, {-0.54F, 0.26F, 0.58F}, {0.24F, 0.48F, 0.90F, 1.0F}},
};

static const BlockfellAssetVertex BF_HERO_HEAD_VERTICES[] = {
    {{-0.24F, 0.92F, -0.20F}, {-0.48F, -0.10F, -0.62F}, {0.72F, 0.54F, 0.36F, 1.0F}},
    {{0.24F, 0.92F, -0.20F}, {0.48F, -0.10F, -0.62F}, {0.94F, 0.72F, 0.48F, 1.0F}},
    {{0.24F, 1.30F, -0.16F}, {0.48F, 0.42F, -0.58F}, {0.98F, 0.78F, 0.54F, 1.0F}},
    {{-0.24F, 1.30F, -0.16F}, {-0.48F, 0.42F, -0.58F}, {0.86F, 0.64F, 0.42F, 1.0F}},
    {{-0.20F, 0.94F, 0.18F}, {-0.44F, -0.10F, 0.58F}, {0.70F, 0.50F, 0.34F, 1.0F}},
    {{0.20F, 0.94F, 0.18F}, {0.44F, -0.10F, 0.58F}, {0.90F, 0.68F, 0.46F, 1.0F}},
    {{0.18F, 1.26F, 0.14F}, {0.42F, 0.40F, 0.56F}, {0.92F, 0.70F, 0.50F, 1.0F}},
    {{-0.18F, 1.26F, 0.14F}, {-0.42F, 0.40F, 0.56F}, {0.82F, 0.60F, 0.42F, 1.0F}},
};

static const BlockfellAssetVertex BF_HERO_SWORD_VERTICES[] = {
    {{0.34F, 0.64F, -0.08F}, {0.0F, 0.0F, -1.0F}, {0.78F, 0.88F, 0.94F, 1.0F}},
    {{0.50F, 0.74F, -0.12F}, {0.0F, 0.0F, -1.0F}, {0.96F, 0.98F, 1.0F, 1.0F}},
    {{0.96F, 1.04F, -0.22F}, {0.0F, 0.0F, -1.0F}, {0.86F, 0.92F, 0.96F, 1.0F}},
    {{1.18F, 1.18F, -0.24F}, {0.0F, 0.0F, -1.0F}, {0.98F, 1.0F, 1.0F, 1.0F}},
    {{0.78F, 0.98F, -0.08F}, {0.0F, 0.0F, -1.0F}, {0.58F, 0.72F, 0.82F, 1.0F}},
};

static const uint16_t BF_HERO_SWORD_INDICES[] = {0, 1, 2, 1, 3, 2, 2, 3, 4};

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

static const BlockfellAssetVertex BF_ENEMY_BODY_VERTICES[] = {
    {{-0.36F, 0.10F, -0.24F}, {-0.56F, -0.22F, -0.58F}, {0.34F, 0.07F, 0.08F, 1.0F}},
    {{0.36F, 0.10F, -0.24F}, {0.56F, -0.22F, -0.58F}, {0.48F, 0.09F, 0.10F, 1.0F}},
    {{0.46F, 0.76F, -0.28F}, {0.62F, 0.20F, -0.56F}, {0.76F, 0.16F, 0.14F, 1.0F}},
    {{-0.46F, 0.76F, -0.28F}, {-0.62F, 0.20F, -0.56F}, {0.58F, 0.12F, 0.12F, 1.0F}},
    {{-0.26F, 0.08F, 0.22F}, {-0.44F, -0.20F, 0.56F}, {0.26F, 0.05F, 0.06F, 1.0F}},
    {{0.26F, 0.08F, 0.22F}, {0.44F, -0.20F, 0.56F}, {0.38F, 0.07F, 0.08F, 1.0F}},
    {{0.32F, 0.72F, 0.18F}, {0.54F, 0.22F, 0.58F}, {0.60F, 0.13F, 0.12F, 1.0F}},
    {{-0.32F, 0.72F, 0.18F}, {-0.54F, 0.22F, 0.58F}, {0.44F, 0.09F, 0.09F, 1.0F}},
};

static const BlockfellAssetVertex BF_ENEMY_HEAD_VERTICES[] = {
    {{-0.28F, 0.82F, -0.22F}, {-0.46F, -0.08F, -0.64F}, {0.64F, 0.40F, 0.26F, 1.0F}},
    {{0.28F, 0.82F, -0.22F}, {0.46F, -0.08F, -0.64F}, {0.82F, 0.52F, 0.32F, 1.0F}},
    {{0.30F, 1.18F, -0.18F}, {0.50F, 0.38F, -0.58F}, {0.88F, 0.60F, 0.38F, 1.0F}},
    {{-0.30F, 1.18F, -0.18F}, {-0.50F, 0.38F, -0.58F}, {0.72F, 0.46F, 0.30F, 1.0F}},
    {{-0.22F, 0.84F, 0.18F}, {-0.42F, -0.10F, 0.58F}, {0.48F, 0.28F, 0.20F, 1.0F}},
    {{0.22F, 0.84F, 0.18F}, {0.42F, -0.10F, 0.58F}, {0.64F, 0.38F, 0.24F, 1.0F}},
    {{0.20F, 1.14F, 0.14F}, {0.40F, 0.36F, 0.56F}, {0.66F, 0.42F, 0.28F, 1.0F}},
    {{-0.20F, 1.14F, 0.14F}, {-0.40F, 0.36F, 0.56F}, {0.54F, 0.32F, 0.22F, 1.0F}},
};

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

static const BlockfellAssetVertex BF_PINE_CROWN_VERTICES[] = {
    {{0.0F, 1.68F, 0.0F}, {0.0F, 0.88F, 0.0F}, {0.10F, 0.52F, 0.24F, 1.0F}},
    {{-0.78F, 0.78F, -0.58F}, {-0.58F, 0.28F, -0.44F}, {0.04F, 0.28F, 0.16F, 1.0F}},
    {{0.78F, 0.78F, -0.58F}, {0.58F, 0.28F, -0.44F}, {0.06F, 0.36F, 0.18F, 1.0F}},
    {{0.68F, 0.78F, 0.64F}, {0.48F, 0.28F, 0.54F}, {0.04F, 0.32F, 0.17F, 1.0F}},
    {{-0.68F, 0.78F, 0.64F}, {-0.48F, 0.28F, 0.54F}, {0.08F, 0.42F, 0.22F, 1.0F}},
    {{0.0F, 1.16F, 0.0F}, {0.0F, 0.50F, 0.0F}, {0.05F, 0.38F, 0.20F, 1.0F}},
    {{-0.50F, 0.40F, -0.34F}, {-0.58F, -0.18F, -0.38F}, {0.03F, 0.23F, 0.13F, 1.0F}},
    {{0.50F, 0.40F, -0.34F}, {0.58F, -0.18F, -0.38F}, {0.05F, 0.30F, 0.15F, 1.0F}},
    {{0.46F, 0.40F, 0.40F}, {0.46F, -0.18F, 0.50F}, {0.03F, 0.26F, 0.14F, 1.0F}},
    {{-0.46F, 0.40F, 0.40F}, {-0.46F, -0.18F, 0.50F}, {0.06F, 0.34F, 0.18F, 1.0F}},
};

static const uint16_t BF_PINE_CROWN_INDICES[] = {
    0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
    5, 6, 7, 5, 7, 8, 5, 8, 9, 5, 9, 6,
};

static const BlockfellAssetVertex BF_ROCK_SHARD_VERTICES[] = {
    {{-0.52F, 0.06F, -0.42F}, {-0.58F, -0.20F, -0.52F}, {0.24F, 0.28F, 0.30F, 1.0F}},
    {{0.48F, 0.04F, -0.36F}, {0.58F, -0.18F, -0.48F}, {0.30F, 0.34F, 0.36F, 1.0F}},
    {{0.38F, 0.02F, 0.48F}, {0.50F, -0.18F, 0.56F}, {0.18F, 0.22F, 0.26F, 1.0F}},
    {{-0.44F, 0.03F, 0.38F}, {-0.52F, -0.18F, 0.50F}, {0.22F, 0.26F, 0.28F, 1.0F}},
    {{-0.24F, 0.72F, -0.18F}, {-0.38F, 0.58F, -0.36F}, {0.42F, 0.48F, 0.52F, 1.0F}},
    {{0.28F, 0.62F, -0.12F}, {0.46F, 0.52F, -0.28F}, {0.34F, 0.40F, 0.44F, 1.0F}},
    {{0.12F, 0.54F, 0.28F}, {0.24F, 0.46F, 0.44F}, {0.28F, 0.34F, 0.38F, 1.0F}},
    {{-0.28F, 0.58F, 0.18F}, {-0.44F, 0.48F, 0.32F}, {0.38F, 0.44F, 0.48F, 1.0F}},
};

static const uint16_t BF_ROCK_SHARD_INDICES[] = {
    0, 1, 5, 0, 5, 4,
    1, 2, 6, 1, 6, 5,
    2, 3, 7, 2, 7, 6,
    3, 0, 4, 3, 4, 7,
    4, 5, 6, 4, 6, 7,
    3, 2, 1, 3, 1, 0,
};

static const BlockfellAssetVertex BF_RUIN_TRIM_VERTICES[] = {
    {{-0.62F, 0.16F, -0.05F}, {-0.36F, 0.12F, -0.82F}, {0.30F, 0.34F, 0.36F, 1.0F}},
    {{0.62F, 0.16F, -0.05F}, {0.36F, 0.12F, -0.82F}, {0.38F, 0.42F, 0.44F, 1.0F}},
    {{0.52F, 0.44F, -0.07F}, {0.32F, 0.44F, -0.78F}, {0.50F, 0.56F, 0.60F, 1.0F}},
    {{-0.52F, 0.44F, -0.07F}, {-0.32F, 0.44F, -0.78F}, {0.40F, 0.46F, 0.50F, 1.0F}},
    {{-0.20F, 0.30F, -0.09F}, {0.0F, 0.24F, -0.90F}, {0.12F, 0.84F, 1.0F, 0.85F}},
    {{0.20F, 0.30F, -0.09F}, {0.0F, 0.24F, -0.90F}, {0.12F, 0.84F, 1.0F, 0.85F}},
    {{0.0F, 0.55F, -0.10F}, {0.0F, 0.42F, -0.88F}, {0.18F, 0.96F, 1.0F, 0.90F}},
};

static const uint16_t BF_RUIN_TRIM_INDICES[] = {0, 1, 2, 0, 2, 3, 4, 5, 6};

static const BlockfellAssetVertex BF_PATH_STONE_VERTICES[] = {
    {{-0.46F, 0.04F, -0.28F}, {-0.24F, 0.78F, -0.30F}, {0.42F, 0.38F, 0.28F, 1.0F}},
    {{0.48F, 0.04F, -0.24F}, {0.30F, 0.78F, -0.24F}, {0.56F, 0.50F, 0.34F, 1.0F}},
    {{0.38F, 0.04F, 0.32F}, {0.22F, 0.78F, 0.34F}, {0.36F, 0.34F, 0.25F, 1.0F}},
    {{-0.40F, 0.04F, 0.30F}, {-0.28F, 0.78F, 0.28F}, {0.50F, 0.44F, 0.30F, 1.0F}},
};

static const uint16_t BF_PATH_STONE_INDICES[] = {0, 1, 2, 0, 2, 3};
static const uint16_t BF_TRI_INDICES[] = {0, 1, 2};

static const BlockfellAssetVertex BF_PINE_TRUNK_VERTICES[] = {
    {{-0.13F, 0.04F, -0.12F}, {-0.54F, -0.20F, -0.52F}, {0.22F, 0.13F, 0.08F, 1.0F}},
    {{0.13F, 0.04F, -0.12F}, {0.54F, -0.20F, -0.52F}, {0.31F, 0.18F, 0.10F, 1.0F}},
    {{0.09F, 0.92F, -0.08F}, {0.44F, 0.22F, -0.48F}, {0.36F, 0.22F, 0.13F, 1.0F}},
    {{-0.09F, 0.92F, -0.08F}, {-0.44F, 0.22F, -0.48F}, {0.25F, 0.15F, 0.09F, 1.0F}},
    {{-0.12F, 0.04F, 0.12F}, {-0.50F, -0.18F, 0.54F}, {0.17F, 0.10F, 0.07F, 1.0F}},
    {{0.12F, 0.04F, 0.12F}, {0.50F, -0.18F, 0.54F}, {0.26F, 0.16F, 0.09F, 1.0F}},
    {{0.08F, 0.88F, 0.09F}, {0.42F, 0.20F, 0.50F}, {0.30F, 0.19F, 0.11F, 1.0F}},
    {{-0.08F, 0.88F, 0.09F}, {-0.42F, 0.20F, 0.50F}, {0.21F, 0.13F, 0.08F, 1.0F}},
};

static const BlockfellAssetVertex BF_MOUNTAIN_BODY_VERTICES[] = {
    {{-0.10F, 1.58F, -0.04F}, {-0.08F, 0.88F, -0.16F}, {0.48F, 0.54F, 0.60F, 1.0F}},
    {{-0.94F, 0.02F, -0.58F}, {-0.58F, -0.20F, -0.42F}, {0.26F, 0.30F, 0.35F, 1.0F}},
    {{0.92F, 0.02F, -0.50F}, {0.58F, -0.18F, -0.40F}, {0.34F, 0.38F, 0.43F, 1.0F}},
    {{0.68F, 0.02F, 0.72F}, {0.44F, -0.18F, 0.56F}, {0.20F, 0.24F, 0.30F, 1.0F}},
    {{-0.82F, 0.02F, 0.64F}, {-0.52F, -0.18F, 0.50F}, {0.30F, 0.34F, 0.39F, 1.0F}},
};

static const BlockfellAssetVertex BF_MOUNTAIN_SNOW_VERTICES[] = {
    {{-0.10F, 1.58F, -0.08F}, {-0.10F, 0.84F, -0.36F}, {0.90F, 0.96F, 1.0F, 1.0F}},
    {{-0.32F, 1.12F, -0.22F}, {-0.30F, 0.48F, -0.58F}, {0.76F, 0.86F, 0.94F, 1.0F}},
    {{0.28F, 1.08F, -0.18F}, {0.34F, 0.44F, -0.56F}, {0.96F, 1.0F, 1.0F, 1.0F}},
};

static const BlockfellAssetVertex BF_CAMP_DAIS_VERTICES[] = {
    {{-1.30F, 0.04F, -0.92F}, {-0.54F, -0.28F, -0.50F}, {0.24F, 0.18F, 0.13F, 1.0F}},
    {{1.30F, 0.04F, -0.92F}, {0.54F, -0.28F, -0.50F}, {0.38F, 0.29F, 0.20F, 1.0F}},
    {{1.18F, 0.24F, -0.82F}, {0.50F, 0.40F, -0.45F}, {0.48F, 0.38F, 0.27F, 1.0F}},
    {{-1.18F, 0.24F, -0.82F}, {-0.50F, 0.40F, -0.45F}, {0.34F, 0.26F, 0.19F, 1.0F}},
    {{-1.16F, 0.04F, 0.86F}, {-0.48F, -0.26F, 0.52F}, {0.18F, 0.13F, 0.10F, 1.0F}},
    {{1.16F, 0.04F, 0.86F}, {0.48F, -0.26F, 0.52F}, {0.30F, 0.22F, 0.15F, 1.0F}},
    {{1.04F, 0.22F, 0.74F}, {0.44F, 0.38F, 0.48F}, {0.40F, 0.30F, 0.20F, 1.0F}},
    {{-1.04F, 0.22F, 0.74F}, {-0.44F, 0.38F, 0.48F}, {0.28F, 0.20F, 0.15F, 1.0F}},
};

static const BlockfellAssetVertex BF_CAMP_CANOPY_VERTICES[] = {
    {{-1.16F, 0.20F, -1.08F}, {-0.36F, -0.20F, -0.70F}, {0.54F, 0.08F, 0.08F, 1.0F}},
    {{0.12F, 0.20F, -1.00F}, {0.14F, -0.18F, -0.76F}, {0.70F, 0.13F, 0.10F, 1.0F}},
    {{-0.20F, 0.94F, -0.62F}, {0.08F, 0.56F, -0.62F}, {0.86F, 0.20F, 0.14F, 1.0F}},
    {{-0.88F, 0.88F, -0.72F}, {-0.38F, 0.50F, -0.58F}, {0.42F, 0.05F, 0.07F, 1.0F}},
    {{0.20F, 0.20F, -0.88F}, {0.20F, -0.18F, -0.72F}, {0.46F, 0.06F, 0.07F, 1.0F}},
    {{1.26F, 0.20F, -0.64F}, {0.52F, -0.16F, -0.58F}, {0.62F, 0.10F, 0.10F, 1.0F}},
    {{0.78F, 0.86F, -0.34F}, {0.36F, 0.50F, -0.54F}, {0.76F, 0.16F, 0.12F, 1.0F}},
    {{0.42F, 0.94F, -0.64F}, {-0.04F, 0.54F, -0.64F}, {0.38F, 0.04F, 0.06F, 1.0F}},
    {{-1.12F, 1.08F, 0.76F}, {0.0F, 0.38F, 0.74F}, {0.58F, 0.08F, 0.09F, 1.0F}},
    {{1.12F, 1.08F, 0.76F}, {0.0F, 0.38F, 0.74F}, {0.76F, 0.14F, 0.12F, 1.0F}},
    {{0.98F, 0.74F, 0.68F}, {0.0F, -0.10F, 0.82F}, {0.44F, 0.05F, 0.07F, 1.0F}},
    {{-0.98F, 0.74F, 0.68F}, {0.0F, -0.10F, 0.82F}, {0.36F, 0.04F, 0.06F, 1.0F}},
};

static const uint16_t BF_CAMP_CANOPY_INDICES[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
};

static const BlockfellAssetVertex BF_CHEST_BODY_VERTICES[] = {
    {{-0.66F, 0.10F, -0.34F}, {-0.52F, -0.24F, -0.56F}, {0.34F, 0.21F, 0.12F, 1.0F}},
    {{0.66F, 0.10F, -0.34F}, {0.52F, -0.24F, -0.56F}, {0.50F, 0.33F, 0.18F, 1.0F}},
    {{0.58F, 0.56F, -0.30F}, {0.46F, 0.36F, -0.54F}, {0.62F, 0.42F, 0.22F, 1.0F}},
    {{-0.58F, 0.56F, -0.30F}, {-0.46F, 0.36F, -0.54F}, {0.42F, 0.27F, 0.15F, 1.0F}},
    {{-0.56F, 0.10F, 0.36F}, {-0.48F, -0.22F, 0.56F}, {0.22F, 0.13F, 0.08F, 1.0F}},
    {{0.56F, 0.10F, 0.36F}, {0.48F, -0.22F, 0.56F}, {0.36F, 0.22F, 0.12F, 1.0F}},
    {{0.50F, 0.52F, 0.30F}, {0.42F, 0.34F, 0.54F}, {0.46F, 0.30F, 0.18F, 1.0F}},
    {{-0.50F, 0.52F, 0.30F}, {-0.42F, 0.34F, 0.54F}, {0.30F, 0.18F, 0.10F, 1.0F}},
};

static const BlockfellAssetVertex BF_CHEST_LID_VERTICES[] = {
    {{-0.58F, 0.52F, -0.34F}, {-0.42F, 0.12F, -0.72F}, {0.16F, 0.09F, 0.06F, 1.0F}},
    {{0.58F, 0.52F, -0.34F}, {0.42F, 0.12F, -0.72F}, {0.28F, 0.16F, 0.08F, 1.0F}},
    {{0.52F, 0.82F, -0.48F}, {0.36F, 0.52F, -0.64F}, {0.82F, 0.56F, 0.16F, 1.0F}},
    {{-0.52F, 0.82F, -0.48F}, {-0.36F, 0.52F, -0.64F}, {0.62F, 0.38F, 0.10F, 1.0F}},
    {{-0.50F, 0.48F, 0.22F}, {-0.36F, 0.08F, 0.70F}, {0.12F, 0.07F, 0.05F, 1.0F}},
    {{0.50F, 0.48F, 0.22F}, {0.36F, 0.08F, 0.70F}, {0.22F, 0.13F, 0.07F, 1.0F}},
    {{0.42F, 0.74F, 0.14F}, {0.30F, 0.46F, 0.64F}, {0.56F, 0.36F, 0.12F, 1.0F}},
    {{-0.42F, 0.74F, 0.14F}, {-0.30F, 0.46F, 0.64F}, {0.42F, 0.25F, 0.08F, 1.0F}},
};

static const BlockfellAssetMesh BF_MESH_HERO_BODY = {BF_HERO_BODY_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_HERO_HEAD = {BF_HERO_HEAD_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_HERO_SWORD = {BF_HERO_SWORD_VERTICES, 5U, BF_HERO_SWORD_INDICES, 9U};
static const BlockfellAssetMesh BF_MESH_HERO_CUIRASS = {BF_HERO_CUIRASS_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_HERO_CAPE = {BF_HERO_CAPE_VERTICES, 4U, BF_QUAD_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_HERO_CREST = {BF_HERO_CREST_VERTICES, 5U, BF_PYRAMID_INDICES, 18U};
static const BlockfellAssetMesh BF_MESH_ENEMY_BODY = {BF_ENEMY_BODY_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_ENEMY_HEAD = {BF_ENEMY_HEAD_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_ENEMY_MASK = {BF_ENEMY_MASK_VERTICES, 4U, BF_ENEMY_MASK_INDICES, 12U};
static const BlockfellAssetMesh BF_MESH_ENEMY_HORNS = {BF_ENEMY_HORN_VERTICES, 6U, BF_ENEMY_HORN_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_RUNE_SPIRE = {BF_RUNE_SPIRE_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_RUNE_GLYPH = {BF_RUNE_GLYPH_VERTICES, 4U, BF_QUAD_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_GATE_KEYSTONE = {BF_GATE_KEYSTONE_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_CHEST_LOCK = {BF_CHEST_LOCK_VERTICES, 8U, BF_CHEST_LOCK_INDICES, 12U};
static const BlockfellAssetMesh BF_MESH_CAMP_STANDARD = {BF_CAMP_STANDARD_VERTICES, 7U, BF_CAMP_STANDARD_INDICES, 9U};
static const BlockfellAssetMesh BF_MESH_CAMP_DAIS = {BF_CAMP_DAIS_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_CAMP_CANOPY = {BF_CAMP_CANOPY_VERTICES, 12U, BF_CAMP_CANOPY_INDICES, 18U};
static const BlockfellAssetMesh BF_MESH_CHEST_BODY = {BF_CHEST_BODY_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_CHEST_LID = {BF_CHEST_LID_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_PINE_CROWN = {BF_PINE_CROWN_VERTICES, 10U, BF_PINE_CROWN_INDICES, 24U};
static const BlockfellAssetMesh BF_MESH_PINE_TRUNK = {BF_PINE_TRUNK_VERTICES, 8U, BF_BOX_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_ROCK_SHARD = {BF_ROCK_SHARD_VERTICES, 8U, BF_ROCK_SHARD_INDICES, 36U};
static const BlockfellAssetMesh BF_MESH_RUIN_TRIM = {BF_RUIN_TRIM_VERTICES, 7U, BF_RUIN_TRIM_INDICES, 9U};
static const BlockfellAssetMesh BF_MESH_PATH_STONE = {BF_PATH_STONE_VERTICES, 4U, BF_PATH_STONE_INDICES, 6U};
static const BlockfellAssetMesh BF_MESH_MOUNTAIN_BODY = {BF_MOUNTAIN_BODY_VERTICES, 5U, BF_PYRAMID_INDICES, 18U};
static const BlockfellAssetMesh BF_MESH_MOUNTAIN_SNOW = {BF_MOUNTAIN_SNOW_VERTICES, 3U, BF_TRI_INDICES, 3U};

#endif
