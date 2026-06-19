#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#endif
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NT_PLATFORM_WEB
#include <glad/gl.h>
#endif

#define CLEAN_SEED_DEVAPI_PORT_DEFAULT 9123
#define ARENA_HALF 8.0F
#define MAX_DRONES 8
#define ROCKET_COST 120
#define CAPTURE_PATH_MAX 512

typedef enum {
  SCREEN_HANGAR = 0,
  SCREEN_BATTLE,
  SCREEN_REWARD,
  SCREEN_UPGRADE,
  SCREEN_RETEST,
} GameScreen;

typedef struct UiBox {
  float x;
  float y;
  float w;
  float h;
} UiBox;

typedef struct Drone {
  float x;
  float z;
  float hp;
  bool alive;
} Drone;

typedef struct MechGame {
  GameScreen screen;
  float mech_x;
  float mech_z;
  float heat;
  float cannon_cd;
  float rocket_cd;
  float dash_cd;
  float dash_flash;
  float rocket_flash;
  float hit_flash;
  float battle_time;
  int salvage;
  int battle_index;
  bool rockets_equipped;
  bool reward_ready;
  bool second_prompt_seen;
  Drone drones[MAX_DRONES];
} MechGame;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = CLEAN_SEED_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;
static MechGame s_game;
static UiBox s_primary_box;
static UiBox s_secondary_box;
static UiBox s_dash_box;
static UiBox s_special_box;
#if NT_DEVAPI_ENABLED
static char s_pending_capture_path[CAPTURE_PATH_MAX];
#endif

static const float COL_BG_FLOOR[4] = {0.12F, 0.15F, 0.17F, 1.0F};
static const float COL_METAL[4] = {0.52F, 0.63F, 0.70F, 1.0F};
static const float COL_METAL_DARK[4] = {0.13F, 0.17F, 0.20F, 1.0F};
static const float COL_ARMOR_BLUE[4] = {0.08F, 0.47F, 0.85F, 1.0F};
static const float COL_ARMOR_LIGHT[4] = {0.28F, 0.78F, 1.0F, 1.0F};
static const float COL_EMISSIVE[4] = {0.02F, 0.92F, 1.0F, 1.0F};
static const float COL_AMBER[4] = {1.0F, 0.48F, 0.05F, 1.0F};
static const float COL_WARNING[4] = {1.0F, 0.18F, 0.08F, 1.0F};
static const float COL_GREEN[4] = {0.18F, 0.85F, 0.38F, 1.0F};
static const float COL_WHITE[4] = {0.92F, 0.98F, 1.0F, 1.0F};
static const float COL_PANEL[4] = {0.035F, 0.105F, 0.13F, 1.0F};

static void ortho(float left, float right, float bottom, float top,
                  float near_z, float far_z, float out[16]) {
  memset(out, 0, sizeof(float) * 16);
  out[0] = 2.0F / (right - left);
  out[5] = 2.0F / (top - bottom);
  out[10] = -2.0F / (far_z - near_z);
  out[12] = -(right + left) / (right - left);
  out[13] = -(top + bottom) / (top - bottom);
  out[14] = -(far_z + near_z) / (far_z - near_z);
  out[15] = 1.0F;
}

static bool contains(UiBox box, float x, float y) {
  return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

static float len2(float x, float z) { return sqrtf((x * x) + (z * z)); }

static const char *screen_name(GameScreen screen) {
  switch (screen) {
  case SCREEN_HANGAR:
    return "hangar";
  case SCREEN_BATTLE:
    return "battle";
  case SCREEN_REWARD:
    return "reward";
  case SCREEN_UPGRADE:
    return "upgrade";
  case SCREEN_RETEST:
    return "retest";
  default:
    return "unknown";
  }
}

static void q_axis(float x, float y, float z, float angle, float out[4]) {
  glm_quatv(out, angle, (vec3){x, y, z});
}

static void rect2(float x, float y, float w, float h, const float color[4]) {
  nt_shape_renderer_rect((float[3]){x + (w * 0.5F), y + (h * 0.5F), 0.0F},
                         (float[2]){w, h}, color);
}

static void capsule2(float x, float y, float w, float h, const float color[4]) {
  const float r = h * 0.5F;
  rect2(x + r, y, w - (r * 2.0F), h, color);
  nt_shape_renderer_circle((float[3]){x + r, y + r, 0.0F}, r, color);
  nt_shape_renderer_circle((float[3]){x + w - r, y + r, 0.0F}, r, color);
}

static void button2(UiBox b, const float color[4], bool enabled) {
  const float shadow[4] = {0.0F, 0.0F, 0.0F, 0.28F};
  const float disabled[4] = {0.22F, 0.26F, 0.30F, 0.88F};
  capsule2(b.x + 5.0F, b.y + 6.0F, b.w, b.h, shadow);
  capsule2(b.x, b.y, b.w, b.h, enabled ? color : disabled);
  rect2(b.x + (b.w * 0.14F), b.y + (b.h * 0.18F), b.w * 0.72F, b.h * 0.16F,
        (float[4]){1.0F, 1.0F, 1.0F, 0.22F});
}

static void draw_segment_digit(float x, float y, float s, int digit,
                               const float color[4]) {
  static const uint8_t segs[10] = {
      0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU,
  };
  const float t = s * 0.16F;
  const float w = s * 0.62F;
  const float h = s;
  const uint8_t mask = segs[digit % 10];
  if (mask & 0x01U) {
    rect2(x + t, y, w, t, color);
  }
  if (mask & 0x02U) {
    rect2(x + w + t, y + t, t, (h * 0.5F) - t, color);
  }
  if (mask & 0x04U) {
    rect2(x + w + t, y + (h * 0.5F), t, (h * 0.5F) - t, color);
  }
  if (mask & 0x08U) {
    rect2(x + t, y + h - t, w, t, color);
  }
  if (mask & 0x10U) {
    rect2(x, y + (h * 0.5F), t, (h * 0.5F) - t, color);
  }
  if (mask & 0x20U) {
    rect2(x, y + t, t, (h * 0.5F) - t, color);
  }
  if (mask & 0x40U) {
    rect2(x + t, y + (h * 0.5F) - (t * 0.5F), w, t, color);
  }
}

static const uint8_t *glyph_rows(char c) {
  static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t glyphs[26][7] = {
      {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
      {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
      {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
      {14, 17, 16, 23, 17, 17, 14}, {17, 17, 17, 31, 17, 17, 17},
      {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},
      {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
      {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
      {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
      {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
      {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
      {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
      {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
      {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
  };
  if (c >= 'a' && c <= 'z') {
    c = (char)(c - 'a' + 'A');
  }
  if (c >= 'A' && c <= 'Z') {
    return glyphs[c - 'A'];
  }
  return blank;
}

static void draw_char(float x, float y, float scale, char c,
                      const float color[4]) {
  if (c >= '0' && c <= '9') {
    draw_segment_digit(x, y, scale * 7.0F, c - '0', color);
    return;
  }
  if (c == '-') {
    rect2(x + scale, y + (scale * 3.0F), scale * 3.0F, scale, color);
    return;
  }
  if (c == '+') {
    rect2(x + scale, y + (scale * 3.0F), scale * 3.0F, scale, color);
    rect2(x + (scale * 2.0F), y + scale, scale, scale * 5.0F, color);
    return;
  }
  if (c == ':') {
    rect2(x + (scale * 2.0F), y + (scale * 1.5F), scale, scale, color);
    rect2(x + (scale * 2.0F), y + (scale * 4.5F), scale, scale, color);
    return;
  }
  const uint8_t *rows = glyph_rows(c);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if (rows[row] & (uint8_t)(1U << (4 - col))) {
        rect2(x + ((float)col * scale), y + ((float)row * scale), scale * 0.82F,
              scale * 0.82F, color);
      }
    }
  }
}

static void draw_text(float x, float y, float scale, const char *text,
                      const float color[4]) {
  float cursor = x;
  for (const char *p = text; p && *p; ++p) {
    if (*p == ' ') {
      cursor += scale * 4.0F;
    } else {
      draw_char(cursor, y, scale, *p, color);
      cursor += scale * ((*p >= '0' && *p <= '9') ? 6.2F : 6.0F);
    }
  }
}

static void draw_int_text(float x, float y, float scale, const char *prefix,
                          int value, const float color[4]) {
  char buf[64];
  (void)snprintf(buf, sizeof(buf), "%s%d", prefix, value);
  draw_text(x, y, scale, buf, color);
}

static void parse_args(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--devapi") == 0) {
      s_devapi_enabled = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
      }
    } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
      int width = 0;
      int height = 0;
      if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 &&
          height > 0) {
        s_window_width = width;
        s_window_height = height;
      }
    }
  }
}

static void spawn_drones(int battle_index) {
  const int count = battle_index == 0 ? 4 : 6;
  for (int i = 0; i < MAX_DRONES; ++i) {
    s_game.drones[i].alive = i < count;
    s_game.drones[i].hp = battle_index == 0 ? 38.0F : 44.0F;
    s_game.drones[i].x = -4.6F + (float)(i % 3) * 2.1F;
    s_game.drones[i].z = -2.5F - (float)(i / 3) * 1.7F;
  }
}

static void reset_runtime(void) {
  memset(&s_game, 0, sizeof(s_game));
  s_game.screen = SCREEN_HANGAR;
  s_game.mech_z = 2.5F;
  s_game.salvage = 0;
  g_game_state.wallet_soft = 0;
  game_state_mark_dirty();
}

static int alive_count(void) {
  int count = 0;
  for (int i = 0; i < MAX_DRONES; ++i) {
    if (s_game.drones[i].alive) {
      count++;
    }
  }
  return count;
}

static int target_drone(void) {
  int best = -1;
  float best_d = 9999.0F;
  for (int i = 0; i < MAX_DRONES; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    const float dx = s_game.drones[i].x - s_game.mech_x;
    const float dz = s_game.drones[i].z - s_game.mech_z;
    const float d = (dx * dx) + (dz * dz);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

static void start_battle(void) {
  s_game.screen = SCREEN_BATTLE;
  s_game.mech_x = 0.0F;
  s_game.mech_z = 2.6F;
  s_game.heat = 0.05F;
  s_game.cannon_cd = 0.25F;
  s_game.rocket_cd = s_game.rockets_equipped ? 0.5F : 999.0F;
  s_game.dash_cd = 0.0F;
  s_game.dash_flash = 0.0F;
  s_game.rocket_flash = 0.0F;
  s_game.hit_flash = 0.0F;
  s_game.battle_time = 0.0F;
  s_game.reward_ready = false;
  spawn_drones(s_game.battle_index);
}

static void finish_battle(void) {
  s_game.screen = SCREEN_REWARD;
  s_game.reward_ready = true;
  s_game.salvage += s_game.battle_index == 0 ? ROCKET_COST : 75;
  g_game_state.wallet_soft = s_game.salvage;
  game_state_mark_dirty();
}

static void buy_rockets(void) {
  if (!s_game.rockets_equipped && s_game.salvage >= ROCKET_COST) {
    s_game.salvage -= ROCKET_COST;
    g_game_state.wallet_soft = s_game.salvage;
    s_game.rockets_equipped = true;
    s_game.battle_index = 1;
    s_game.screen = SCREEN_RETEST;
    game_state_mark_dirty();
  }
}

static void fire_rockets(void) {
  if (!s_game.rockets_equipped || s_game.rocket_cd > 0.0F ||
      s_game.heat > 0.82F) {
    return;
  }
  s_game.rocket_cd = 3.5F;
  s_game.heat = clampf(s_game.heat + 0.42F, 0.0F, 1.0F);
  s_game.rocket_flash = 1.45F;
  int hit = 0;
  for (int i = 0; i < MAX_DRONES && hit < 4; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    s_game.drones[i].hp -= 70.0F;
    s_game.drones[i].z -= 0.45F;
    if (s_game.drones[i].hp <= 0.0F) {
      s_game.drones[i].alive = false;
    }
    hit++;
  }
}

static void dash(void) {
  if (s_game.dash_cd > 0.0F || s_game.heat > 0.92F) {
    return;
  }
  s_game.dash_cd = 1.3F;
  s_game.dash_flash = 0.35F;
  s_game.heat = clampf(s_game.heat + 0.12F, 0.0F, 1.0F);
  s_game.mech_z -= 0.8F;
}

static void handle_button_click(void) {
  if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
    return;
  }
  for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
    const nt_pointer_t pointer = g_nt_input.pointers[i];
    if (!pointer.active) {
      continue;
    }
    if (contains(s_primary_box, pointer.x, pointer.y)) {
      if (s_game.screen == SCREEN_HANGAR || s_game.screen == SCREEN_RETEST) {
        start_battle();
      } else if (s_game.screen == SCREEN_REWARD) {
        s_game.screen =
            s_game.rockets_equipped ? SCREEN_RETEST : SCREEN_UPGRADE;
      } else if (s_game.screen == SCREEN_UPGRADE) {
        buy_rockets();
      }
    }
    if (s_game.screen == SCREEN_BATTLE &&
        contains(s_dash_box, pointer.x, pointer.y)) {
      dash();
    }
    if (s_game.screen == SCREEN_BATTLE &&
        contains(s_special_box, pointer.x, pointer.y)) {
      fire_rockets();
    }
  }
}

static void handle_input(void) {
  if (nt_input_key_is_pressed(NT_KEY_ENTER) ||
      nt_input_key_is_pressed(NT_KEY_SPACE)) {
    if (s_game.screen == SCREEN_HANGAR || s_game.screen == SCREEN_RETEST) {
      start_battle();
    } else if (s_game.screen == SCREEN_REWARD) {
      s_game.screen = s_game.rockets_equipped ? SCREEN_RETEST : SCREEN_UPGRADE;
    } else if (s_game.screen == SCREEN_UPGRADE) {
      buy_rockets();
    }
  }
  if (s_game.screen == SCREEN_BATTLE) {
    if (nt_input_key_is_pressed(NT_KEY_Q)) {
      dash();
    }
    if (nt_input_key_is_pressed(NT_KEY_E)) {
      fire_rockets();
    }
  }
  handle_button_click();
}

static void update_battle(float dt) {
  if (s_game.screen != SCREEN_BATTLE) {
    return;
  }
  float dx = 0.0F;
  float dz = 0.0F;
  if (nt_input_key_is_down(NT_KEY_A) ||
      nt_input_key_is_down(NT_KEY_ARROW_LEFT)) {
    dx -= 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_D) ||
      nt_input_key_is_down(NT_KEY_ARROW_RIGHT)) {
    dx += 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_W) || nt_input_key_is_down(NT_KEY_ARROW_UP)) {
    dz -= 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_S) ||
      nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
    dz += 1.0F;
  }
  const float l = len2(dx, dz);
  if (l > 0.001F) {
    dx /= l;
    dz /= l;
    s_game.mech_x = clampf(s_game.mech_x + (dx * dt * 4.0F), -5.8F, 5.8F);
    s_game.mech_z = clampf(s_game.mech_z + (dz * dt * 4.0F), -0.8F, 5.4F);
  }

  s_game.battle_time += dt;
  s_game.cannon_cd -= dt;
  s_game.rocket_cd -= dt;
  s_game.dash_cd -= dt;
  s_game.dash_flash = clampf(s_game.dash_flash - dt, 0.0F, 1.0F);
  s_game.rocket_flash = clampf(s_game.rocket_flash - dt, 0.0F, 1.0F);
  s_game.hit_flash = clampf(s_game.hit_flash - dt, 0.0F, 1.0F);
  s_game.heat = clampf(s_game.heat - (dt * 0.18F), 0.0F, 1.0F);

  for (int i = 0; i < MAX_DRONES; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    const float ox = s_game.mech_x - s_game.drones[i].x;
    const float oz = s_game.mech_z - s_game.drones[i].z;
    const float l2 = len2(ox, oz);
    if (l2 > 0.2F) {
      const float speed = s_game.battle_index == 0 ? 0.72F : 0.95F;
      s_game.drones[i].x += (ox / l2) * dt * speed;
      s_game.drones[i].z += (oz / l2) * dt * speed;
    }
  }

  const int target = target_drone();
  if (target >= 0 && s_game.cannon_cd <= 0.0F && s_game.heat < 0.96F) {
    s_game.cannon_cd = 0.55F;
    s_game.heat = clampf(s_game.heat + 0.055F, 0.0F, 1.0F);
    s_game.hit_flash = 0.16F;
    s_game.drones[target].hp -= s_game.rockets_equipped ? 20.0F : 17.0F;
    s_game.drones[target].z -= 0.08F;
    if (s_game.drones[target].hp <= 0.0F) {
      s_game.drones[target].alive = false;
    }
  }

  if (s_game.rockets_equipped && s_game.battle_time > 0.8F &&
      s_game.rocket_cd <= 0.0F && alive_count() >= 2) {
    fire_rockets();
  }

  if (alive_count() == 0 || s_game.battle_time > 38.0F) {
    finish_battle();
  }
}

static void layout(float w, float h) {
  const float btn_h = h < 620.0F ? 54.0F : 64.0F;
  s_primary_box =
      (UiBox){.x = w - 274.0F, .y = h - btn_h - 34.0F, .w = 230.0F, .h = btn_h};
  s_secondary_box =
      (UiBox){.x = 42.0F, .y = h - btn_h - 34.0F, .w = 190.0F, .h = btn_h};
  s_dash_box =
      (UiBox){.x = w - 244.0F, .y = h - 176.0F, .w = 88.0F, .h = 82.0F};
  s_special_box =
      (UiBox){.x = w - 132.0F, .y = h - 192.0F, .w = 96.0F, .h = 96.0F};
}

static void draw_floor_grid(float half, bool hangar) {
  const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  nt_shape_renderer_rect_rot((float[3]){0.0F, -0.02F, 0.0F},
                             (float[2]){half * 2.2F, half * 2.0F}, floor_rot,
                             COL_BG_FLOOR);
  nt_shape_renderer_rect((float[3]){0.0F, 2.7F, -half + 0.08F},
                         (float[2]){half * 2.2F, 5.4F},
                         (float[4]){0.045F, 0.075F, 0.09F, 1.0F});
  nt_shape_renderer_rect((float[3]){-half - 0.2F, 2.3F, -1.0F},
                         (float[2]){half * 1.65F, 4.6F},
                         (float[4]){0.035F, 0.058F, 0.068F, 1.0F});
  const float grid_col[4] = {0.25F, 0.36F, 0.42F, 0.65F};
  for (int i = -8; i <= 8; ++i) {
    const float p = (float)i;
    nt_shape_renderer_line((float[3]){-half, 0.01F, p},
                           (float[3]){half, 0.01F, p}, grid_col);
    nt_shape_renderer_line((float[3]){p, 0.01F, -half},
                           (float[3]){p, 0.01F, half}, grid_col);
  }
  if (hangar) {
    for (int i = -2; i <= 2; ++i) {
      const float x = (float)i * 3.0F;
      nt_shape_renderer_cube((float[3]){x, 2.1F, -6.7F},
                             (float[3]){0.32F, 4.2F, 0.32F}, COL_METAL_DARK);
      nt_shape_renderer_cube((float[3]){x, 4.3F, -6.7F},
                             (float[3]){1.5F, 0.16F, 0.22F}, COL_AMBER);
      nt_shape_renderer_line((float[3]){x - 0.52F, 4.1F, -6.55F},
                             (float[3]){x - 1.2F, 0.05F, -1.8F},
                             (float[4]){1.0F, 0.48F, 0.04F, 0.55F});
      nt_shape_renderer_line((float[3]){x + 0.52F, 4.1F, -6.55F},
                             (float[3]){x + 1.2F, 0.05F, -1.8F},
                             (float[4]){0.0F, 0.82F, 1.0F, 0.38F});
    }
  } else {
    nt_shape_renderer_cube((float[3]){-7.2F, 0.42F, -4.8F},
                           (float[3]){0.35F, 0.82F, 2.8F}, COL_METAL_DARK);
    nt_shape_renderer_cube((float[3]){7.2F, 0.42F, -4.8F},
                           (float[3]){0.35F, 0.82F, 2.8F}, COL_METAL_DARK);
    nt_shape_renderer_line((float[3]){-7.1F, 0.95F, -4.8F},
                           (float[3]){7.1F, 0.95F, -4.8F}, COL_AMBER);
  }
}

static void draw_shadow(float x, float z, float sx, float sz, float alpha) {
  const float rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  nt_shape_renderer_rect_rot((float[3]){x, 0.018F, z}, (float[2]){sx, sz}, rot,
                             (float[4]){0.0F, 0.0F, 0.0F, alpha});
}

static void draw_joint_sphere(float x, float y, float z, float r) {
  nt_shape_renderer_sphere((float[3]){x, y, z}, r, COL_METAL_DARK);
  nt_shape_renderer_sphere_wire((float[3]){x, y, z}, r * 1.03F,
                                (float[4]){0.58F, 0.78F, 0.86F, 0.55F});
}

static void draw_mech(float x, float z, float scale, bool rockets,
                      bool hangar_pose) {
  float q_y[4];
  float q_x[4];
  float q_z[4];
  q_axis(0.0F, 1.0F, 0.0F, hangar_pose ? 0.18F : -0.10F, q_y);
  q_axis(1.0F, 0.0F, 0.0F, 1.5708F, q_x);
  q_axis(0.0F, 0.0F, 1.0F, 1.5708F, q_z);

  draw_shadow(x, z + 0.08F, 2.8F * scale, 2.1F * scale, 0.38F);

  const float y0 = 0.25F;
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (1.85F * scale), z},
      (float[3]){1.35F * scale, 1.55F * scale, 0.82F * scale}, q_y,
      COL_ARMOR_BLUE);
  nt_shape_renderer_cube_wire_rot(
      (float[3]){x, y0 + (1.85F * scale), z},
      (float[3]){1.39F * scale, 1.59F * scale, 0.86F * scale}, q_y,
      (float[4]){0.78F, 0.94F, 1.0F, 0.45F});
  nt_shape_renderer_cube_rot(
      (float[3]){x - (0.42F * scale), y0 + (1.96F * scale),
                 z - (0.46F * scale)},
      (float[3]){0.33F * scale, 1.04F * scale, 0.10F * scale}, q_y,
      COL_ARMOR_LIGHT);
  nt_shape_renderer_cube_rot(
      (float[3]){x + (0.42F * scale), y0 + (1.96F * scale),
                 z - (0.46F * scale)},
      (float[3]){0.33F * scale, 1.04F * scale, 0.10F * scale}, q_y,
      COL_ARMOR_LIGHT);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (2.16F * scale), z - (0.44F * scale)},
      (float[3]){0.72F * scale, 0.28F * scale, 0.13F * scale}, q_y,
      COL_EMISSIVE);
  nt_shape_renderer_sphere(
      (float[3]){x, y0 + (2.82F * scale), z - (0.05F * scale)}, 0.38F * scale,
      COL_METAL);
  nt_shape_renderer_cube(
      (float[3]){x, y0 + (2.86F * scale), z - (0.38F * scale)},
      (float[3]){0.56F * scale, 0.12F * scale, 0.08F * scale}, COL_EMISSIVE);

  for (int side = -1; side <= 1; side += 2) {
    const float sx = x + ((float)side * 0.95F * scale);
    draw_joint_sphere(sx, y0 + (2.32F * scale), z, 0.23F * scale);
    nt_shape_renderer_capsule_rot(
        (float[3]){x + ((float)side * 1.35F * scale), y0 + (1.92F * scale), z},
        0.18F * scale, 0.88F * scale, q_z, COL_METAL);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 1.86F * scale), y0 + (1.72F * scale),
                   z - (0.08F * scale)},
        (float[3]){0.34F * scale, 0.78F * scale, 0.34F * scale},
        COL_ARMOR_LIGHT);
    nt_shape_renderer_cylinder_rot(
        (float[3]){x + ((float)side * 2.02F * scale), y0 + (1.45F * scale),
                   z - (0.38F * scale)},
        0.15F * scale, 0.92F * scale, q_x, side < 0 ? COL_METAL : COL_AMBER);

    draw_joint_sphere(x + ((float)side * 0.48F * scale), y0 + (0.95F * scale),
                      z + (0.02F * scale), 0.20F * scale);
    nt_shape_renderer_capsule((float[3]){x + ((float)side * 0.52F * scale),
                                         y0 + (0.50F * scale),
                                         z + (0.04F * scale)},
                              0.18F * scale, 0.82F * scale, COL_METAL_DARK);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 0.60F * scale), y0 + (0.10F * scale),
                   z - (0.18F * scale)},
        (float[3]){0.54F * scale, 0.24F * scale, 0.86F * scale}, COL_METAL);

    if (rockets) {
      nt_shape_renderer_cube(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.92F * scale),
                     z + (0.04F * scale)},
          (float[3]){0.36F * scale, 0.34F * scale, 0.82F * scale}, COL_AMBER);
      nt_shape_renderer_cylinder_rot(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.91F * scale),
                     z - (0.45F * scale)},
          0.11F * scale, 0.34F * scale, q_x, COL_WARNING);
    } else {
      nt_shape_renderer_cube_wire(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.92F * scale),
                     z + (0.04F * scale)},
          (float[3]){0.42F * scale, 0.38F * scale, 0.92F * scale},
          (float[4]){1.0F, 0.62F, 0.18F, 0.8F});
    }
  }

  const float glow = 0.55F + (sinf((float)g_nt_app.frame * 0.08F) * 0.25F);
  nt_shape_renderer_sphere(
      (float[3]){x, y0 + (1.62F * scale), z + (0.48F * scale)}, 0.22F * scale,
      (float[4]){0.0F, 0.95F, 1.0F, glow});
  if (s_game.dash_flash > 0.0F && !hangar_pose) {
    nt_shape_renderer_line((float[3]){x, 0.25F, z + 0.7F},
                           (float[3]){x, 0.25F, z + 2.6F},
                           (float[4]){0.0F, 0.9F, 1.0F, 1.0F});
    nt_shape_renderer_line((float[3]){x - 0.55F, 0.20F, z + 0.9F},
                           (float[3]){x - 0.55F, 0.20F, z + 2.0F},
                           (float[4]){0.0F, 0.9F, 1.0F, 0.65F});
    nt_shape_renderer_line((float[3]){x + 0.55F, 0.20F, z + 0.9F},
                           (float[3]){x + 0.55F, 0.20F, z + 2.0F},
                           (float[4]){0.0F, 0.9F, 1.0F, 0.65F});
  }
  if (s_game.rocket_flash > 0.0F && !hangar_pose) {
    nt_shape_renderer_sphere((float[3]){x - (0.62F * scale),
                                        y0 + (3.0F * scale),
                                        z - (0.48F * scale)},
                             0.18F * scale, COL_AMBER);
    nt_shape_renderer_sphere((float[3]){x + (0.62F * scale),
                                        y0 + (3.0F * scale),
                                        z - (0.48F * scale)},
                             0.18F * scale, COL_AMBER);
  }
}

static void draw_drone(const Drone *d, bool target) {
  if (!d->alive) {
    return;
  }
  draw_shadow(d->x, d->z, 0.95F, 0.72F, 0.22F);
  const float bob = sinf((float)g_nt_app.frame * 0.08F + d->x) * 0.08F;
  const float y = 0.88F + bob;
  nt_shape_renderer_sphere((float[3]){d->x, y, d->z}, 0.34F,
                           target ? COL_WARNING : COL_METAL_DARK);
  nt_shape_renderer_cube((float[3]){d->x, y, d->z - 0.34F},
                         (float[3]){0.48F, 0.16F, 0.12F}, COL_EMISSIVE);
  nt_shape_renderer_cube((float[3]){d->x - 0.48F, y, d->z},
                         (float[3]){0.48F, 0.08F, 0.16F}, COL_METAL);
  nt_shape_renderer_cube((float[3]){d->x + 0.48F, y, d->z},
                         (float[3]){0.48F, 0.08F, 0.16F}, COL_METAL);
  if (target) {
    nt_shape_renderer_circle_wire((float[3]){d->x, 0.06F, d->z}, 0.72F,
                                  COL_WARNING);
  }
}

static void draw_projectiles(void) {
  const int target = target_drone();
  if (target >= 0 && s_game.hit_flash > 0.0F) {
    nt_shape_renderer_line(
        (float[3]){s_game.mech_x + 1.9F, 1.72F, s_game.mech_z - 0.38F},
        (float[3]){s_game.drones[target].x, 0.9F, s_game.drones[target].z},
        COL_AMBER);
    nt_shape_renderer_sphere(
        (float[3]){s_game.drones[target].x, 0.9F, s_game.drones[target].z},
        0.28F, COL_AMBER);
  }
  if (s_game.rocket_flash > 0.0F) {
    const float fade = clampf(s_game.rocket_flash / 1.45F, 0.0F, 1.0F);
    for (int i = 0; i < MAX_DRONES; ++i) {
      if (s_game.drones[i].alive) {
        nt_shape_renderer_line(
            (float[3]){s_game.mech_x - 0.45F, 3.1F, s_game.mech_z - 0.4F},
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            COL_AMBER);
        nt_shape_renderer_line(
            (float[3]){s_game.mech_x + 0.45F, 3.1F, s_game.mech_z - 0.4F},
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            (float[4]){1.0F, 0.24F, 0.02F, 1.0F});
        nt_shape_renderer_sphere(
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            0.34F + (0.38F * fade), COL_AMBER);
      }
    }
    nt_shape_renderer_line(
        (float[3]){s_game.mech_x - 0.45F, 3.1F, s_game.mech_z - 0.4F},
        (float[3]){s_game.mech_x - 2.5F, 2.0F, s_game.mech_z - 5.0F},
        COL_AMBER);
    nt_shape_renderer_line(
        (float[3]){s_game.mech_x + 0.45F, 3.1F, s_game.mech_z - 0.4F},
        (float[3]){s_game.mech_x + 2.5F, 2.0F, s_game.mech_z - 5.0F},
        COL_AMBER);
  }
}

static void setup_perspective(float w, float h, bool hangar) {
  const float aspect = h > 0.0F ? w / h : 1.777F;
  const vec3 eye = {hangar ? 4.4F : 5.4F, hangar ? 4.1F : 6.0F,
                    hangar ? 8.4F : 9.4F};
  const vec3 center = {0.0F, hangar ? 1.55F : 0.95F, hangar ? 0.0F : 0.2F};
  const vec3 up = {0.0F, 1.0F, 0.0F};
  mat4 view;
  mat4 proj;
  mat4 vp;
  glm_lookat((vec3){eye[0], eye[1], eye[2]},
             (vec3){center[0], center[1], center[2]},
             (vec3){up[0], up[1], up[2]}, view);
  glm_perspective(glm_rad(hangar ? 54.0F : 58.0F), aspect, 0.1F, 80.0F, proj);
  glm_mat4_mul(proj, view, vp);
  nt_shape_renderer_set_vp((float *)vp);
  nt_shape_renderer_set_cam_pos((float[3]){eye[0], eye[1], eye[2]});
  nt_shape_renderer_set_depth(true);
}

static void setup_ortho(float w, float h) {
  float vp[16];
  ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);
  nt_shape_renderer_set_vp(vp);
  nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
  nt_shape_renderer_set_depth(false);
}

static void draw_world(float w, float h) {
  const bool hangar = s_game.screen != SCREEN_BATTLE;
  setup_perspective(w, h, hangar);
  draw_floor_grid(ARENA_HALF, hangar);
  if (s_game.screen == SCREEN_BATTLE) {
    draw_mech(s_game.mech_x, s_game.mech_z, 0.88F, s_game.rockets_equipped,
              false);
    const int target = target_drone();
    for (int i = 0; i < MAX_DRONES; ++i) {
      draw_drone(&s_game.drones[i], i == target);
    }
    draw_projectiles();
  } else {
    draw_mech(0.0F, 0.4F, 1.18F, s_game.rockets_equipped, true);
    if (s_game.screen == SCREEN_UPGRADE || s_game.screen == SCREEN_REWARD ||
        s_game.screen == SCREEN_RETEST) {
      nt_shape_renderer_cube_wire((float[3]){1.8F, 3.7F, 0.4F},
                                  (float[3]){1.1F, 0.72F, 1.25F}, COL_AMBER);
      nt_shape_renderer_cube((float[3]){3.2F, 0.45F, -1.2F},
                             (float[3]){1.2F, 0.28F, 1.2F}, COL_METAL_DARK);
      nt_shape_renderer_cube((float[3]){3.2F, 0.96F, -1.2F},
                             (float[3]){0.9F, 0.62F, 0.9F},
                             s_game.rockets_equipped ? COL_GREEN : COL_AMBER);
    }
  }
}

static void draw_cooling_meter(float x, float y, float w, float h) {
  capsule2(x, y, w, h, (float[4]){0.03F, 0.08F, 0.10F, 0.9F});
  const float cool = 1.0F - s_game.heat;
  const float color_hot[4] = {1.0F, 0.25F, 0.08F, 1.0F};
  const float color_cool[4] = {0.0F, 0.86F, 1.0F, 1.0F};
  capsule2(x, y, w * cool, h, s_game.heat > 0.72F ? color_hot : color_cool);
  draw_text(x, y - 23.0F, 3.0F, "COOLING", COL_WHITE);
}

static void draw_hud(float w, float h) {
  setup_ortho(w, h);
  rect2(0.0F, 0.0F, w, 78.0F, (float[4]){0.0F, 0.02F, 0.03F, 0.72F});
  draw_text(34.0F, 24.0F, 4.2F, "MECH BUILDER BATTLER", COL_WHITE);
  draw_int_text(w - 250.0F, 24.0F, 4.0F, "SALVAGE ", s_game.salvage, COL_AMBER);

  if (s_game.screen == SCREEN_BATTLE) {
    draw_text(42.0F, 94.0F, 3.0F, "WASD MOVE  Q DASH  E ROCKETS", COL_WHITE);
    draw_int_text(42.0F, 128.0F, 3.6F, "DRONES ", alive_count(), COL_GREEN);
    draw_cooling_meter(w * 0.5F - 160.0F, h - 54.0F, 320.0F, 18.0F);
    button2(s_dash_box, COL_ARMOR_BLUE, s_game.dash_cd <= 0.0F);
    draw_text(s_dash_box.x + 18.0F, s_dash_box.y + 31.0F, 3.2F, "DASH",
              COL_WHITE);
    button2(s_special_box, COL_AMBER,
            s_game.rockets_equipped && s_game.rocket_cd <= 0.0F);
    draw_text(s_special_box.x + 12.0F, s_special_box.y + 38.0F, 2.7F,
              s_game.rockets_equipped ? "ROCKET" : "LOCK", COL_WHITE);
    nt_shape_renderer_circle((float[3]){117.0F, h - 111.0F, 0.0F}, 72.0F,
                             (float[4]){0.025F, 0.12F, 0.16F, 1.0F});
    nt_shape_renderer_circle_wire((float[3]){117.0F, h - 111.0F, 0.0F}, 57.0F,
                                  (float[4]){0.35F, 0.88F, 1.0F, 0.92F});
    nt_shape_renderer_circle((float[3]){117.0F, h - 111.0F, 0.0F}, 18.0F,
                             (float[4]){0.0F, 0.72F, 0.9F, 1.0F});
    draw_text(88.0F, h - 122.0F, 2.4F, "WASD", COL_WHITE);
  } else if (s_game.screen == SCREEN_HANGAR) {
    draw_text(46.0F, 98.0F, 4.0F, "HANGAR", COL_AMBER);
    draw_text(46.0F, 138.0F, 3.0F, "ONE MECH  ONE NEXT ACTION", COL_WHITE);
    button2(s_primary_box, COL_GREEN, true);
    draw_text(s_primary_box.x + 48.0F, s_primary_box.y + 23.0F, 4.2F, "BATTLE",
              COL_WHITE);
    rect2(w - 314.0F, 104.0F, 270.0F, 104.0F, COL_PANEL);
    draw_text(w - 286.0F, 126.0F, 3.0F, "SHOULDER MODULE", COL_AMBER);
    draw_text(w - 286.0F, 162.0F, 2.7F,
              s_game.rockets_equipped ? "ROCKETS EQUIPPED"
                                      : "LOCKED  WIN SALVAGE",
              COL_WHITE);
  } else if (s_game.screen == SCREEN_REWARD) {
    rect2(w * 0.5F - 250.0F, 102.0F, 500.0F, 170.0F, COL_PANEL);
    draw_text(w * 0.5F - 182.0F, 130.0F, 5.0F, "REWARD", COL_AMBER);
    draw_text(w * 0.5F - 188.0F, 190.0F, 4.0F,
              s_game.battle_index == 0 ? "SALVAGE +120" : "SALVAGE +75",
              COL_WHITE);
    button2(s_primary_box, COL_GREEN, true);
    draw_text(s_primary_box.x + 30.0F, s_primary_box.y + 23.0F, 3.8F,
              s_game.rockets_equipped ? "CONTINUE" : "UPGRADE", COL_WHITE);
  } else if (s_game.screen == SCREEN_UPGRADE) {
    rect2(w * 0.5F - 292.0F, 98.0F, 584.0F, 200.0F, COL_PANEL);
    draw_text(w * 0.5F - 226.0F, 126.0F, 4.3F, "BUY SHOULDER ROCKETS",
              COL_AMBER);
    draw_text(w * 0.5F - 188.0F, 184.0F, 3.6F, "COST 120 SALVAGE", COL_WHITE);
    button2(s_primary_box,
            s_game.salvage >= ROCKET_COST ? COL_GREEN : COL_WARNING,
            s_game.salvage >= ROCKET_COST);
    draw_text(s_primary_box.x + 50.0F, s_primary_box.y + 23.0F, 4.0F, "ATTACH",
              COL_WHITE);
  } else if (s_game.screen == SCREEN_RETEST) {
    rect2(w * 0.5F - 320.0F, 104.0F, 640.0F, 170.0F, COL_PANEL);
    draw_text(w * 0.5F - 250.0F, 132.0F, 4.4F, "ROCKETS ATTACHED", COL_GREEN);
    draw_text(w * 0.5F - 286.0F, 190.0F, 3.4F, "TEST THEM AGAINST DRONES",
              COL_WHITE);
    button2(s_primary_box, COL_AMBER, true);
    draw_text(s_primary_box.x + 56.0F, s_primary_box.y + 23.0F, 4.0F, "RETEST",
              COL_WHITE);
  }
}

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static bool write_framebuffer_ppm(const char *path, int width, int height) {
  if (!path || !path[0] || width <= 0 || height <= 0) {
    return false;
  }
  const size_t row_bytes = (size_t)width * 3U;
  const size_t total = row_bytes * (size_t)height;
  unsigned char *pixels = (unsigned char *)malloc(total);
  if (!pixels) {
    return false;
  }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

  FILE *file = fopen(path, "wb");
  if (!file) {
    free(pixels);
    return false;
  }
  (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
  for (int y = height - 1; y >= 0; --y) {
    const unsigned char *row = pixels + ((size_t)y * row_bytes);
    if (fwrite(row, 1, row_bytes, file) != row_bytes) {
      fclose(file);
      free(pixels);
      return false;
    }
  }
  fclose(file);
  free(pixels);
  return true;
}

static void maybe_capture_framebuffer(void) {
  if (!s_pending_capture_path[0]) {
    return;
  }
  const int width =
      (int)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const int height =
      (int)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
  (void)write_framebuffer_ppm(s_pending_capture_path, width, height);
  s_pending_capture_path[0] = '\0';
}
#else
static void maybe_capture_framebuffer(void) {}
#endif

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static bool emit_state(cJSON *result_obj) {
  cJSON_AddStringToObject(result_obj, "runtime", "mech_builder_battler");
  cJSON_AddStringToObject(result_obj, "screen", screen_name(s_game.screen));
  cJSON_AddNumberToObject(result_obj, "salvage", (double)s_game.salvage);
  cJSON_AddBoolToObject(result_obj, "rockets_equipped",
                        s_game.rockets_equipped);
  cJSON_AddNumberToObject(result_obj, "battle_index",
                          (double)s_game.battle_index);
  cJSON_AddNumberToObject(result_obj, "alive_drones", (double)alive_count());
  cJSON_AddNumberToObject(result_obj, "heat", (double)s_game.heat);
  cJSON_AddNumberToObject(result_obj, "mech_x", (double)s_game.mech_x);
  cJSON_AddNumberToObject(result_obj, "mech_z", (double)s_game.mech_z);
  return true;
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj,
                          nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  return emit_state(result_obj);
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj,
                                   nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  reset_runtime();
  return emit_state(result_obj);
}

static bool ep_game_action_start_battle(const cJSON *params, cJSON *result_obj,
                                        nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  start_battle();
  return emit_state(result_obj);
}

static bool ep_game_action_use_special(const cJSON *params, cJSON *result_obj,
                                       nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  fire_rockets();
  return emit_state(result_obj);
}

static bool ep_game_action_buy_rockets(const cJSON *params, cJSON *result_obj,
                                       nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  buy_rockets();
  return emit_state(result_obj);
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON *result_obj,
                                        nt_devapi_error *err, void *user) {
  (void)user;
  const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
  if (!cJSON_IsString(output) || !output->valuestring ||
      !output->valuestring[0]) {
    err->code = "bad_params";
    err->message = "output path is required";
    return false;
  }
#ifdef NT_PLATFORM_WEB
  err->code = "unsupported";
  err->message = "framebuffer capture is native-only in this prototype";
  return false;
#else
  (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s",
                 output->valuestring);
  s_pending_capture_path[sizeof(s_pending_capture_path) - 1] = '\0';
  cJSON_AddStringToObject(result_obj, "output", s_pending_capture_path);
  cJSON_AddStringToObject(result_obj, "status", "scheduled_next_frame");
  return true;
#endif
}

static void register_game_endpoints(void) {
  game_state_register_devapi();
  static const nt_devapi_command_desc descs[] = {
      {"game.state", "game", "Return the mech playable slice state.", "",
       "state object", "immediate", "none"},
      {"game.reset_playtest", "game", "Reset the mech playable slice.", "",
       "state object", "immediate", "mutates state"},
      {"game.action.start_battle", "game", "Start the current mech battle.", "",
       "state object", "immediate", "mutates state"},
      {"game.action.use_special", "game", "Fire shoulder rockets if available.",
       "", "state object", "immediate", "mutates state"},
      {"game.action.buy_rockets", "game",
       "Buy and equip shoulder rockets when affordable.", "", "state object",
       "immediate", "mutates state"},
      {"game.capture.framebuffer", "game",
       "Capture the native framebuffer to a PPM file.", "output",
       "{output,status}", "next-frame", "writes file"},
  };
  (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
  (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
  (void)nt_devapi_register(&descs[2], ep_game_action_start_battle, NULL);
  (void)nt_devapi_register(&descs[3], ep_game_action_use_special, NULL);
  (void)nt_devapi_register(&descs[4], ep_game_action_buy_rockets, NULL);
  (void)nt_devapi_register(&descs[5], ep_game_capture_framebuffer, NULL);
  game_devapi_ui_register();
}

static void register_ui_devapi(float w, float h) {
  game_devapi_ui_clear();
  (void)game_devapi_ui_register_node(
      "root", "", "screen", "Mech Builder Battler", screen_name(s_game.screen),
      0.0F, 0.0F, w, h, true, true);
  if (s_game.screen == SCREEN_HANGAR) {
    (void)game_devapi_ui_register_node(
        "action.battle", "root", "button", "Battle", "Start battle",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
    (void)game_devapi_ui_register_node(
        "slot.shoulder", "root", "slot", "Shoulder Module",
        s_game.rockets_equipped ? "Rockets equipped" : "Locked", w - 314.0F,
        104.0F, 270.0F, 104.0F, true, false);
  } else if (s_game.screen == SCREEN_BATTLE) {
    (void)game_devapi_ui_register_node(
        "action.dash", "root", "button", "Dash", "Q", s_dash_box.x,
        s_dash_box.y, s_dash_box.w, s_dash_box.h, true, s_game.dash_cd <= 0.0F);
    (void)game_devapi_ui_register_node(
        "action.rockets", "root", "button", "Rockets", "E", s_special_box.x,
        s_special_box.y, s_special_box.w, s_special_box.h, true,
        s_game.rockets_equipped);
    (void)game_devapi_ui_register_node(
        "meter.cooling", "root", "meter", "Cooling", "Heat limiter",
        w * 0.5F - 160.0F, h - 54.0F, 320.0F, 18.0F, true, true);
  } else if (s_game.screen == SCREEN_REWARD) {
    (void)game_devapi_ui_register_node(
        "action.reward_continue", "root", "button", "Continue", "Go to upgrade",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
  } else if (s_game.screen == SCREEN_UPGRADE) {
    (void)game_devapi_ui_register_node(
        "action.attach_rockets", "root", "button", "Attach Rockets",
        "Cost 120 salvage", s_primary_box.x, s_primary_box.y, s_primary_box.w,
        s_primary_box.h, true, s_game.salvage >= ROCKET_COST);
  } else if (s_game.screen == SCREEN_RETEST) {
    (void)game_devapi_ui_register_node(
        "action.retest", "root", "button", "Retest", "Test rockets",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
  }
}
#endif

static void frame(void) {
  nt_window_poll();
#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    nt_devapi_update();
  }
#endif
  nt_input_poll();

  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  layout(w, h);
  handle_input();
  update_battle(g_nt_app.dt);

#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    register_ui_devapi(w, h);
  }
#endif

#ifndef NT_PLATFORM_WEB
  if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
    nt_app_quit();
  }
#endif

  nt_gfx_begin_frame();
  if (g_nt_gfx.context_restored) {
    nt_shape_renderer_restore_gpu();
  }
  nt_gfx_begin_pass(&(nt_pass_desc_t){
      .clear_color = {0.035F, 0.065F, 0.085F, 1.0F}, .clear_depth = 1.0F});
  draw_world(w, h);
  nt_shape_renderer_flush();
  draw_hud(w, h);
  nt_shape_renderer_flush();
  maybe_capture_framebuffer();
  nt_gfx_end_pass();
  nt_gfx_end_frame();
  nt_window_swap_buffers();
}

int main(int argc, char **argv) {
  nt_engine_config_t config = {0};
  config.app_name = "Mech Builder Battler";
  config.version = 1;
  if (nt_engine_init(&config) != NT_OK) {
    return 1;
  }

  parse_args(argc, argv);
  reset_runtime();

  g_nt_window.title = "Mech Builder Battler";
  g_nt_window.width = (uint32_t)s_window_width;
  g_nt_window.height = (uint32_t)s_window_height;
  nt_window_init();
  nt_input_init();

  nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
  gfx_desc.depth = true;
  nt_gfx_init(&gfx_desc);
  nt_shape_renderer_init();

#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    if (nt_devapi_init() != NT_OK) {
      (void)fprintf(stderr, "Failed to init DevAPI\n");
      s_devapi_enabled = false;
    } else {
      register_game_endpoints();
      if (!nt_devapi_net_start(s_devapi_port)) {
        (void)fprintf(stderr, "Failed to start DevAPI on port %u\n",
                      (unsigned)s_devapi_port);
      }
    }
  }
#endif

#ifdef NT_PLATFORM_WEB
  nt_platform_web_loading_complete();
#endif

  g_nt_app.target_dt = 1.0F / 60.0F;
  nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    nt_devapi_net_stop();
    nt_devapi_shutdown();
  }
#endif
  nt_shape_renderer_shutdown();
  nt_gfx_shutdown();
  nt_input_shutdown();
  nt_window_shutdown();
  nt_engine_shutdown();
#endif

  return 0;
}
