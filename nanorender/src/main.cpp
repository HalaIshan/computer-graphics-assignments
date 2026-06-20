#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 900

static uint32_t g_buffer[WIDTH * HEIGHT];
static bool g_checkerboard_mode = false;
static bool g_invert_colors = false;

// Part 4 temporary click marker state
static int g_last_click_x = -1;
static int g_last_click_y = -1;
static int g_click_marker_frames = 0;

// Part 5: application state controlled by UI widgets
static float part5_tile_size = 80.0f;
static float part5_color_intensity = 1.0f;
static int part5_animate_background = 0;
static int frame_counter = 0;

struct DrawnLine {
  int x0;
  int y0;
  int x1;
  int y1;
  uint32_t color;
};

static std::vector<DrawnLine> g_drawn_lines;

static bool g_is_drawing_line = false;
static int g_line_start_x = 0;
static int g_line_start_y = 0;
static int g_line_current_x = 0;
static int g_line_current_y = 0;
static bool g_previous_left_down = false;

// Part 6.3: current drawing color controlled by UI
static float g_line_color_r = 255.0f;
static float g_line_color_g = 255.0f;
static float g_line_color_b = 255.0f;

static uint32_t get_current_line_color() {
  return MFB_RGB((uint8_t)g_line_color_r,
                 (uint8_t)g_line_color_g,
                 (uint8_t)g_line_color_b);
}

static bool is_inside_drawing_area(int x, int y) {
  return x > 420 && x < WIDTH && y > 0 && y < HEIGHT;
}

static void update_line_drawing(mu_Context *ctx) {
  bool left_down = (ctx->mouse_down & MU_MOUSE_LEFT) != 0;

  static const int LINE_DRAWING_OFFSET_Y = 0;

  int mouse_x = ctx->mouse_pos.x;
  int mouse_y = ctx->mouse_pos.y + LINE_DRAWING_OFFSET_Y; // Adjust for UI input offset

  bool left_pressed = left_down && !g_previous_left_down;
  bool left_released = !left_down && g_previous_left_down;

  if (left_pressed && is_inside_drawing_area(mouse_x, mouse_y)) {
    g_is_drawing_line = true;
    g_line_start_x = mouse_x;
    g_line_start_y = mouse_y;
    g_line_current_x = mouse_x;
    g_line_current_y = mouse_y;
  }

  if (g_is_drawing_line && left_down) {
    g_line_current_x = mouse_x;
    g_line_current_y = mouse_y;
  }

  if (g_is_drawing_line && left_released) {
    DrawnLine line;
    line.x0 = g_line_start_x;
    line.y0 = g_line_start_y;
    line.x1 = mouse_x;
    line.y1 = mouse_y;
    line.color = get_current_line_color();

    g_drawn_lines.push_back(line);
    g_is_drawing_line = false;
  }

  g_previous_left_down = left_down;
}

static void colored_label(mu_Context *ctx, const char *text, mu_Color color) {
  mu_Color old_text_color = ctx->style->colors[MU_COLOR_TEXT];

  ctx->style->colors[MU_COLOR_TEXT] = color;
  mu_label(ctx, text);

  ctx->style->colors[MU_COLOR_TEXT] = old_text_color;
}

// Temporary debug marker for Part 4 recording.
// This draws a visible red circle at the last mouse click position.
static void draw_click_marker() {
  if (g_click_marker_frames <= 0 || g_last_click_x < 0 || g_last_click_y < 0) {
    return;
  }

  int radius = 12;

  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        int px = g_last_click_x + dx;
        int py = g_last_click_y + dy;

        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
          g_buffer[py * WIDTH + px] = MFB_RGB(255, 0, 0);
        }
      }
    }
  }

  g_click_marker_frames--;
}
static void draw_pixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  g_buffer[y * WIDTH + x] = color;
}

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;

  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;

  int error = dx + dy;

  while (true) {
    draw_pixel(x0, y0, color);

    if (x0 == x1 && y0 == y1) {
      break;
    }

    int e2 = 2 * error;

    if (e2 >= dy) {
      error += dy;
      x0 += sx;
    }

    if (e2 <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

int main() {
  struct mfb_window *window =
      mfb_open_ex("MiniGUI Platform", WIDTH, HEIGHT, MFB_WF_RESIZABLE);
  if (!window)
    return 1;

  mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
  mu_init(ctx);

  // Make UI rows taller so text is not clipped
  ctx->style->size.y = 22;
  ctx->style->padding = 4;
  ctx->style->spacing = 4;

  // Set font callbacks for microui
  ctx->text_width = [](mu_Font font, const char *str, int len) {
    return (len < 0 ? (int)strlen(str) : len) * 8;
  };
  ctx->text_height = [](mu_Font font) { return 8; };

  UIRenderer renderer(WIDTH, HEIGHT);

  // Part 0: small GLM test
  glm::vec3 glm_test_point(1.0f, 2.0f, 3.0f);
  glm::mat4 glm_test_matrix(1.0f);

  glm_test_matrix = glm::translate(glm_test_matrix, glm::vec3(10.0f, 0.0f, 0.0f));

  glm::vec4 glm_test_result =
      glm_test_matrix * glm::vec4(glm_test_point, 1.0f);

  // Set up char input callback for textbox input
  mfb_set_char_input_callback(
      [](struct mfb_window *w, unsigned int c) {
        extern void ui_bridge_char_input(struct mfb_window *, unsigned int);

        if (c == 'p' || c == 'P') {
          g_checkerboard_mode = !g_checkerboard_mode;
          return; // consume the shortcut key
        }

        if (c == 'i' || c == 'I') {
          g_invert_colors = !g_invert_colors;
          return; // consume the shortcut key
        }

        ui_bridge_char_input(w, c);
      },
      window);

  while (mfb_update_events(window) != MFB_STATE_EXIT) {
    // 1. Input
    ui_bridge_input(ctx, window);

    frame_counter++;

    // Temporary input calibration for Part 4 testing.
    // The visible UI is slightly above the logical click area,
    // so we shift the mouse position down before MicroUI uses it.
    int raw_mouse_y = ctx->mouse_pos.y;

    static const int UI_INPUT_OFFSET_Y = 0;
    ctx->mouse_pos.y = raw_mouse_y + UI_INPUT_OFFSET_Y;

    update_line_drawing(ctx);

    // Part 4 temporary click marker input
    if (ctx->mouse_down & MU_MOUSE_LEFT) {
      g_last_click_x = ctx->mouse_pos.x;
      g_last_click_y = ctx->mouse_pos.y;
      g_click_marker_frames = 30;
    }

    // 2. Scene Rendering (Background)
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
      int x = i % WIDTH;
      int y = i / WIDTH;

      int center_x = WIDTH / 2;
      int center_y = HEIGHT / 2;
      int dx = x - center_x;
      int dy = y - center_y;

      int distance_squared = dx * dx + dy * dy;

      uint8_t r;
      uint8_t g;
      uint8_t b;

      if (g_checkerboard_mode) {
        int tile_size = (int)part5_tile_size;
        if (tile_size < 10) tile_size = 10;

        int animated_shift = part5_animate_background ? frame_counter / 4 : 0;
        bool checker = (((x + animated_shift) / tile_size) + ((y + animated_shift) / tile_size)) % 2 == 0;

        r = checker ? 230 : 40;
        g = checker ? 230 : 80;
        b = checker ? 230 : 160;
      } else {
        int animation_shift = part5_animate_background ? frame_counter * 20 : 0;

        r = (uint8_t)(((distance_squared / 900) + animation_shift) % 256);
        g = (uint8_t)(((distance_squared / 500) + animation_shift) % 256);
        b = (uint8_t)(((distance_squared / 300) + animation_shift) % 256);

        r = (uint8_t)std::min(255.0f, (float)r * part5_color_intensity);
        g = (uint8_t)std::min(255.0f, (float)g * part5_color_intensity);
        b = (uint8_t)std::min(255.0f, (float)b * part5_color_intensity);
      }

      if (g_invert_colors) {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;
      }
      g_buffer[i] = MFB_RGB(r, g, b);
    }

    // 3. UI Logic
    static bool quit_requested = false;
    static int show_part2_label = 0;

    mu_begin(ctx);

    // --- Widgets window ---
    if (mu_begin_window(ctx, "Widgets", mu_rect(20, 20, 360, 800))){
      int w1[] = {-1};

      // Part 3: keyboard controls explanation
      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "Part 3 keyboard controls:", mu_color(100, 200, 255, 255));

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "P: switch circles/checkerboard", mu_color(100, 200, 255, 255));

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "I: invert background colors", mu_color(100, 200, 255, 255));

      // Part 5: UI controls bound to application state
      mu_layout_row(ctx, 1, w1, 8);
      mu_label(ctx, "");

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "Part 5 background controls:", mu_color(180, 255, 180, 255));

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Checker tile size:");
      mu_slider(ctx, &part5_tile_size, 20, 160);

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Circle color intensity:");
      mu_slider(ctx, &part5_color_intensity, 0.2f, 2.0f);

      mu_layout_row(ctx, 1, w1, 0);
      mu_checkbox(ctx, "Animate background", &part5_animate_background);

      mu_layout_row(ctx, 1, w1, 8);
      mu_label(ctx, "");

      // Part 2: custom interactive widget
      mu_layout_row(ctx, 1, w1, 8);
      mu_label(ctx, "");

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "Part 2 checkbox demo:", mu_color(255, 220, 80, 255));

      mu_layout_row(ctx, 1, w1, 0);
      mu_checkbox(ctx, "Show Part 2 label", &show_part2_label);

      if (show_part2_label) {
        mu_layout_row(ctx, 1, w1, 0);
        colored_label(ctx, "Part 2 label is now visible", mu_color(255, 220, 80, 255));

        mu_layout_row(ctx, 1, w1, 0);
        colored_label(ctx, "Mission accomplished!", mu_color(255, 220, 80, 255));
      }

      // Assignment 2 - Part 0: GLM test display
      mu_layout_row(ctx, 1, w1, 18);
      mu_label(ctx, "");

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "Assignment 2 - Part 0:", mu_color(200, 180, 255, 255));

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, "GLM transformation test", mu_color(200, 180, 255, 255));

      mu_layout_row(ctx, 1, w1, 8);
      mu_label(ctx, "");

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Original point: 1.0, 2.0, 3.0");

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Translation: +10.0 on X axis");

      char glm_result_text[128];
      snprintf(glm_result_text, sizeof(glm_result_text),
               "Result point: %.1f, %.1f, %.1f",
               glm_test_result.x,
               glm_test_result.y,
               glm_test_result.z);

      mu_layout_row(ctx, 1, w1, 0);
      colored_label(ctx, glm_result_text, mu_color(180, 255, 180, 255));

      mu_layout_row(ctx, 1, w1, 12);
      mu_label(ctx, "");

      // quit button
      mu_layout_row(ctx, 1, w1, 0);
      if (mu_button(ctx, "Quit")) {
        quit_requested = true;
      }

      mu_end_window(ctx);
    }

    // --- Creative Canvas window ---
    if (mu_begin_window(ctx, "Creative Canvas", mu_rect(390, 20, 360, 450))) {
      int wc[] = {-1};

      mu_layout_row(ctx, 1, wc, 0);
      colored_label(ctx, "Part 6 line drawing:", mu_color(255, 180, 120, 255));

      mu_layout_row(ctx, 1, wc, 0);
      colored_label(ctx, "Drag in the empty area to draw a line", mu_color(255, 180, 120, 255));

      mu_layout_row(ctx, 1, wc, 8);
      mu_label(ctx, "");


      mu_layout_row(ctx, 1, wc, 0);
      colored_label(ctx, "Line color controls:", mu_color(255, 180, 120, 255));

      mu_layout_row(ctx, 1, wc, 0);
      mu_label(ctx, "Line red:");
      mu_slider(ctx, &g_line_color_r, 0, 255);

      mu_layout_row(ctx, 1, wc, 0);
      mu_label(ctx, "Line green:");
      mu_slider(ctx, &g_line_color_g, 0, 255);

      mu_layout_row(ctx, 1, wc, 0);
      mu_label(ctx, "Line blue:");
      mu_slider(ctx, &g_line_color_b, 0, 255);

      mu_layout_row(ctx, 1, wc, 0);
      colored_label(ctx, "Current line color preview",
                    mu_color((int)g_line_color_r,
                             (int)g_line_color_g,
                             (int)g_line_color_b,
                             255));

      mu_layout_row(ctx, 1, wc, 0);
      if (mu_button(ctx, "Clear canvas")) {
        g_drawn_lines.clear();
        g_is_drawing_line = false;
      }

      mu_end_window(ctx);
    }

    // --- Popup demo window ---
    if (mu_begin_window(ctx, "Popup Demo", mu_rect(760, 70, 380, 120))) {
      int w3[] = {-1};
      mu_layout_row(ctx, 1, w3, 0);
      if (mu_button(ctx, "Open popup")) {
        mu_Container *popup = mu_get_container(ctx, "my popup");
        popup->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 260, 84);
        popup->open = 1;
        ctx->hover_root = ctx->next_hover_root = popup;
        mu_bring_to_front(ctx, popup);
      }
      int popup_opt = MU_OPT_POPUP | MU_OPT_NORESIZE | MU_OPT_NOSCROLL |
                      MU_OPT_NOTITLE | MU_OPT_CLOSED;
      if (mu_begin_window_ex(ctx, "my popup", mu_rect(0, 0, 260, 84),
                             popup_opt)) {
        int wp[] = {-1};
        mu_layout_row(ctx, 1, wp, 0);
        mu_label(ctx, "mu_popup: click outside to close");
        if (mu_button(ctx, "Close")) {
          mu_get_current_container(ctx)->open = 0;
        }
        mu_end_window(ctx);
      }
      mu_end_window(ctx);
    }

    mu_end(ctx);

    if (quit_requested) {
      mfb_close(window);
      break;
    }

    // Part 6: draw saved lines
    for (const DrawnLine &line : g_drawn_lines) {
      draw_line(line.x0, line.y0, line.x1, line.y1, line.color);
    }

    // Part 6: draw temporary preview line while dragging
    if (g_is_drawing_line) {
      draw_line(g_line_start_x, g_line_start_y,
                g_line_current_x, g_line_current_y,
                get_current_line_color());
    }

    // 4. UI Rendering
    renderer.render(ctx, g_buffer);

    // 5. Display
    mfb_update_state state = mfb_update_ex(window, g_buffer, WIDTH, HEIGHT);
    if (state < 0)
      break;

    // Cap FPS (optional, minifb has built-in sync)
    mfb_wait_sync(window);
  }

  mfb_close(window);
  free(ctx);
  return 0;
}
// cd "C:\courses\ComputerGraphics\cg-at-uoh-26-HalaIshan\nanorender"
// powershell -ExecutionPolicy Bypass -File .\build_and_run.ps1