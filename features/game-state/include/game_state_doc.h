#ifndef GAME_STATE_DOC_H
#define GAME_STATE_DOC_H

/* Instance documents: an NTGS document over caller-owned fragment states, for a
   process that holds many profiles at once (a room server, a solo client's
   local and cloud copies). Nothing here touches the game_save singleton.

       NTGS 1
       save_version=2
       save_id="00c0ffee00c0ffee"
       rev=17

       [progress 2]
       xp=1200

   save_id names the profile's lineage (set once when a profile is created) and
   rev counts its stored writes; both are the caller's to maintain. Fragments are
   generated with `generate_state.py --instance`, which emits the descriptor. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "game_save.h"
#include "game_save_text.h"

typedef struct game_state_doc_fragment {
    const char *id;
    int version;                    /* = steps count + 1 */
    const GameSaveMigrateFn *steps; /* steps[v-1]: fragment v -> v+1; NULL at version 1 */
    size_t state_size;
    void (*reset)(void *state);
    bool (*validate)(const void *state, char *error, int error_cap);
    bool (*write_text)(const void *state, game_save_text_writer_t *writer);
    bool (*from_text)(void *state, const char *text, size_t size, char *error, int error_cap);
} game_state_doc_fragment_t;

typedef struct game_state_doc_schema {
    const game_state_doc_fragment_t *const *fragments; /* written in this order */
    int fragment_count;
    int save_version;                               /* = document steps count + 1 */
    const GameSaveDocumentMigrateFn *document_steps; /* steps[v-1]: features v -> v+1 */
} game_state_doc_schema_t;

typedef struct game_state_doc_header {
    int save_version;
    uint64_t save_id;
    int64_t rev; /* >= 0 */
} game_state_doc_header_t;

/* states[i] belongs to schema->fragments[i]. The document is stamped with the
   schema's save_version whatever header->save_version holds. Every state is
   validated first: a document this build wrote is one it can read back. */
bool game_state_doc_write(const game_state_doc_schema_t *schema, const game_state_doc_header_t *header,
                          const void *const *states, game_save_text_writer_t *writer, char *error, int error_cap);

/* Reads a current document: its save_version and every fragment version must
   match the schema (migrate first). A fragment the document lacks reads as its
   defaults; a fragment the schema lacks is an error, because only a migration
   may drop one. On failure every state is reset to its defaults. */
bool game_state_doc_read(const game_state_doc_schema_t *schema, const char *text, size_t size,
                         game_state_doc_header_t *header, void *const *states, char *error, int error_cap);

/* The header alone, without parsing any fragment. */
bool game_state_doc_read_header(const char *text, size_t size, game_state_doc_header_t *header, char *error,
                                int error_cap);

/* Pure: rewrites a document of any older save_version or fragment version as a
   current one, running document steps first and then each fragment's steps,
   exactly as the game_save load path orders them. save_id and rev pass through.
   A current document is copied byte for byte, so migrating twice changes
   nothing, and so is every fragment that is current and no document step
   changed. A fragment a step does touch travels through cJSON doubles, as on the
   game_save path; if it holds an integer of magnitude 2^53 or more the migration fails with
   an error naming the field rather than write a document that reads back wrong. */
bool game_state_doc_migrate(const game_state_doc_schema_t *schema, const char *text, size_t size,
                            game_save_text_writer_t *out, char *error, int error_cap);

#endif /* GAME_STATE_DOC_H */
