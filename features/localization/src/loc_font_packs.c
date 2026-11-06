#include "features/localization/loc_font_packs.h"

#include <string.h>

#include "core/nt_assert.h"
#include "features/localization/loc.h"
#include "hash/nt_hash.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

static loc_font_packs_desc_t s_desc;
static nt_resource_t s_face;
static nt_font_t s_pack_font; /* built when a pack language first shows */
static int s_wanted_lang;
static bool s_wants_pack;
static nt_font_t s_font;

static bool in_pack(const char *code) {
    for (int index = 0; index < s_desc.count; ++index) {
        if (strcmp(s_desc.languages[index], code) == 0) { return true; }
    }
    return false;
}

void loc_font_packs_init(const loc_font_packs_desc_t *desc) {
    NT_ASSERT(desc != NULL && desc->count >= 0 && desc->font_resource != NULL && desc->fallback != NULL && "loc_font_packs_init: bad desc");
    s_desc = *desc;
    s_face = nt_resource_request(nt_hash64_str(s_desc.font_resource), NT_ASSET_FONT);
    s_pack_font = (nt_font_t){0};
    s_wanted_lang = loc_lang_index();
    s_wants_pack = false;
    s_font = s_desc.common_font;
}

void loc_font_packs_shutdown(void) {
    if (s_pack_font.id != 0U) { nt_font_destroy(s_pack_font); }
    s_pack_font = (nt_font_t){0};
}

static void show_pack_language(void) {
    if (s_pack_font.id == 0U) {
        s_pack_font = nt_font_create(&s_desc.font);
        /* The pack's face first: the common face may hold a few of the same
           codepoints in another hand, for the names in a language list. */
        nt_font_add(s_pack_font, s_face);
        nt_font_add(s_pack_font, s_desc.common_face);
    }
    s_font = s_pack_font;
    loc_set_lang_index(s_wanted_lang);
}

bool loc_font_packs_set_language(const char *code) {
    const int lang = code != NULL ? loc_lang_from_code(code) : -1;
    if (lang < 0) { return false; }
    s_wanted_lang = lang;
    s_wants_pack = in_pack(code);
    if (!s_wants_pack) {
        s_font = s_desc.common_font;
        loc_set_lang_index(lang);
    } else if (nt_resource_is_ready(s_face)) {
        show_pack_language();
    } else {
        s_font = s_desc.common_font;
        loc_set_lang_index(loc_lang_from_code(s_desc.fallback));
    }
    return true;
}

const char *loc_font_packs_language(void) { return loc_lang_code(s_wanted_lang); }

nt_font_t loc_font_packs_font(void) { return s_font; }

void loc_font_packs_step(void) {
    if (s_wants_pack && (s_font.id != s_pack_font.id || s_pack_font.id == 0U) && nt_resource_is_ready(s_face)) { show_pack_language(); }
}
