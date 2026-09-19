#include "net_hmac.h"

#include <string.h>

/* FIPS 180-4 SHA-256, straight from the specification; no table beyond the
   round constants, no platform intrinsics: the inputs are tens of bytes. */

static const uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32U - n)); }

static uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void store_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void compress(uint32_t state[8], const uint8_t block[NET_SHA256_BLOCK]) {
    uint32_t w[64];
    for (size_t i = 0U; i < 16U; ++i) { w[i] = load_be32(block + i * 4U); }
    for (size_t i = 16U; i < 64U; ++i) {
        const uint32_t s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3);
        const uint32_t s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (size_t i = 0U; i < 64U; ++i) {
        const uint32_t S1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + S1 + ch + K[i] + w[i];
        const uint32_t S0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void net_sha256_init(net_sha256_t *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };
    memcpy(ctx->state, initial, sizeof initial);
    ctx->total_bytes = 0U;
    ctx->block_used = 0U;
}

void net_sha256_update(net_sha256_t *ctx, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    ctx->total_bytes += size;
    while (size > 0U) {
        const size_t room = NET_SHA256_BLOCK - ctx->block_used;
        const size_t take = size < room ? size : room;
        memcpy(ctx->block + ctx->block_used, bytes, take);
        ctx->block_used += take;
        bytes += take;
        size -= take;
        if (ctx->block_used == NET_SHA256_BLOCK) {
            compress(ctx->state, ctx->block);
            ctx->block_used = 0U;
        }
    }
}

void net_sha256_final(net_sha256_t *ctx, uint8_t digest[NET_SHA256_SIZE]) {
    const uint64_t bits = ctx->total_bytes * 8U;
    const uint8_t one = 0x80U;
    net_sha256_update(ctx, &one, 1U);
    /* Pad with zeros up to the 8 length bytes that end a block. */
    static const uint8_t zeros[NET_SHA256_BLOCK] = {0};
    const size_t pad = (ctx->block_used <= NET_SHA256_BLOCK - 8U)
        ? NET_SHA256_BLOCK - 8U - ctx->block_used
        : NET_SHA256_BLOCK * 2U - 8U - ctx->block_used;
    net_sha256_update(ctx, zeros, pad);
    uint8_t length[8];
    store_be32(length, (uint32_t)(bits >> 32));
    store_be32(length + 4, (uint32_t)bits);
    net_sha256_update(ctx, length, sizeof length);
    for (size_t i = 0U; i < 8U; ++i) { store_be32(digest + i * 4U, ctx->state[i]); }
    memset(ctx, 0, sizeof *ctx);
}

void net_sha256(const void *data, size_t size, uint8_t digest[NET_SHA256_SIZE]) {
    net_sha256_t ctx;
    net_sha256_init(&ctx);
    net_sha256_update(&ctx, data, size);
    net_sha256_final(&ctx, digest);
}

void net_hmac_sha256(const void *key, size_t key_size, const void *data, size_t size,
    uint8_t mac[NET_SHA256_SIZE]) {
    uint8_t key_block[NET_SHA256_BLOCK] = {0};
    if (key_size > NET_SHA256_BLOCK) {
        net_sha256(key, key_size, key_block);
    } else {
        memcpy(key_block, key, key_size);
    }
    uint8_t pad[NET_SHA256_BLOCK];
    net_sha256_t ctx;
    for (size_t i = 0U; i < NET_SHA256_BLOCK; ++i) { pad[i] = key_block[i] ^ 0x36U; }
    net_sha256_init(&ctx);
    net_sha256_update(&ctx, pad, sizeof pad);
    net_sha256_update(&ctx, data, size);
    uint8_t inner[NET_SHA256_SIZE];
    net_sha256_final(&ctx, inner);
    for (size_t i = 0U; i < NET_SHA256_BLOCK; ++i) { pad[i] = key_block[i] ^ 0x5cU; }
    net_sha256_init(&ctx);
    net_sha256_update(&ctx, pad, sizeof pad);
    net_sha256_update(&ctx, inner, sizeof inner);
    net_sha256_final(&ctx, mac);
    memset(key_block, 0, sizeof key_block);
    memset(pad, 0, sizeof pad);
}

bool net_equal_constant_time(const void *a, const void *b, size_t size) {
    const volatile uint8_t *x = (const volatile uint8_t *)a;
    const volatile uint8_t *y = (const volatile uint8_t *)b;
    uint8_t diff = 0U;
    for (size_t i = 0U; i < size; ++i) { diff |= (uint8_t)(x[i] ^ y[i]); }
    return diff == 0U;
}
