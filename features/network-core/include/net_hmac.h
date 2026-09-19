#ifndef NETWORK_CORE_NET_HMAC_H
#define NETWORK_CORE_NET_HMAC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SHA-256 and HMAC-SHA256 for authenticating small tokens between trusted
   services (a metaserver signs, a room verifies). The room builds without a
   TLS library, so the primitive lives here; it is not a general crypto API
   and offers no secrecy, only integrity under a shared key. */

#define NET_SHA256_SIZE 32U
#define NET_SHA256_BLOCK 64U

typedef struct net_sha256_t {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t block[NET_SHA256_BLOCK];
    size_t block_used;
} net_sha256_t;

void net_sha256_init(net_sha256_t *ctx);
void net_sha256_update(net_sha256_t *ctx, const void *data, size_t size);
void net_sha256_final(net_sha256_t *ctx, uint8_t digest[NET_SHA256_SIZE]);
void net_sha256(const void *data, size_t size, uint8_t digest[NET_SHA256_SIZE]);

/* Keys longer than one block are hashed first, as RFC 2104 requires. */
void net_hmac_sha256(const void *key, size_t key_size, const void *data, size_t size,
    uint8_t mac[NET_SHA256_SIZE]);

/* Time depends only on `size`, never on where the inputs differ, so a
   verifier cannot be walked byte by byte. */
bool net_equal_constant_time(const void *a, const void *b, size_t size);

#endif
