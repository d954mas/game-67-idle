#include "game_save_seal.h"

#include "game_state_json.h"

#include <string.h>

/* SHA-256 (FIPS 180-4), HMAC (RFC 2104) and ChaCha20 (RFC 8439), from the
   specifications. game-state is L0 and carries its own copy rather than link a
   network or TLS layer for a save. */

#define SHA_BLOCK 64U
#define SHA_SIZE 32U
#define NONCE_SIZE 12U
#define TAG_SIZE 32U
#define PREFIX "NTSEAL1:"
#define PREFIX_SIZE (sizeof(PREFIX) - 1U)

typedef struct {
    uint32_t state[8];
    uint64_t total;
    uint8_t block[SHA_BLOCK];
    size_t used;
} sha256_t;

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
static uint32_t rotl(uint32_t x, unsigned n) { return (x << n) | (x >> (32U - n)); }

static uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t load_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void store_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void sha256_compress(sha256_t *ctx, const uint8_t block[SHA_BLOCK]) {
    uint32_t w[64];
    for (unsigned i = 0; i < 16U; i++) w[i] = load_be32(block + 4U * i);
    for (unsigned i = 16; i < 64U; i++) {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (unsigned i = 0; i < 64U; i++) {
        const uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_t *ctx) {
    static const uint32_t iv[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                   0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    memcpy(ctx->state, iv, sizeof iv);
    ctx->total = 0;
    ctx->used = 0;
}

static void sha256_update(sha256_t *ctx, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    ctx->total += size;
    while (size > 0U) {
        const size_t take = SHA_BLOCK - ctx->used < size ? SHA_BLOCK - ctx->used : size;
        memcpy(ctx->block + ctx->used, bytes, take);
        ctx->used += take;
        bytes += take;
        size -= take;
        if (ctx->used == SHA_BLOCK) {
            sha256_compress(ctx, ctx->block);
            ctx->used = 0;
        }
    }
}

static void sha256_final(sha256_t *ctx, uint8_t digest[SHA_SIZE]) {
    const uint64_t bits = ctx->total * 8U;
    const uint8_t pad = 0x80U;
    const uint8_t zero = 0U;
    sha256_update(ctx, &pad, 1U);
    while (ctx->used != SHA_BLOCK - 8U) sha256_update(ctx, &zero, 1U);
    uint8_t length[8];
    for (unsigned i = 0; i < 8U; i++) length[i] = (uint8_t)(bits >> (56U - 8U * i));
    sha256_update(ctx, length, sizeof length);
    for (unsigned i = 0; i < 8U; i++) store_be32(digest + 4U * i, ctx->state[i]);
}

typedef struct {
    sha256_t inner;
    uint8_t outer_key[SHA_BLOCK];
} hmac_t;

/* Keys here are 32 bytes or short labels, never longer than a block. */
static void hmac_init(hmac_t *ctx, const uint8_t *key, size_t key_size) {
    uint8_t inner_key[SHA_BLOCK] = {0};
    memset(ctx->outer_key, 0, sizeof ctx->outer_key);
    memcpy(inner_key, key, key_size);
    memcpy(ctx->outer_key, key, key_size);
    for (unsigned i = 0; i < SHA_BLOCK; i++) {
        inner_key[i] ^= 0x36U;
        ctx->outer_key[i] ^= 0x5cU;
    }
    sha256_init(&ctx->inner);
    sha256_update(&ctx->inner, inner_key, sizeof inner_key);
}

static void hmac_final(hmac_t *ctx, uint8_t mac[SHA_SIZE]) {
    uint8_t inner_digest[SHA_SIZE];
    sha256_final(&ctx->inner, inner_digest);
    sha256_t outer;
    sha256_init(&outer);
    sha256_update(&outer, ctx->outer_key, sizeof ctx->outer_key);
    sha256_update(&outer, inner_digest, sizeof inner_digest);
    sha256_final(&outer, mac);
}

static void derive(const uint8_t key[GAME_SAVE_SEAL_KEY_SIZE], const char *label, uint8_t out[SHA_SIZE]) {
    hmac_t ctx;
    hmac_init(&ctx, key, GAME_SAVE_SEAL_KEY_SIZE);
    sha256_update(&ctx.inner, label, strlen(label));
    hmac_final(&ctx, out);
}

#define QR(a, b, c, d)                                                                                                \
    do {                                                                                                               \
        a += b;                                                                                                        \
        d = rotl(d ^ a, 16);                                                                                           \
        c += d;                                                                                                        \
        b = rotl(b ^ c, 12);                                                                                           \
        a += b;                                                                                                        \
        d = rotl(d ^ a, 8);                                                                                            \
        c += d;                                                                                                        \
        b = rotl(b ^ c, 7);                                                                                            \
    } while (0)

static void chacha20_block(const uint8_t key[32], uint32_t counter, const uint8_t nonce[NONCE_SIZE],
                           uint8_t out[64]) {
    uint32_t input[16] = {0x61707865U, 0x3320646eU, 0x79622d32U, 0x6b206574U};
    for (unsigned i = 0; i < 8U; i++) input[4 + i] = load_le32(key + 4U * i);
    input[12] = counter;
    for (unsigned i = 0; i < 3U; i++) input[13 + i] = load_le32(nonce + 4U * i);
    uint32_t x[16];
    memcpy(x, input, sizeof x);
    for (unsigned round = 0; round < 10U; round++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for (unsigned i = 0; i < 16U; i++) store_le32(out + 4U * i, x[i] + input[i]);
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

typedef struct {
    char *out;
    size_t used;
    uint8_t pending[3];
    unsigned pending_count;
} b64_writer_t;

/* n bytes of a group encode to n + 1 digits; no padding. */
static void b64_emit(b64_writer_t *w) {
    const uint32_t v = ((uint32_t)w->pending[0] << 16) | ((uint32_t)w->pending[1] << 8) | w->pending[2];
    for (unsigned i = 0; i <= w->pending_count; i++) w->out[w->used++] = B64[(v >> (18U - 6U * i)) & 63U];
    w->pending[0] = w->pending[1] = w->pending[2] = 0;
    w->pending_count = 0;
}

static void b64_put(b64_writer_t *w, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        w->pending[w->pending_count++] = data[i];
        if (w->pending_count == 3U) b64_emit(w);
    }
}

static void b64_finish(b64_writer_t *w) {
    if (w->pending_count > 0U) b64_emit(w);
}

static size_t b64_size(size_t raw) { return raw / 3U * 4U + (raw % 3U ? raw % 3U + 1U : 0U); }

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

size_t game_save_seal_capacity(size_t plain_size) {
    return PREFIX_SIZE + b64_size(NONCE_SIZE + plain_size + TAG_SIZE) + 1U;
}

bool game_save_seal(const uint8_t key[GAME_SAVE_SEAL_KEY_SIZE], const char *plain, size_t plain_size, char *out,
                    size_t out_capacity, size_t *out_size) {
    if (key == NULL || plain == NULL || out == NULL || out_capacity < game_save_seal_capacity(plain_size)) {
        return false;
    }
    uint8_t enc_key[SHA_SIZE], mac_key[SHA_SIZE], iv_key[SHA_SIZE], iv[SHA_SIZE];
    derive(key, "ntseal1 enc", enc_key);
    derive(key, "ntseal1 mac", mac_key);
    derive(key, "ntseal1 iv", iv_key);
    hmac_t ctx;
    hmac_init(&ctx, iv_key, sizeof iv_key);
    sha256_update(&ctx.inner, plain, plain_size);
    hmac_final(&ctx, iv);

    memcpy(out, PREFIX, PREFIX_SIZE);
    b64_writer_t writer = {.out = out, .used = PREFIX_SIZE};
    hmac_t mac;
    hmac_init(&mac, mac_key, sizeof mac_key);
    sha256_update(&mac.inner, PREFIX, PREFIX_SIZE);
    sha256_update(&mac.inner, iv, NONCE_SIZE);
    b64_put(&writer, iv, NONCE_SIZE);
    uint8_t stream[64];
    for (size_t offset = 0; offset < plain_size; offset += 64U) {
        chacha20_block(enc_key, (uint32_t)(offset / 64U), iv, stream);
        const size_t n = plain_size - offset < 64U ? plain_size - offset : 64U;
        for (size_t i = 0; i < n; i++) stream[i] ^= (uint8_t)plain[offset + i];
        sha256_update(&mac.inner, stream, n);
        b64_put(&writer, stream, n);
    }
    uint8_t tag[TAG_SIZE];
    hmac_final(&mac, tag);
    b64_put(&writer, tag, TAG_SIZE);
    b64_finish(&writer);
    out[writer.used] = '\0';
    if (out_size != NULL) *out_size = writer.used;
    return true;
}

size_t game_save_unseal_capacity(size_t sealed_size) {
    return sealed_size > PREFIX_SIZE ? (sealed_size - PREFIX_SIZE) * 3U / 4U + 1U : 1U;
}

bool game_save_unseal(const uint8_t key[GAME_SAVE_SEAL_KEY_SIZE], const char *sealed, size_t sealed_size, char *out,
                      size_t out_capacity, size_t *out_size, char *error, int error_cap) {
    if (out != NULL && out_capacity > 0U) out[0] = '\0';
    if (key == NULL || sealed == NULL || out == NULL || sealed_size < PREFIX_SIZE ||
        memcmp(sealed, PREFIX, PREFIX_SIZE) != 0) {
        gsj_set_error(error, error_cap, "not a sealed save");
        return false;
    }
    const size_t digits = sealed_size - PREFIX_SIZE;
    const size_t raw_size = digits / 4U * 3U + (digits % 4U ? digits % 4U - 1U : 0U);
    if (digits % 4U == 1U || raw_size < NONCE_SIZE + TAG_SIZE ||
        out_capacity < raw_size - NONCE_SIZE - TAG_SIZE + 1U) {
        gsj_set_error(error, error_cap, "not a sealed save");
        return false;
    }
    const size_t plain_size = raw_size - NONCE_SIZE - TAG_SIZE;
    uint8_t nonce[NONCE_SIZE], tag[TAG_SIZE];
    size_t raw = 0;
    uint32_t acc = 0;
    unsigned bits = 0;
    for (size_t i = 0; i < digits; i++) {
        const int v = b64_value(sealed[PREFIX_SIZE + i]);
        if (v < 0) {
            gsj_set_error(error, error_cap, "not a sealed save");
            return false;
        }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6U;
        if (bits >= 8U) {
            bits -= 8U;
            const uint8_t byte = (uint8_t)(acc >> bits);
            if (raw < NONCE_SIZE) {
                nonce[raw] = byte;
            } else if (raw < NONCE_SIZE + plain_size) {
                out[raw - NONCE_SIZE] = (char)byte;
            } else {
                tag[raw - NONCE_SIZE - plain_size] = byte;
            }
            raw++;
        }
    }
    /* Leftover bits must be zero, so one document has exactly one sealed text. */
    if ((acc & ((1U << bits) - 1U)) != 0U) {
        memset(out, 0, plain_size + 1U);
        gsj_set_error(error, error_cap, "not a sealed save");
        return false;
    }
    uint8_t enc_key[SHA_SIZE], mac_key[SHA_SIZE], expected[TAG_SIZE];
    derive(key, "ntseal1 enc", enc_key);
    derive(key, "ntseal1 mac", mac_key);
    hmac_t mac;
    hmac_init(&mac, mac_key, sizeof mac_key);
    sha256_update(&mac.inner, PREFIX, PREFIX_SIZE);
    sha256_update(&mac.inner, nonce, NONCE_SIZE);
    sha256_update(&mac.inner, out, plain_size);
    hmac_final(&mac, expected);
    uint8_t diff = 0;
    for (unsigned i = 0; i < TAG_SIZE; i++) diff |= (uint8_t)(expected[i] ^ tag[i]);
    if (diff != 0U) {
        memset(out, 0, plain_size + 1U);
        gsj_set_error(error, error_cap, "sealed save failed verification");
        return false;
    }
    uint8_t stream[64];
    for (size_t offset = 0; offset < plain_size; offset += 64U) {
        chacha20_block(enc_key, (uint32_t)(offset / 64U), nonce, stream);
        const size_t n = plain_size - offset < 64U ? plain_size - offset : 64U;
        for (size_t i = 0; i < n; i++) out[offset + i] = (char)((uint8_t)out[offset + i] ^ stream[i]);
    }
    out[plain_size] = '\0';
    if (out_size != NULL) *out_size = plain_size;
    return true;
}
