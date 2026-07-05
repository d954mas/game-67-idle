#include <stdbool.h>
#include <string.h>

static bool s_shop_open;
static bool s_world_map_open;
static char s_shop_id[64];

bool shop_screen_open(void) { return s_shop_open; }

void shop_screen_set_open(bool open) {
  s_shop_open = open;
  if (!open) {
    s_shop_id[0] = '\0';
  }
}

bool shop_screen_open_shop(const char *shop_id) {
  if (!shop_id || strcmp(shop_id, "shop_post_trader_basic") != 0) {
    return false;
  }
  (void)strcpy(s_shop_id, shop_id);
  s_shop_open = true;
  return true;
}

bool world_map_screen_open(void) { return s_world_map_open; }

void world_map_screen_set_open(bool open) { s_world_map_open = open; }

void world_map_screen_open_map(void) { s_world_map_open = true; }
