#ifndef GAME_SAVE_SEAL_H
#define GAME_SAVE_SEAL_H

/* A sealed save: ChaCha20 encryption, then HMAC-SHA256 over the result, under a
   32-byte game key. The key ships in the client, so this stops casual reading
   and editing of a save, not a player who extracts the key.

       sealed = "NTSEAL1:" base64url_nopad(nonce[12] || ciphertext || tag[32])
       enc_key = HMAC(key, "ntseal1 enc")   mac_key = HMAC(key, "ntseal1 mac")
       iv_key  = HMAC(key, "ntseal1 iv")
       nonce   = HMAC(iv_key, plaintext)[0..12)
       ciphertext = ChaCha20(enc_key, nonce, block counter from 0) ^ plaintext
       tag     = HMAC(mac_key, "NTSEAL1:" || nonce || ciphertext)

   The nonce is derived from the plaintext, so a document seals to the same
   text every time: byte comparison of sealed documents (the sync base, a
   compare-and-swap) keeps working, and two different documents never share a
   keystream. A server holding the key verifies `tag` with any standard
   HMAC-SHA256. Sealed text is ASCII with no NUL, so it travels as a C string. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_SAVE_SEAL_KEY_SIZE 32U

/* Bytes game_save_seal needs for `plain_size` bytes of plaintext, NUL included. */
size_t game_save_seal_capacity(size_t plain_size);

/* Writes the NUL-terminated sealed text; *out_size excludes the NUL. */
bool game_save_seal(const uint8_t key[GAME_SAVE_SEAL_KEY_SIZE], const char *plain, size_t plain_size, char *out,
                    size_t out_capacity, size_t *out_size);

/* Bytes game_save_unseal needs for `sealed_size` bytes of sealed text, NUL included. */
size_t game_save_unseal_capacity(size_t sealed_size);

/* Verifies the tag before anything is decrypted. On success writes the
   NUL-terminated plaintext; on failure `out` is cleared and false returned for a
   malformed text and a wrong tag alike. */
bool game_save_unseal(const uint8_t key[GAME_SAVE_SEAL_KEY_SIZE], const char *sealed, size_t sealed_size, char *out,
                      size_t out_capacity, size_t *out_size, char *error, int error_cap);

#endif /* GAME_SAVE_SEAL_H */
