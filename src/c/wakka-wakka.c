#include <pebble.h>

#define MAP_W 19
#define MAP_H 21
#define TILE 14
#define HUD_H 22
#define SIM_STEP_MS 60
#define RENDER_STEP_MS 50
#define FIXED_ONE 256
#define SUBTILES 8
#define MOVE_STEP 2
#define ACCEL_TRIGGER 140
#define ACCEL_RELEASE 80
#define ACCEL_DWELL 3
#define GHOST_COUNT 4
#define DEATH_TICKS 18
#define POWER_TICKS 140
#define POWER_FLASH_TICKS 36
#define READY_TICKS 34
#define WW_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WW_MAX(a, b) ((a) > (b) ? (a) : (b))
#define WW_ABS(a) ((a) < 0 ? -(a) : (a))
#define WW_CLAMP(v, lo, hi) (WW_MIN(WW_MAX((v), (lo)), (hi)))

typedef enum {
  DIR_NONE,
  DIR_UP,
  DIR_RIGHT,
  DIR_DOWN,
  DIR_LEFT
} Direction;

typedef struct {
  int16_t x;
  int16_t y;
  Direction direction;
  GColor color;
} Ghost;

static const char *const MAP[MAP_H] = {
  "###################",
  "#o...............o#",
  "#.###.#.###.###.#.#",
  "#.......#.........#",
  "#.#.###.#.###.#.#.#",
  "#.#.......#.......#",
  "#.###.#####.#.#.###",
  "#...#.............#",
  "#.#.#.########.##.#",
  "#.#.#.#XXXXXX#....#",
  "......#XXXXXX#.##..",
  "#.#.#.########....#",
  "#...#..........##.#",
  "#.#.#.#.#.#####...#",
  "#.#.#.#.#...#...#.#",
  "#.........#...#...#",
  "#.#.#.###.#.#.#.#.#",
  "#.#......P......#.#",
  "#.#.#.#.#####.###.#",
  "#o...............o#",
  "###################"
};

static Window *s_window;
static Layer *s_game_layer;
static Layer *s_hud_layer;
static Layer *s_home_layer;
static AppTimer *s_timer;
static uint8_t s_tiles[MAP_H][MAP_W];
static int16_t s_px;
static int16_t s_py;
static int16_t s_prev_px;
static int16_t s_prev_py;
static int16_t s_prev_ghost_x[GHOST_COUNT];
static int16_t s_prev_ghost_y[GHOST_COUNT];
static int32_t s_camera_x_fixed;
static int32_t s_camera_y_fixed;
static int16_t s_score;
static int16_t s_pellets;
static Direction s_direction;
static Direction s_wanted;
static bool s_paused = true;
static bool s_calibrating = true;
static int32_t s_base_x;
static int32_t s_base_y;
static int32_t s_filter_x;
static int32_t s_filter_y;
static int16_t s_calibration_samples;
static Direction s_accel_candidate;
static int8_t s_candidate_count;
static bool s_backlight_enabled = true;
static bool s_home_backlight_enabled;
static uint8_t s_light_notice_ticks;
static bool s_home = true;
static bool s_game_over;
static int8_t s_lives = 3;
static uint8_t s_respawn_ticks;
static Ghost s_ghosts[GHOST_COUNT];
static uint8_t s_death_ticks;
static uint8_t s_power_ticks;
static uint8_t s_ready_ticks;
static AppTimer *s_backlight_off_timer;
static uint32_t s_last_timer_ms;
static uint16_t s_sim_accumulator_ms;
static GFont s_font_18;
static GFont s_font_18_bold;
static GFont s_font_24_bold;
static char s_hud_text[24];
static int16_t s_hud_score = -1;
static int8_t s_hud_lives = -1;
static bool s_hud_power;
static bool s_hud_paused;
static int8_t s_hud_light_state = -1;

static uint32_t clock_ms(void) {
  time_t seconds;
  uint16_t milliseconds;
  time_ms(&seconds, &milliseconds);
  return (uint32_t)seconds * 1000 + milliseconds;
}

static bool backlight_requested(void) {
  if (s_home) {
    return s_home_backlight_enabled;
  }
  return s_backlight_enabled && !s_game_over &&
         !s_paused && !s_calibrating;
}

static void update_backlight(void) {
  light_enable(backlight_requested());
}

static void backlight_off_handler(void *context) {
  s_backlight_off_timer = NULL;
  if (!backlight_requested()) {
    // Override the button's timed interaction light, then release it off.
    light_enable(true);
    light_enable(false);
  }
}

static int dir_dx(Direction dir) {
  return dir == DIR_RIGHT ? 1 : dir == DIR_LEFT ? -1 : 0;
}

static int dir_dy(Direction dir) {
  return dir == DIR_DOWN ? 1 : dir == DIR_UP ? -1 : 0;
}

static void snapshot_actor_positions(void) {
  s_prev_px = s_px;
  s_prev_py = s_py;
  for (int i = 0; i < GHOST_COUNT; ++i) {
    s_prev_ghost_x[i] = s_ghosts[i].x;
    s_prev_ghost_y[i] = s_ghosts[i].y;
  }
}

static int32_t interpolated_position(int16_t previous, int16_t current,
                                     bool horizontal_wrap) {
  int32_t delta = current - previous;
  int32_t wrap_width = MAP_W * SUBTILES;
  if (horizontal_wrap && WW_ABS(delta) > wrap_width / 2) {
    delta += delta < 0 ? wrap_width : -wrap_width;
  }
  return (int32_t)previous * FIXED_ONE +
         delta * FIXED_ONE * s_sim_accumulator_ms / SIM_STEP_MS;
}

static Direction opposite(Direction dir);

static bool is_wall(int x, int y) {
  if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) {
    return true;
  }
  return s_tiles[y][x] == '#' || s_tiles[y][x] == 'X';
}

static bool can_move(Direction dir) {
  int tx = s_px / SUBTILES;
  int ty = s_py / SUBTILES;
  int next_x = tx + dir_dx(dir);
  int next_y = ty + dir_dy(dir);
  if (ty == 10 && next_y == 10) {
    if (next_x < 0) {
      next_x = MAP_W - 1;
    } else if (next_x >= MAP_W) {
      next_x = 0;
    }
  }
  return dir != DIR_NONE && !is_wall(next_x, next_y);
}

static bool can_move_from(int16_t px, int16_t py, Direction dir) {
  int tx = px / SUBTILES;
  int ty = py / SUBTILES;
  int next_x = tx + dir_dx(dir);
  int next_y = ty + dir_dy(dir);
  if (ty == 10 && next_y == 10) {
    if (next_x < 0) {
      next_x = MAP_W - 1;
    } else if (next_x >= MAP_W) {
      next_x = 0;
    }
  }
  return dir != DIR_NONE && !is_wall(next_x, next_y);
}

static void request_direction(Direction dir) {
  if (dir != DIR_NONE) {
    s_wanted = dir;
  }
}

static void reset_calibration(void) {
  s_calibrating = true;
  s_calibration_samples = 0;
  s_base_x = s_base_y = 0;
  s_filter_x = s_filter_y = 0;
  s_accel_candidate = DIR_NONE;
  s_candidate_count = 0;
}

static void reset_ghost(int index) {
  static const int8_t spawn_x[GHOST_COUNT] = {7, 9, 11, 13};
  static const int8_t spawn_y[GHOST_COUNT] = {7, 7, 7, 7};
  GColor color = GColorRed;
  if (index == 1) color = GColorCyan;
  if (index == 2) color = GColorMagenta;
  if (index == 3) color = GColorOrange;
  s_ghosts[index] = (Ghost) {
    .x = spawn_x[index] * SUBTILES,
    .y = spawn_y[index] * SUBTILES,
    .direction = index % 2 == 0 ? DIR_LEFT : DIR_RIGHT,
    .color = color
  };
  s_prev_ghost_x[index] = s_ghosts[index].x;
  s_prev_ghost_y[index] = s_ghosts[index].y;
}

static void reset_actors(void) {
  s_px = 9 * SUBTILES;
  s_py = 17 * SUBTILES;
  s_direction = DIR_LEFT;
  s_wanted = DIR_LEFT;
  s_prev_px = s_px;
  s_prev_py = s_py;
  for (int i = 0; i < GHOST_COUNT; ++i) {
    reset_ghost(i);
  }
  s_death_ticks = 0;
  s_power_ticks = 0;
  s_ready_ticks = READY_TICKS;
  s_respawn_ticks = 24;
}

static void load_level(void) {
  s_pellets = 0;
  for (int y = 0; y < MAP_H; ++y) {
    for (int x = 0; x < MAP_W; ++x) {
      char tile = MAP[y][x];
      if (tile == 'P') {
        tile = ' ';
      }
      s_tiles[y][x] = tile;
      if (tile == '.' || tile == 'o') {
        ++s_pellets;
      }
    }
  }
  s_score = 0;
  reset_actors();
}

static void collect_tile(void) {
  int tx = s_px / SUBTILES;
  int ty = s_py / SUBTILES;
  if (s_tiles[ty][tx] == '.') {
    s_tiles[ty][tx] = ' ';
    s_score += 10;
    --s_pellets;
  } else if (s_tiles[ty][tx] == 'o') {
    s_tiles[ty][tx] = ' ';
    s_score += 50;
    --s_pellets;
    s_power_ticks = POWER_TICKS;
    for (int i = 0; i < GHOST_COUNT; ++i) {
      s_ghosts[i].direction = opposite(s_ghosts[i].direction);
    }
  }
}

static void update_camera(GRect bounds) {
  int32_t player_x = interpolated_position(s_prev_px, s_px, true);
  int32_t player_y = interpolated_position(s_prev_py, s_py, false);
  int32_t world_x = player_x * TILE / SUBTILES +
                    TILE * FIXED_ONE / 2;
  int32_t world_y = player_y * TILE / SUBTILES +
                    TILE * FIXED_ONE / 2;
  int32_t look_x = dir_dx(s_direction) * TILE * 2 * FIXED_ONE;
  int32_t look_y = dir_dy(s_direction) * TILE * 2 * FIXED_ONE;
  int32_t target_x = world_x + look_x -
                     bounds.size.w * FIXED_ONE / 2;
  int32_t target_y = world_y + look_y -
                     bounds.size.h * FIXED_ONE / 2;
  int32_t max_x = (MAP_W * TILE - bounds.size.w) * FIXED_ONE;
  int32_t max_y = (MAP_H * TILE - bounds.size.h) * FIXED_ONE;
  target_x = WW_CLAMP(target_x, 0, WW_MAX(0, max_x));
  target_y = WW_CLAMP(target_y, 0, WW_MAX(0, max_y));
  s_camera_x_fixed += (target_x - s_camera_x_fixed) / 4;
  s_camera_y_fixed += (target_y - s_camera_y_fixed) / 4;
}

static Direction opposite(Direction dir) {
  if (dir == DIR_UP) return DIR_DOWN;
  if (dir == DIR_DOWN) return DIR_UP;
  if (dir == DIR_LEFT) return DIR_RIGHT;
  if (dir == DIR_RIGHT) return DIR_LEFT;
  return DIR_NONE;
}

static void move_ghost(Ghost *ghost, int index) {
  bool centered = (ghost->x % SUBTILES == 0) &&
                  (ghost->y % SUBTILES == 0);
  if (centered) {
    Direction choices[4];
    int choice_count = 0;
    Direction reverse = opposite(ghost->direction);
    const Direction directions[] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};
    for (int i = 0; i < 4; ++i) {
      Direction candidate = directions[i];
      if (can_move_from(ghost->x, ghost->y, candidate) &&
          candidate != reverse) {
        choices[choice_count++] = candidate;
      }
    }
    if (choice_count == 0 && can_move_from(ghost->x, ghost->y, reverse)) {
      choices[choice_count++] = reverse;
    }

    int target_x = s_px / SUBTILES;
    int target_y = s_py / SUBTILES;
    if (index == 1) {
      target_x += dir_dx(s_direction) * 4;
      target_y += dir_dy(s_direction) * 4;
    } else if (index == 2) {
      int ahead_x = target_x + dir_dx(s_direction) * 2;
      int ahead_y = target_y + dir_dy(s_direction) * 2;
      target_x = ahead_x * 2 - s_ghosts[0].x / SUBTILES;
      target_y = ahead_y * 2 - s_ghosts[0].y / SUBTILES;
    } else if (index == 3) {
      int gx_now = ghost->x / SUBTILES;
      int gy_now = ghost->y / SUBTILES;
      int pac_distance = WW_ABS(target_x - gx_now) +
                         WW_ABS(target_y - gy_now);
      if (pac_distance <= 8) {
        target_x = 3;
        target_y = 19;
      }
    }
    int best_distance = s_power_ticks > 0 ? -1 : 32767;
    Direction best = ghost->direction;
    int gx = ghost->x / SUBTILES;
    int gy = ghost->y / SUBTILES;
    for (int i = 0; i < choice_count; ++i) {
      int nx = gx + dir_dx(choices[i]);
      int ny = gy + dir_dy(choices[i]);
      int distance = WW_ABS(target_x - nx) + WW_ABS(target_y - ny);
      if ((s_power_ticks > 0 && distance > best_distance) ||
          (s_power_ticks == 0 && distance < best_distance)) {
        best_distance = distance;
        best = choices[i];
      }
    }
    ghost->direction = best;
  }

  ghost->x += dir_dx(ghost->direction);
  ghost->y += dir_dy(ghost->direction);
  if (ghost->y / SUBTILES == 10) {
    if (ghost->x < 0) {
      ghost->x += MAP_W * SUBTILES;
    } else if (ghost->x >= MAP_W * SUBTILES) {
      ghost->x -= MAP_W * SUBTILES;
    }
  }
}

static void lose_life(void) {
  s_power_ticks = 0;
  --s_lives;
  if (s_lives <= 0) {
    s_game_over = true;
    s_paused = true;
    update_backlight();
  } else {
    reset_actors();
  }
}

static void begin_death(void) {
  s_death_ticks = DEATH_TICKS;
  s_direction = DIR_NONE;
  s_wanted = DIR_NONE;
}

static void game_step(void) {
  if (s_home || s_game_over || s_paused || s_calibrating) {
    return;
  }
  if (s_death_ticks > 0) {
    if (--s_death_ticks == 0) {
      lose_life();
    }
    return;
  }
  if (s_ready_ticks > 0) {
    --s_ready_ticks;
    return;
  }

  bool centered = (s_px % SUBTILES == 0) && (s_py % SUBTILES == 0);
  if (centered) {
    collect_tile();
    if (can_move(s_wanted)) {
      s_direction = s_wanted;
    }
    if (!can_move(s_direction)) {
      s_direction = DIR_NONE;
    }
  }

  s_px += dir_dx(s_direction) * MOVE_STEP;
  s_py += dir_dy(s_direction) * MOVE_STEP;
  if (s_py / SUBTILES == 10) {
    if (s_px < 0) {
      s_px += MAP_W * SUBTILES;
    } else if (s_px >= MAP_W * SUBTILES) {
      s_px -= MAP_W * SUBTILES;
    }
  }

  if (s_pellets <= 0) {
    load_level();
  }

  for (int i = 0; i < GHOST_COUNT; ++i) {
    move_ghost(&s_ghosts[i], i);
  }
  if (s_power_ticks > 0) {
    --s_power_ticks;
  }
  if (s_respawn_ticks > 0) {
    --s_respawn_ticks;
  } else {
    for (int i = 0; i < GHOST_COUNT; ++i) {
      if (WW_ABS(s_px - s_ghosts[i].x) <= 5 &&
          WW_ABS(s_py - s_ghosts[i].y) <= 5) {
        if (s_power_ticks > 0) {
          s_score += 200;
          reset_ghost(i);
        } else {
          begin_death();
        }
        break;
      }
    }
  }
}

static void draw_direction_arrow(GContext *ctx, int x, int y, Direction dir) {
  if (dir == DIR_NONE) {
    return;
  }
  GPoint tip = GPoint(x + dir_dx(dir) * 6, y + dir_dy(dir) * 6);
  GPoint a;
  GPoint b;
  if (dir == DIR_LEFT || dir == DIR_RIGHT) {
    a = GPoint(x - dir_dx(dir) * 3, y - 4);
    b = GPoint(x - dir_dx(dir) * 3, y + 4);
  } else {
    a = GPoint(x - 4, y - dir_dy(dir) * 3);
    b = GPoint(x + 4, y - dir_dy(dir) * 3);
  }
  graphics_context_set_stroke_color(ctx, GColorCyan);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, tip, a);
  graphics_draw_line(ctx, tip, b);
}

static void draw_pac(GContext *ctx, int x, int y) {
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, GPoint(x, y), 6);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(x, y),
                     GPoint(x + dir_dx(s_direction) * 7,
                            y + dir_dy(s_direction) * 7));
}

static void draw_death(GContext *ctx, int x, int y) {
  int radius = WW_MAX(1, (s_death_ticks * 6) / DEATH_TICKS);
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, GPoint(x, y), radius);
  graphics_context_set_stroke_color(ctx, GColorYellow);
  graphics_context_set_stroke_width(ctx, 1);
  int ray = 3 + (DEATH_TICKS - s_death_ticks) / 3;
  graphics_draw_line(ctx, GPoint(x - ray, y), GPoint(x - ray - 4, y));
  graphics_draw_line(ctx, GPoint(x + ray, y), GPoint(x + ray + 4, y));
  graphics_draw_line(ctx, GPoint(x, y - ray), GPoint(x, y - ray - 4));
  graphics_draw_line(ctx, GPoint(x, y + ray), GPoint(x, y + ray + 4));
}

static void draw_ghost(GContext *ctx, int x, int y, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(x, y - 2), 6);
  graphics_fill_rect(ctx, GRect(x - 6, y - 2, 12, 8), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(x - 2, y - 2), 2);
  graphics_fill_circle(ctx, GPoint(x + 3, y - 2), 2);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(x - 2, y - 2), 1);
  graphics_fill_circle(ctx, GPoint(x + 3, y - 2), 1);
}

static const char *hud_text(void) {
  int8_t light_state = s_light_notice_ticks > 0
                           ? (s_backlight_enabled ? 1 : 0)
                           : -1;
  bool power = s_power_ticks > 0;
  if (s_hud_score == s_score && s_hud_lives == s_lives &&
      s_hud_power == power && s_hud_paused == s_paused &&
      s_hud_light_state == light_state) {
    return s_hud_text;
  }

  s_hud_score = s_score;
  s_hud_lives = s_lives;
  s_hud_power = power;
  s_hud_paused = s_paused;
  s_hud_light_state = light_state;
  if (light_state >= 0) {
    snprintf(s_hud_text, sizeof(s_hud_text), "LIGHT %s",
             light_state ? "ON" : "OFF");
  } else if (s_paused) {
    snprintf(s_hud_text, sizeof(s_hud_text), "PAUSED");
  } else {
    snprintf(s_hud_text, sizeof(s_hud_text), "%d x%d%s",
             s_score, s_lives, power ? " PWR" : "");
  }
  return s_hud_text;
}

static bool hud_state_changed(void) {
  int8_t light_state = s_light_notice_ticks > 0
                           ? (s_backlight_enabled ? 1 : 0)
                           : -1;
  return s_hud_score != s_score || s_hud_lives != s_lives ||
         s_hud_power != (s_power_ticks > 0) ||
         s_hud_paused != s_paused ||
         s_hud_light_state != light_state;
}

static void draw_home(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, GColorYellow);
  graphics_draw_text(ctx, "WAKKA WAKKA",
                     s_font_24_bold,
                     GRect(4, 15, bounds.size.w - 8, 32),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  draw_pac(ctx, 70, 70);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int x = 91; x <= 119; x += 14) {
    graphics_fill_circle(ctx, GPoint(x, 70), 2);
  }
  draw_ghost(ctx, 141, 70, GColorRed);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx,
                     "TILT TO TURN\nUP: LIGHT\nSELECT: PAUSE",
                     s_font_18,
                     GRect(8, 91, bounds.size.w - 16, 72),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  graphics_context_set_fill_color(ctx, GColorBlueMoon);
  graphics_fill_rect(ctx, GRect(27, 174, bounds.size.w - 54, 36), 5,
                     GCornersAll);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "SELECT TO START",
                     s_font_18_bold,
                     GRect(30, 179, bounds.size.w - 60, 26),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void home_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  draw_home(ctx, bounds);
}

static void hud_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "WAKKA", s_font_18_bold,
                     GRect(4, -2, 62, HUD_H), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, hud_text(), s_font_18,
                     GRect(66, -2, bounds.size.w - 70, HUD_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);
}

static void game_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  update_camera(bounds);
  int camera_x = s_camera_x_fixed / FIXED_ONE;
  int camera_y = s_camera_y_fixed / FIXED_ONE;
  graphics_context_set_stroke_color(ctx, GColorBlueMoon);
  graphics_context_set_stroke_width(ctx, 2);

  int first_col = WW_MAX(0, camera_x / TILE);
  int last_col =
      WW_MIN(MAP_W - 1, (camera_x + bounds.size.w - 1) / TILE);
  int first_row = WW_MAX(0, camera_y / TILE);
  int last_row = WW_MIN(MAP_H - 1,
      (camera_y + bounds.size.h - 1) / TILE);
  for (int y = first_row; y <= last_row; ++y) {
    for (int x = first_col; x <= last_col; ++x) {
      int sx = x * TILE - camera_x;
      int sy = y * TILE - camera_y;
      uint8_t tile = s_tiles[y][x];
      if (tile == '#') {
        graphics_draw_rect(ctx, GRect(sx + 2, sy + 2, TILE - 4, TILE - 4));
      } else if (tile == '.') {
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, GPoint(sx + TILE / 2, sy + TILE / 2), 1);
      } else if (tile == 'o') {
        graphics_context_set_fill_color(ctx, GColorChromeYellow);
        graphics_fill_circle(ctx, GPoint(sx + TILE / 2, sy + TILE / 2), 3);
      }
    }
  }
  int32_t pac_world_x =
      interpolated_position(s_prev_px, s_px, true) * TILE / SUBTILES;
  int32_t pac_world_y =
      interpolated_position(s_prev_py, s_py, false) * TILE / SUBTILES;
  int pac_x = pac_world_x / FIXED_ONE + TILE / 2 - camera_x;
  int pac_y = pac_world_y / FIXED_ONE + TILE / 2 - camera_y;
  for (int i = 0; i < GHOST_COUNT; ++i) {
    int32_t ghost_world_x =
        interpolated_position(s_prev_ghost_x[i], s_ghosts[i].x, true) *
        TILE / SUBTILES;
    int32_t ghost_world_y =
        interpolated_position(s_prev_ghost_y[i], s_ghosts[i].y, false) *
        TILE / SUBTILES;
    int ghost_x = ghost_world_x / FIXED_ONE + TILE / 2 - camera_x;
    int ghost_y = ghost_world_y / FIXED_ONE + TILE / 2 - camera_y;
    if (ghost_x < -7 || ghost_x > bounds.size.w + 7 ||
        ghost_y < -7 || ghost_y > bounds.size.h + 7) {
      continue;
    }
    GColor ghost_color = s_ghosts[i].color;
    if (s_power_ticks > 0) {
      bool flashing = s_power_ticks < POWER_FLASH_TICKS &&
                      ((s_power_ticks / 6) % 2 == 0);
      ghost_color = flashing ? GColorWhite : GColorBlue;
    }
    draw_ghost(ctx, ghost_x, ghost_y, ghost_color);
  }
  if (s_death_ticks > 0) {
    draw_death(ctx, pac_x, pac_y);
  } else {
    draw_pac(ctx, pac_x, pac_y);
    if (s_wanted != s_direction) {
      draw_direction_arrow(ctx, pac_x, pac_y - 12, s_wanted);
    }
  }
  if (s_calibrating) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(12, 53, bounds.size.w - 24, 70), 4,
                       GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorCyan);
    graphics_draw_round_rect(ctx, GRect(12, 53, bounds.size.w - 24, 70), 4);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "HOLD LEVEL\nCALIBRATING...",
                       s_font_24_bold,
                       GRect(18, 60, bounds.size.w - 36, 58),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else if (s_game_over) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(15, 50, bounds.size.w - 30, 84), 5,
                       GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_draw_round_rect(ctx, GRect(15, 50, bounds.size.w - 30, 84), 5);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "GAME OVER\nSELECT TO RESTART",
                       s_font_24_bold,
                       GRect(20, 57, bounds.size.w - 40, 72),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else if (s_ready_ticks > 0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(28, 69, bounds.size.w - 56, 48), 5,
                       GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorYellow);
    graphics_draw_round_rect(ctx, GRect(28, 69, bounds.size.w - 56, 48), 5);
    graphics_context_set_text_color(ctx, GColorYellow);
    graphics_draw_text(ctx, "GET READY!!",
                       s_font_24_bold,
                       GRect(31, 76, bounds.size.w - 62, 34),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void timer_handler(void *context) {
  uint32_t now = clock_ms();
  uint32_t elapsed =
      now >= s_last_timer_ms ? now - s_last_timer_ms : RENDER_STEP_MS;
  s_last_timer_ms = now;
  if (elapsed > 1000) {
    elapsed = RENDER_STEP_MS;
  }
  elapsed = WW_MIN(elapsed, SIM_STEP_MS * 4);
  s_sim_accumulator_ms += elapsed;
  int simulation_steps = 0;
  while (s_sim_accumulator_ms >= SIM_STEP_MS && simulation_steps < 4) {
    snapshot_actor_positions();
    game_step();
    s_sim_accumulator_ms -= SIM_STEP_MS;
    ++simulation_steps;
  }
  if (s_light_notice_ticks > 0) {
    --s_light_notice_ticks;
  }
  if (!s_home) {
    layer_mark_dirty(s_game_layer);
    if (hud_state_changed()) {
      layer_mark_dirty(s_hud_layer);
    }
  }
  s_timer = app_timer_register(RENDER_STEP_MS, timer_handler, NULL);
}

static void accel_handler(AccelData *data, uint32_t count) {
  if (s_home || s_game_over) {
    return;
  }
  for (uint32_t i = 0; i < count; ++i) {
    if (data[i].did_vibrate) {
      continue;
    }
    if (s_calibrating) {
      s_base_x += data[i].x;
      s_base_y += data[i].y;
      if (++s_calibration_samples >= 20) {
        s_base_x /= s_calibration_samples;
        s_base_y /= s_calibration_samples;
        s_filter_x = s_base_x;
        s_filter_y = s_base_y;
        s_calibrating = false;
        s_paused = false;
        update_backlight();
      }
      continue;
    }

    s_filter_x = (s_filter_x * 3 + data[i].x) / 4;
    s_filter_y = (s_filter_y * 3 + data[i].y) / 4;
    int x = s_filter_x - s_base_x;
    int y = s_filter_y - s_base_y;
    Direction candidate = DIR_NONE;
    if (WW_ABS(x) > WW_ABS(y) && WW_ABS(x) > ACCEL_TRIGGER) {
      candidate = x > 0 ? DIR_RIGHT : DIR_LEFT;
    } else if (WW_ABS(y) > ACCEL_TRIGGER) {
      candidate = y > 0 ? DIR_UP : DIR_DOWN;
    } else if (WW_ABS(x) < ACCEL_RELEASE && WW_ABS(y) < ACCEL_RELEASE) {
      s_accel_candidate = DIR_NONE;
      s_candidate_count = 0;
    }

    if (candidate != DIR_NONE) {
      if (candidate == s_accel_candidate) {
        ++s_candidate_count;
      } else {
        s_accel_candidate = candidate;
        s_candidate_count = 1;
      }
      if (s_candidate_count >= ACCEL_DWELL) {
        request_direction(candidate);
        s_candidate_count = 0;
      }
    }
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_game_over) {
    return;
  }
  bool *enabled = s_home ? &s_home_backlight_enabled : &s_backlight_enabled;
  *enabled = !*enabled;
  if (!s_home) {
    s_light_notice_ticks = 20;
  }
  if (s_backlight_off_timer) {
    app_timer_cancel(s_backlight_off_timer);
    s_backlight_off_timer = NULL;
  }
  update_backlight();
  if (!*enabled) {
    s_backlight_off_timer =
        app_timer_register(100, backlight_off_handler, NULL);
  }
  if (!s_home) {
    layer_mark_dirty(s_hud_layer);
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_home || s_game_over) {
    return;
  }
  request_direction(DIR_DOWN);
}

static void start_game(void) {
  s_lives = 3;
  s_game_over = false;
  s_home = false;
  s_paused = false;
  s_hud_score = -1;
  s_hud_lives = -1;
  s_hud_light_state = -1;
  s_camera_x_fixed = 0;
  s_camera_y_fixed = 0;
  s_sim_accumulator_ms = 0;
  s_last_timer_ms = clock_ms();
  load_level();
  reset_calibration();
  update_backlight();
  layer_set_hidden(s_home_layer, true);
  layer_set_hidden(s_game_layer, false);
  layer_set_hidden(s_hud_layer, false);
  layer_mark_dirty(s_game_layer);
  layer_mark_dirty(s_hud_layer);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_home || s_game_over) {
    start_game();
    return;
  }
  s_paused = !s_paused;
  if (!s_paused) {
    reset_calibration();
  }
  update_backlight();
  layer_mark_dirty(s_game_layer);
  layer_mark_dirty(s_hud_layer);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_game_layer =
      layer_create(GRect(0, HUD_H, bounds.size.w, bounds.size.h - HUD_H));
  layer_set_update_proc(s_game_layer, game_layer_update);
  layer_add_child(root, s_game_layer);
  s_hud_layer = layer_create(GRect(0, 0, bounds.size.w, HUD_H));
  layer_set_update_proc(s_hud_layer, hud_layer_update);
  layer_add_child(root, s_hud_layer);
  s_home_layer = layer_create(bounds);
  layer_set_update_proc(s_home_layer, home_layer_update);
  layer_add_child(root, s_home_layer);
  layer_set_hidden(s_game_layer, true);
  layer_set_hidden(s_hud_layer, true);
}

static void window_unload(Window *window) {
  layer_destroy(s_home_layer);
  layer_destroy(s_hud_layer);
  layer_destroy(s_game_layer);
}

static void init(void) {
  load_level();
  s_home = true;
  s_home_backlight_enabled = false;
  s_game_over = false;
  s_paused = true;
  s_calibrating = false;
  s_font_18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_font_18_bold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  update_backlight();
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_window, true);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  accel_data_service_subscribe(1, accel_handler);
  s_last_timer_ms = clock_ms();
  s_timer = app_timer_register(RENDER_STEP_MS, timer_handler, NULL);
}

static void deinit(void) {
  light_enable(false);
  if (s_backlight_off_timer) {
    app_timer_cancel(s_backlight_off_timer);
  }
  if (s_timer) {
    app_timer_cancel(s_timer);
  }
  accel_data_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
