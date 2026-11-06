#include "features/localization/loc_font_packs.h"

#include <string.h>

#include "core/nt_assert.h"
#include "features/localization/loc.h"
#include "hash/nt_hash.h"
#include "nt_pack_format.h"

#define PACK_PATH_MAX 1024U
/* Seconds before a failed fetch is tried again; the wait doubles up to the cap. */
#define RETRY_FIRST_S 1.0
#define RETRY_CAP_S 8.0

typedef struct {
    nt_resource_t face;
    nt_font_t font; /* built when the language first shows */
    bool loading;
    double retry_at;
    double retry_wait;
} pack_slot_t;

static loc_font_packs_desc_t s_desc;
static pack_slot_t s_slots[LOC_FONT_PACKS_MAX];
static int s_wanted_lang;
static int s_wanted_slot;
static nt_font_t s_font;

static int slot_of(const char *code) {
    for (int index = 0; index < s_desc.count; ++index) {
        if (strcmp(s_desc.packs[index].code, code) == 0) { return index; }
    }
    return -1;
}

void loc_font_packs_init(const loc_font_packs_desc_t *desc) {
    NT_ASSERT(desc != NULL && desc->count >= 0 && desc->count <= LOC_FONT_PACKS_MAX && "loc_font_packs_init: bad pack list");
    NT_ASSERT(desc->path != NULL && desc->fallback != NULL && "loc_font_packs_init: a path callback and a fallback are required");
    s_desc = *desc;
    memset(s_slots, 0, sizeof s_slots);
    for (int index = 0; index < s_desc.count; ++index) {
        s_slots[index].face = nt_resource_request(nt_hash64_str(s_desc.packs[index].font_resource), NT_ASSET_FONT);
        s_slots[index].retry_wait = RETRY_FIRST_S;
    }
    s_wanted_lang = loc_lang_index();
    s_wanted_slot = -1;
    s_font = s_desc.common_font;
}

void loc_font_packs_shutdown(void) {
    for (int index = 0; index < s_desc.count; ++index) {
        if (s_slots[index].font.id != 0U) { nt_font_destroy(s_slots[index].font); }
    }
    memset(s_slots, 0, sizeof s_slots);
}

static void show_slot(int slot) {
    pack_slot_t *pack = &s_slots[slot];
    if (pack->font.id == 0U) {
        pack->font = nt_font_create(&s_desc.font);
        /* The language's own face first: the common face may hold a few of the
           same codepoints in another hand, for the names in a language list. */
        nt_font_add(pack->font, pack->face);
        nt_font_add(pack->font, s_desc.common_face);
    }
    s_font = pack->font;
    loc_set_lang_index(s_wanted_lang);
}

bool loc_font_packs_set_language(const char *code) {
    const int lang = code != NULL ? loc_lang_from_code(code) : -1;
    if (lang < 0) { return false; }
    s_wanted_lang = lang;
    s_wanted_slot = slot_of(code);
    if (s_wanted_slot < 0) {
        s_font = s_desc.common_font;
        loc_set_lang_index(lang);
    } else if (nt_resource_is_ready(s_slots[s_wanted_slot].face)) {
        show_slot(s_wanted_slot);
    } else {
        s_font = s_desc.common_font;
        loc_set_lang_index(loc_lang_from_code(s_desc.fallback));
    }
    return true;
}

const char *loc_font_packs_language(void) { return loc_lang_code(s_wanted_lang); }

nt_font_t loc_font_packs_font(void) { return s_font; }

static void fetch(int slot, double now) {
    pack_slot_t *pack = &s_slots[slot];
    const nt_hash32_t id = nt_hash32_str(s_desc.packs[slot].pack);
    if (pack->loading && nt_resource_pack_state(id) == NT_PACK_STATE_FAILED) {
        nt_resource_unmount(id);
        pack->loading = false;
        pack->retry_at = now + pack->retry_wait;
        pack->retry_wait = pack->retry_wait * 2.0 < RETRY_CAP_S ? pack->retry_wait * 2.0 : RETRY_CAP_S;
    }
    if (pack->loading || now < pack->retry_at) { return; }
    char path[PACK_PATH_MAX];
    if (s_desc.path(s_desc.packs[slot].pack, path, sizeof path, s_desc.user) && nt_resource_mount(id, s_desc.priority) == NT_OK) {
        if (nt_resource_load_auto(id, path) == NT_OK) {
            pack->loading = true;
        } else {
            nt_resource_unmount(id);
        }
    }
}

void loc_font_packs_step(double now) {
    if (s_wanted_slot < 0) { return; }
    if (nt_resource_is_ready(s_slots[s_wanted_slot].face)) {
        if (s_font.id != s_slots[s_wanted_slot].font.id || s_slots[s_wanted_slot].font.id == 0U) { show_slot(s_wanted_slot); }
        return;
    }
    fetch(s_wanted_slot, now);
}
