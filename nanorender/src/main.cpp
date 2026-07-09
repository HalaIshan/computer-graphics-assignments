#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600  // Logical framebuffer width
#define HEIGHT 900  // Logical framebuffer height

struct DrawnLine {
  int x0;
  int y0;
  int x1;
  int y1;
  uint32_t color;
};

// Mesh data structures
struct Face {
  int v0;
  int v1;
  int v2;
};

struct Mesh {
  std::vector<glm::vec3> vertices;
  std::vector<Face> faces;
};

struct BoundingBox {
  glm::vec3 min;
  glm::vec3 max;
};

struct ViewportTransform {
  BoundingBox box;
  glm::vec3 center;
  glm::vec3 translation;
  float scale;
  bool valid;
};

struct MeshOption {
  const char *name;
  const char *paths[3];
};

struct TransformParameters {
  glm::vec3 translation;
  glm::vec3 rotation_degrees;
  glm::vec3 scale;
};

struct ScreenPoint {
  int x;
  int y;
};

enum BackgroundMode {
  BACKGROUND_PLAIN,
  BACKGROUND_CIRCLES,
  BACKGROUND_CHECKERBOARD
};

enum TransformSpace {
  TRANSFORM_LOCAL,
  TRANSFORM_WORLD
};

enum TransformOperation {
  TRANSFORM_TRANSLATION,
  TRANSFORM_ROTATION,
  TRANSFORM_SCALE
};

static const char *WIDGETS_WINDOW_TITLE = "Scene Controls";            // Left scene panel
static const char *CANVAS_WINDOW_TITLE = "Creative Canvas";           // Drawing tools panel
static const char *WINDOW_CONTROLS_TITLE = "Window Controls";         // Reset/show buttons panel

static const MeshOption MESH_OPTIONS[] = {
    {"simple_pyramid.obj",
     {"assets/hw2/models/simple_pyramid.obj",
      "../assets/hw2/models/simple_pyramid.obj",
      "../../assets/hw2/models/simple_pyramid.obj"}},
    {"regular_cube.obj",
     {"assets/hw2/models/regular_cube.obj",
      "../assets/hw2/models/regular_cube.obj",
      "../../assets/hw2/models/regular_cube.obj"}},
    {"angled_box.obj",
     {"assets/hw2/models/angled_box.obj",
      "../assets/hw2/models/angled_box.obj",
      "../../assets/hw2/models/angled_box.obj"}}};

static const int MESH_OPTION_COUNT = sizeof(MESH_OPTIONS) / sizeof(MESH_OPTIONS[0]);
static const int DEFAULT_MESH_INDEX = 1; // Start with the regular cube comparison mesh

static const float TRANSLATION_MIN = -2.0f;
static const float TRANSLATION_MAX = 2.0f;
static const float ROTATION_MIN = -180.0f;
static const float ROTATION_MAX = 180.0f;
static const float SCALE_MIN = 0.1f;
static const float SCALE_MAX = 2.0f;

static const mu_Rect DEFAULT_WIDGETS_RECT = {20, 20, 460, 800};             // Left scene controls
static const mu_Rect DEFAULT_WINDOW_CONTROLS_RECT = {1220, 20, 360, 160};   // Upper right
static const mu_Rect DEFAULT_CANVAS_RECT = {1220, 200, 360, 450};           // Below window controls

static const mu_Color KEYBOARD_COLOR = {100, 200, 255, 255};         // Light blue
static const mu_Color BACKGROUND_COLOR = {180, 255, 180, 255};       // Light green
static const mu_Color MESH_COLOR = {200, 180, 255, 255};             // Light purple
static const mu_Color TRANSFORM_COLOR = {255, 220, 120, 255};        // Warm yellow
static const mu_Color LINE_TOOLS_COLOR = {255, 180, 120, 255};       // Soft orange
static const mu_Color WINDOW_CONTROLS_COLOR = {220, 220, 255, 255};  // Pale blue

// Framebuffer state
static uint32_t g_buffer[WIDTH * HEIGHT];

// Background state controlled by UI widgets
static BackgroundMode g_background_mode = BACKGROUND_PLAIN;
static bool g_invert_colors = true;                // Start with darker plain background
static float g_background_tile_size = 80.0f;        // Checkerboard square size
static float g_background_color_intensity = 1.0f;   // Circle pattern brightness multiplier
static int g_background_animate = 0;                // MicroUI checkbox uses int
static int g_frame_counter = 0;                     // Drives simple background animation

// Mesh loading state
static Mesh g_loaded_mesh;
static bool g_mesh_loaded = false;
static char g_mesh_status[256] = "Mesh not loaded yet"; // Last load result shown in UI
static char g_mesh_path_used[256] = "";                 // Path that successfully loaded
static ViewportTransform g_viewport_transform = {};
static int g_selected_mesh_index = DEFAULT_MESH_INDEX;
static bool g_mesh_selector_open = false;

// Part 4 transformation state
static TransformParameters g_local_transform = {
    glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
static TransformParameters g_world_transform = {
    glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
static TransformSpace g_transform_space = TRANSFORM_LOCAL;
static TransformOperation g_transform_operation = TRANSFORM_TRANSLATION;
static float g_model_size = 1.0f; // Uniform size multiplier for the whole model

// Line drawing state
static std::vector<DrawnLine> g_drawn_lines;
static bool g_is_drawing_line = false;
static int g_line_start_x = 0;
static int g_line_start_y = 0;
static int g_line_current_x = 0;
static int g_line_current_y = 0;
static bool g_previous_left_down = false;

// Current line drawing color controlled by UI
static float g_line_color_r = 255.0f; // Slider value, 0-255
static float g_line_color_g = 255.0f; // Slider value, 0-255
static float g_line_color_b = 255.0f; // Slider value, 0-255

static mu_Color ui_current_line_color() {
  return mu_color((int)g_line_color_r, (int)g_line_color_g, (int)g_line_color_b, 255);
}

// Cached UI window positions.
// These values start with the default window positions,
// then get updated every frame after MicroUI builds the UI.
static mu_Rect g_widgets_window_rect = DEFAULT_WIDGETS_RECT;
static mu_Rect g_canvas_window_rect = DEFAULT_CANVAS_RECT;
static mu_Rect g_window_controls_rect = DEFAULT_WINDOW_CONTROLS_RECT;

static bool g_widgets_window_open = true;
static bool g_canvas_window_open = true;
static bool g_window_controls_open = true;

// General helpers
static uint32_t get_current_line_color() {
  return MFB_RGB((uint8_t)g_line_color_r,
                 (uint8_t)g_line_color_g,
                 (uint8_t)g_line_color_b);
}

static int clamp_int(int value, int min_value, int max_value) {
  if (max_value < min_value) {
    return min_value;
  }

  if (value < min_value) {
    return min_value;
  }

  if (value > max_value) {
    return max_value;
  }

  return value;
}

static uint8_t apply_color_intensity(uint8_t value, float intensity) {
  return (uint8_t)std::min(255.0f, (float)value * intensity);
}

static uint8_t invert_color_channel(uint8_t value) {
  return 255 - value;
}

static void cycle_background_mode() {
  if (g_background_mode == BACKGROUND_PLAIN) {
    g_background_mode = BACKGROUND_CIRCLES;
  } else if (g_background_mode == BACKGROUND_CIRCLES) {
    g_background_mode = BACKGROUND_CHECKERBOARD;
  } else {
    g_background_mode = BACKGROUND_PLAIN;
  }
}

// Window and input helpers
static void clamp_rect_to_logical_screen(mu_Rect *rect) {
  int max_x = WIDTH - rect->w;
  int max_y = HEIGHT - rect->h;

  rect->x = clamp_int(rect->x, 0, max_x);
  rect->y = clamp_int(rect->y, 0, max_y);
}

static bool is_inside_drawing_area(int x, int y) {
  return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

static bool point_inside_rect(int x, int y, mu_Rect rect) {
  return x >= rect.x &&
         x < rect.x + rect.w &&
         y >= rect.y &&
         y < rect.y + rect.h;
}

static bool is_mouse_over_ui_window(int x, int y) {
  if (g_widgets_window_open &&
      point_inside_rect(x, y, g_widgets_window_rect)) {
    return true;
  }


  if (g_canvas_window_open &&
      point_inside_rect(x, y, g_canvas_window_rect)) {
    return true;
  }

  if (g_window_controls_open &&
      point_inside_rect(x, y, g_window_controls_rect)) {
    return true;
  }

  return false;
}

static void reset_ui_window(mu_Context *ctx, const char *title, mu_Rect rect) {
  mu_Container *container = mu_get_container(ctx, title);
  container->rect = rect;
  container->open = 1;
  mu_bring_to_front(ctx, container);
}

static void cache_ui_window_state(mu_Context *ctx, const char *title,
                                  mu_Rect *rect, bool *open, bool keep_on_screen) {
  mu_Container *container = mu_get_container(ctx, title);

  if (keep_on_screen) { // Only fixed panels need clamping
    clamp_rect_to_logical_screen(&container->rect);
  }

  *rect = container->rect;
  *open = container->open;
}

// Line drawing input
static void update_line_drawing(mu_Context *ctx) {
  bool left_down = (ctx->mouse_down & MU_MOUSE_LEFT) != 0;

  int mouse_x = ctx->mouse_pos.x;
  int mouse_y = ctx->mouse_pos.y;

  bool left_pressed = left_down && !g_previous_left_down;
  bool left_released = !left_down && g_previous_left_down;

  bool mouse_over_ui = is_mouse_over_ui_window(mouse_x, mouse_y);

  // Start drawing only from empty framebuffer space, not from UI panels.
  if (left_pressed && is_inside_drawing_area(mouse_x, mouse_y) && !mouse_over_ui) {
    g_is_drawing_line = true;
    g_line_start_x = mouse_x;
    g_line_start_y = mouse_y;
    g_line_current_x = mouse_x;
    g_line_current_y = mouse_y;
  }

  // While the mouse is held, update the preview line endpoint.
  if (g_is_drawing_line && left_down) {
    g_line_current_x = mouse_x;
    g_line_current_y = mouse_y;
  }

  // When released, store the finished line so it is redrawn every frame.
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

// UI helpers
static void colored_label(mu_Context *ctx, const char *text, mu_Color color) {
  mu_Color old_text_color = ctx->style->colors[MU_COLOR_TEXT];

  ctx->style->colors[MU_COLOR_TEXT] = color; // Temporarily override MicroUI text color
  mu_label(ctx, text);

  ctx->style->colors[MU_COLOR_TEXT] = old_text_color;
}

static void label_row(mu_Context *ctx, int *widths, const char *text) {
  mu_layout_row(ctx, 1, widths, 0);
  mu_label(ctx, text);
}

static void colored_label_row(mu_Context *ctx, int *widths, const char *text, mu_Color color) {
  mu_layout_row(ctx, 1, widths, 0);
  colored_label(ctx, text, color);
}

static void spacer_row(mu_Context *ctx, int *widths, int height) {
  mu_layout_row(ctx, 1, widths, height);
  mu_label(ctx, ""); // Empty label reserves vertical space
}

static bool button_row(mu_Context *ctx, int *widths, const char *text) {
  mu_layout_row(ctx, 1, widths, 0);
  return mu_button(ctx, text);
}

static void checkbox_row(mu_Context *ctx, int *widths, const char *text, int *value) {
  mu_layout_row(ctx, 1, widths, 0);
  mu_checkbox(ctx, text, value);
}

static void slider_row(mu_Context *ctx, int *widths, const char *label,
                       float *value, float low, float high) {
  label_row(ctx, widths, label);
  mu_slider(ctx, value, low, high);
}

static void axis_slider_row(mu_Context *ctx, const char *axis,
                            float *value, float low, float high,
                            float default_value, const char *reset_label) {
  int widths[] = {24, 300, 100};
  mu_layout_row(ctx, 3, widths, 0);
  mu_label(ctx, axis);
  mu_slider(ctx, value, low, high);
  if (mu_button(ctx, reset_label)) {
    *value = default_value;
  }
}

static void uniform_scale_row(mu_Context *ctx) {
  int widths[] = {112, 212, 100};
  mu_layout_row(ctx, 3, widths, 0);
  mu_label(ctx, "Uniform scale:");
  mu_slider(ctx, &g_model_size, SCALE_MIN, SCALE_MAX);
  if (mu_button(ctx, "Reset Size")) {
    g_model_size = 1.0f;
  }
}

static bool load_mesh_option(int option_index);

static void draw_keyboard_controls(mu_Context *ctx, int *widths) {
  mu_Color keyboard_color = KEYBOARD_COLOR;
  colored_label_row(ctx, widths, "Keyboard controls:", keyboard_color);
  colored_label_row(ctx, widths, "P: change background", keyboard_color);
  colored_label_row(ctx, widths, "Cycle: Plain > Circles > Checker", keyboard_color);
  colored_label_row(ctx, widths, "I: invert background colors", keyboard_color);
}

static void draw_background_controls(mu_Context *ctx, int *widths) {
  mu_Color background_color = BACKGROUND_COLOR;

  if (g_background_mode == BACKGROUND_CHECKERBOARD) {
    colored_label_row(ctx, widths, "Background controls:", background_color);
    slider_row(ctx, widths, "Checker tile size:", &g_background_tile_size, 20, 160);
    checkbox_row(ctx, widths, "Animate background", &g_background_animate);
  } else if (g_background_mode == BACKGROUND_CIRCLES) {
    colored_label_row(ctx, widths, "Background controls:", background_color);
    slider_row(ctx, widths, "Circle color intensity:", &g_background_color_intensity, 0.2f, 2.0f);
    checkbox_row(ctx, widths, "Animate background", &g_background_animate);
  }
}

static bool calculate_bounding_box(const Mesh &mesh, BoundingBox &box) {
  if (mesh.vertices.empty()) {
    return false;
  }

  box.min = mesh.vertices[0];
  box.max = mesh.vertices[0];

  for (const glm::vec3 &vertex : mesh.vertices) {
    box.min = glm::min(box.min, vertex);
    box.max = glm::max(box.max, vertex);
  }

  return true;
}

static ViewportTransform calculate_viewport_transform(const Mesh &mesh) {
  ViewportTransform transform = {};
  transform.valid = calculate_bounding_box(mesh, transform.box);

  if (!transform.valid) {
    return transform;
  }

  glm::vec3 mesh_size = transform.box.max - transform.box.min;
  float max_extent = std::max(mesh_size.x, std::max(mesh_size.y, mesh_size.z));

  if (max_extent <= 0.0f) {
    transform.valid = false;
    return transform;
  }

  float target_size = 600.0f; // Fits comfortably inside the 1600x900 framebuffer
  transform.center = (transform.box.min + transform.box.max) * 0.5f;
  transform.scale = target_size / max_extent;
  transform.translation = glm::vec3(WIDTH / 2.0f, HEIGHT / 2.0f, 0.0f) -
                          transform.center * transform.scale;

  return transform;
}

static glm::vec3 apply_viewport_transform(const glm::vec3 &vertex,
                                          const ViewportTransform &transform) {
  return vertex * transform.scale + transform.translation;
}

static ScreenPoint project_vertex_orthographic(const glm::vec3 &vertex,
                                               const ViewportTransform &transform) {
  glm::vec3 mapped = apply_viewport_transform(vertex, transform);
  return {
      (int)std::lround(mapped.x),
      (int)std::lround((HEIGHT - 1) - mapped.y)}; // Flip y only for screen drawing
}

static glm::mat4 build_rotation_matrix(const glm::vec3 &rotation_degrees) {
  glm::mat4 rotation(1.0f);
  rotation = glm::rotate(rotation, glm::radians(rotation_degrees.x),
                         glm::vec3(1.0f, 0.0f, 0.0f));
  rotation = glm::rotate(rotation, glm::radians(rotation_degrees.y),
                         glm::vec3(0.0f, 1.0f, 0.0f));
  rotation = glm::rotate(rotation, glm::radians(rotation_degrees.z),
                         glm::vec3(0.0f, 0.0f, 1.0f));
  return rotation;
}

static glm::mat4 build_trs_matrix(const TransformParameters &transform,
                                  const glm::vec3 &scale_multiplier) {
  glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.translation);
  glm::mat4 rotation = build_rotation_matrix(transform.rotation_degrees);
  glm::mat4 scale = glm::scale(glm::mat4(1.0f),
                               transform.scale * scale_multiplier);

  return translation * rotation * scale;
}

static glm::mat4 build_local_transform_matrix(const TransformParameters &transform,
                                              const glm::vec3 &mesh_center) {
  glm::mat4 move_to_center = glm::translate(glm::mat4(1.0f), mesh_center);
  glm::mat4 move_from_center = glm::translate(glm::mat4(1.0f), -mesh_center);
  glm::mat4 local_trs = build_trs_matrix(transform, glm::vec3(g_model_size));

  return move_to_center * local_trs * move_from_center;
}

static glm::mat4 build_world_transform_matrix(const TransformParameters &transform) {
  return build_trs_matrix(transform, glm::vec3(1.0f));
}

static glm::mat4 build_model_matrix() {
  glm::mat4 local_matrix =
      build_local_transform_matrix(g_local_transform, g_viewport_transform.center);
  glm::mat4 world_matrix = build_world_transform_matrix(g_world_transform);

  return world_matrix * local_matrix;
}

static glm::vec3 apply_model_transform(const glm::vec3 &vertex,
                                       const glm::mat4 &model_matrix) {
  glm::vec4 transformed = model_matrix * glm::vec4(vertex, 1.0f);
  return glm::vec3(transformed);
}

static void reset_transform(TransformParameters &transform) {
  transform.translation = glm::vec3(0.0f);
  transform.rotation_degrees = glm::vec3(0.0f);
  transform.scale = glm::vec3(1.0f);
}

static void reset_all_transforms() {
  reset_transform(g_local_transform);
  reset_transform(g_world_transform);
  g_model_size = 1.0f;
}

static TransformParameters &selected_transform() {
  return g_transform_space == TRANSFORM_LOCAL
             ? g_local_transform
             : g_world_transform;
}

static glm::vec3 &selected_transform_values() {
  TransformParameters &transform = selected_transform();

  if (g_transform_operation == TRANSFORM_ROTATION) {
    return transform.rotation_degrees;
  }

  if (g_transform_operation == TRANSFORM_SCALE) {
    return transform.scale;
  }

  return transform.translation;
}

static void draw_mesh_controls(mu_Context *ctx, int *widths) {
  mu_Color mesh_color = MESH_COLOR;
  colored_label_row(ctx, widths, "Mesh:", mesh_color);

  char selector_text[128];
  snprintf(selector_text, sizeof(selector_text),
           "Model: %s", MESH_OPTIONS[g_selected_mesh_index].name);

  if (button_row(ctx, widths, selector_text)) {
    g_mesh_selector_open = !g_mesh_selector_open;
  }

  if (g_mesh_selector_open) {
    for (int i = 0; i < MESH_OPTION_COUNT; i++) {
      char option_text[128];
      snprintf(option_text, sizeof(option_text),
               "%s%s",
               i == g_selected_mesh_index ? "> " : "  ",
               MESH_OPTIONS[i].name);

      if (button_row(ctx, widths, option_text)) {
        load_mesh_option(i);
        g_mesh_selector_open = false;
      }
    }
  }

  char mesh_summary_text[128];
  snprintf(mesh_summary_text, sizeof(mesh_summary_text),
           "Loaded: %s | V: %zu | F: %zu",
           g_mesh_loaded ? "yes" : "no",
           g_loaded_mesh.vertices.size(),
           g_loaded_mesh.faces.size());
  label_row(ctx, widths, mesh_summary_text);

  if (!g_viewport_transform.valid) {
    label_row(ctx, widths, "Viewport: not available");
    return;
  }

  const BoundingBox &box = g_viewport_transform.box;

  char bounds_text[160];
  snprintf(bounds_text, sizeof(bounds_text),
           "Bounds: %.1f %.1f %.1f -> %.1f %.1f %.1f",
           box.min.x, box.min.y, box.min.z,
           box.max.x, box.max.y, box.max.z);
  label_row(ctx, widths, bounds_text);

  char center_text[128];
  snprintf(center_text, sizeof(center_text),
           "Center: %.1f, %.1f, %.1f",
           g_viewport_transform.center.x,
           g_viewport_transform.center.y,
           g_viewport_transform.center.z);
  label_row(ctx, widths, center_text);

  char scale_text[128];
  snprintf(scale_text, sizeof(scale_text),
           "Scale: %.1f", g_viewport_transform.scale);
  label_row(ctx, widths, scale_text);

  char translation_text[128];
  snprintf(translation_text, sizeof(translation_text),
           "Translate: %.1f, %.1f, %.1f",
           g_viewport_transform.translation.x,
           g_viewport_transform.translation.y,
           g_viewport_transform.translation.z);
  label_row(ctx, widths, translation_text);
}

static void draw_transform_controls(mu_Context *ctx, int *widths) {
  colored_label_row(ctx, widths, "Transform controls:", TRANSFORM_COLOR);

  int space_widths[] = {210, 210};
  mu_layout_row(ctx, 2, space_widths, 0);
  if (mu_button(ctx, g_transform_space == TRANSFORM_LOCAL ? "> Local" : "Local")) {
    g_transform_space = TRANSFORM_LOCAL;
  }
  if (mu_button(ctx, g_transform_space == TRANSFORM_WORLD ? "> World" : "World")) {
    g_transform_space = TRANSFORM_WORLD;
  }

  int operation_widths[] = {134, 134, 134};
  mu_layout_row(ctx, 3, operation_widths, 0);
  if (mu_button(ctx, g_transform_operation == TRANSFORM_TRANSLATION
                         ? "> Translation" : "Translation")) {
    g_transform_operation = TRANSFORM_TRANSLATION;
  }
  if (mu_button(ctx, g_transform_operation == TRANSFORM_ROTATION
                         ? "> Rotation" : "Rotation")) {
    g_transform_operation = TRANSFORM_ROTATION;
  }
  if (mu_button(ctx, g_transform_operation == TRANSFORM_SCALE
                         ? "> Scale" : "Scale")) {
    g_transform_operation = TRANSFORM_SCALE;
  }

  const char *space_name =
      g_transform_space == TRANSFORM_LOCAL ? "Local" : "World";
  const char *operation_name = "Translation";
  float low = TRANSLATION_MIN;
  float high = TRANSLATION_MAX;

  if (g_transform_operation == TRANSFORM_ROTATION) {
    operation_name = "Rotation";
    low = ROTATION_MIN;
    high = ROTATION_MAX;
  } else if (g_transform_operation == TRANSFORM_SCALE) {
    operation_name = "Scale";
    low = SCALE_MIN;
    high = SCALE_MAX;
  }

  char editing_text[96];
  snprintf(editing_text, sizeof(editing_text),
           "Editing: %s %s", space_name, operation_name);
  label_row(ctx, widths, editing_text);

  glm::vec3 &values = selected_transform_values();
  float default_value =
      g_transform_operation == TRANSFORM_SCALE ? 1.0f : 0.0f;

  axis_slider_row(ctx, "X:", &values.x, low, high, default_value, "Reset X");
  axis_slider_row(ctx, "Y:", &values.y, low, high, default_value, "Reset Y");
  axis_slider_row(ctx, "Z:", &values.z, low, high, default_value, "Reset Z");

  uniform_scale_row(ctx);

  if (button_row(ctx, widths, "Reset All")) {
    reset_all_transforms();
  }
}

static void draw_widgets_window(mu_Context *ctx, bool *quit_requested) {
  if (mu_begin_window(ctx, WIDGETS_WINDOW_TITLE, DEFAULT_WIDGETS_RECT)) {
    int widths[] = {-1};

    draw_keyboard_controls(ctx, widths);

    if (g_background_mode != BACKGROUND_PLAIN) {
      spacer_row(ctx, widths, 8);
      draw_background_controls(ctx, widths);
    }

    spacer_row(ctx, widths, 8);
    draw_mesh_controls(ctx, widths);

    spacer_row(ctx, widths, 8);
    draw_transform_controls(ctx, widths);

    if (button_row(ctx, widths, "Quit")) {
      *quit_requested = true;
    }

    mu_end_window(ctx);
  }
}


static void draw_creative_canvas_window(mu_Context *ctx) {
  if (mu_begin_window(ctx, CANVAS_WINDOW_TITLE, DEFAULT_CANVAS_RECT)) {
    int widths[] = {-1};
    mu_Color line_tools_color = LINE_TOOLS_COLOR;

    colored_label_row(ctx, widths, "Line drawing tools:", line_tools_color);
    colored_label_row(ctx, widths, "Drag in the empty area to draw a line", line_tools_color);

    spacer_row(ctx, widths, 8);

    colored_label_row(ctx, widths, "Line color controls:", line_tools_color);

    slider_row(ctx, widths, "Line red:", &g_line_color_r, 0, 255);
    slider_row(ctx, widths, "Line green:", &g_line_color_g, 0, 255);
    slider_row(ctx, widths, "Line blue:", &g_line_color_b, 0, 255);

    colored_label_row(ctx, widths, "Current line color preview", ui_current_line_color());

    if (button_row(ctx, widths, "Clear canvas")) {
      g_drawn_lines.clear();
      g_is_drawing_line = false;
    }

    mu_end_window(ctx);
  }
}

static void draw_window_controls_window(mu_Context *ctx,
                                        bool *reset_widgets_requested,
                                        bool *reset_canvas_requested) {
  int window_controls_options = MU_OPT_NOCLOSE | MU_OPT_NORESIZE;

  if (mu_begin_window_ex(ctx, WINDOW_CONTROLS_TITLE,
                         DEFAULT_WINDOW_CONTROLS_RECT, window_controls_options)) {
    int widths[] = {-1};

    colored_label_row(ctx, widths, "Window controls:", WINDOW_CONTROLS_COLOR);

    if (button_row(ctx, widths, "Show / Reset Scene Controls")) {
      *reset_widgets_requested = true;
    }

    if (button_row(ctx, widths, "Show / Reset Canvas")) {
      *reset_canvas_requested = true;
    }

    mu_end_window(ctx);
  }
}

static void handle_ui_reset_requests(mu_Context *ctx,
                                     bool *reset_widgets_requested,
                                     bool *reset_canvas_requested) {
  // Reset requests are set by buttons, then applied at the start of the next UI frame.
  if (*reset_widgets_requested) {
    reset_ui_window(ctx, WIDGETS_WINDOW_TITLE, DEFAULT_WIDGETS_RECT);
    *reset_widgets_requested = false;
  }


  if (*reset_canvas_requested) {
    reset_ui_window(ctx, CANVAS_WINDOW_TITLE, DEFAULT_CANVAS_RECT);
    *reset_canvas_requested = false;
  }
}

static void cache_all_ui_window_states(mu_Context *ctx) {
  // Cached rectangles let drawing input ignore clicks that begin on top of UI.
  cache_ui_window_state(ctx, WIDGETS_WINDOW_TITLE,
                        &g_widgets_window_rect, &g_widgets_window_open, false);

  cache_ui_window_state(ctx, CANVAS_WINDOW_TITLE,
                        &g_canvas_window_rect, &g_canvas_window_open, false);
  cache_ui_window_state(ctx, WINDOW_CONTROLS_TITLE,
                        &g_window_controls_rect, &g_window_controls_open, true);
}

// Drawing helpers
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

static void render_background() {
  int center_x = WIDTH / 2;
  int center_y = HEIGHT / 2;

  // Rebuild the procedural background from scratch each frame.
  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    int x = i % WIDTH;
    int y = i / WIDTH;

    int dx = x - center_x;
    int dy = y - center_y;

    int distance_squared = dx * dx + dy * dy;

    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (g_background_mode == BACKGROUND_PLAIN) {
      r = 210;
      g = 210;
      b = 210;
    } else if (g_background_mode == BACKGROUND_CHECKERBOARD) {
      int tile_size = (int)g_background_tile_size;
      if (tile_size < 10) tile_size = 10; // Avoid tiny checker cells

      int animated_shift = g_background_animate ? g_frame_counter / 4 : 0;
      bool checker = (((x + animated_shift) / tile_size) + ((y + animated_shift) / tile_size)) % 2 == 0;

      r = checker ? 230 : 40;
      g = checker ? 230 : 80;
      b = checker ? 230 : 160;
    } else if (g_background_mode == BACKGROUND_CIRCLES) {
      int animation_shift = g_background_animate ? g_frame_counter * 20 : 0;

      r = (uint8_t)(((distance_squared / 900) + animation_shift) % 256); // Wide red rings
      g = (uint8_t)(((distance_squared / 500) + animation_shift) % 256); // Medium green rings
      b = (uint8_t)(((distance_squared / 300) + animation_shift) % 256); // Tight blue rings

      r = apply_color_intensity(r, g_background_color_intensity);
      g = apply_color_intensity(g, g_background_color_intensity);
      b = apply_color_intensity(b, g_background_color_intensity);
    } else {
      r = 255;
      g = 255;
      b = 255;
    }

    if (g_invert_colors) {
      r = invert_color_channel(r);
      g = invert_color_channel(g);
      b = invert_color_channel(b);
    }

    g_buffer[i] = MFB_RGB(r, g, b);
  }
}

static void draw_line_overlay() {
  // Draw user-created lines after the background so they stay visible.
  for (const DrawnLine &line : g_drawn_lines) {
    draw_line(line.x0, line.y0, line.x1, line.y1, line.color);
  }

  // The temporary preview is not stored until the mouse button is released.
  if (g_is_drawing_line) {
    draw_line(g_line_start_x, g_line_start_y,
              g_line_current_x, g_line_current_y,
              get_current_line_color());
  }
}

static uint32_t get_wireframe_color() {
  return g_invert_colors ? MFB_RGB(245, 245, 245) : MFB_RGB(20, 20, 20);
}

static void draw_mesh_wireframe() {
  if (!g_mesh_loaded || !g_viewport_transform.valid) {
    return;
  }

  uint32_t wireframe_color = get_wireframe_color();
  glm::mat4 model_matrix = build_model_matrix();

  for (const Face &face : g_loaded_mesh.faces) {
    glm::vec3 v0 = apply_model_transform(g_loaded_mesh.vertices[face.v0],
                                         model_matrix);
    glm::vec3 v1 = apply_model_transform(g_loaded_mesh.vertices[face.v1],
                                         model_matrix);
    glm::vec3 v2 = apply_model_transform(g_loaded_mesh.vertices[face.v2],
                                         model_matrix);

    ScreenPoint p0 = project_vertex_orthographic(v0, g_viewport_transform);
    ScreenPoint p1 = project_vertex_orthographic(v1, g_viewport_transform);
    ScreenPoint p2 = project_vertex_orthographic(v2, g_viewport_transform);

    draw_line(p0.x, p0.y, p1.x, p1.y, wireframe_color);
    draw_line(p1.x, p1.y, p2.x, p2.y, wireframe_color);
    draw_line(p2.x, p2.y, p0.x, p0.y, wireframe_color);
  }
}

// OBJ loading helpers
static bool parse_obj_face_index(const std::string &token, int &index) {
  // OBJ face tokens can look like:
  // f 1 2 3
  // or f 1/1/1 2/2/2 3/3/3
  // For this assignment, we only need the vertex index before the first slash.

  size_t slash_pos = token.find('/');
  std::string index_text = token;

  if (slash_pos != std::string::npos) {
    index_text = token.substr(0, slash_pos);
  }

  if (index_text.empty()) {
    return false;
  }

  int obj_index = std::stoi(index_text);

  // OBJ indices start from 1, but C++ vector indices start from 0.
  if (obj_index <= 0) {
    return false;
  }

  index = obj_index - 1;
  return true;
}

static bool load_obj_file(const char *filename, Mesh &mesh, char *status, int status_size) {
  mesh.vertices.clear();
  mesh.faces.clear();

  std::ifstream file(filename);

  if (!file.is_open()) {
    snprintf(status, status_size, "Could not open OBJ file: %s", filename);
    return false;
  }

  std::string line;

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;

    if (prefix == "v") {
      float x;
      float y;
      float z;

      if (iss >> x >> y >> z) {
        mesh.vertices.push_back(glm::vec3(x, y, z));
      }
    } else if (prefix == "f") {
      std::string a;
      std::string b;
      std::string c;

      if (iss >> a >> b >> c) {
        Face face;

        try {
          if (parse_obj_face_index(a, face.v0) &&
              parse_obj_face_index(b, face.v1) &&
              parse_obj_face_index(c, face.v2)) {
            mesh.faces.push_back(face);
          }
        } catch (...) {
          snprintf(status, status_size, "Invalid face format in OBJ file");
          mesh.vertices.clear();
          mesh.faces.clear();
          return false;
        }
      }
    }
  }

  for (const Face &face : mesh.faces) {
    if (face.v0 < 0 || face.v0 >= (int)mesh.vertices.size() ||
        face.v1 < 0 || face.v1 >= (int)mesh.vertices.size() ||
        face.v2 < 0 || face.v2 >= (int)mesh.vertices.size()) {
      snprintf(status, status_size, "OBJ file contains a face with an invalid vertex index");
      mesh.vertices.clear();
      mesh.faces.clear();
      return false;
    }
  }

  if (mesh.vertices.empty() || mesh.faces.empty()) {
    snprintf(status, status_size, "OBJ file loaded, but it has no vertices or no faces");
    return false;
  }

  snprintf(status, status_size,
           "Loaded OBJ successfully: %zu vertices, %zu faces",
           mesh.vertices.size(),
           mesh.faces.size());

  return true;
}

static bool load_mesh_option(int option_index) {
  if (option_index < 0 || option_index >= MESH_OPTION_COUNT) {
    return false;
  }

  Mesh loaded_mesh;
  const MeshOption &option = MESH_OPTIONS[option_index];

  for (const char *path : option.paths) {
    if (load_obj_file(path, loaded_mesh, g_mesh_status, sizeof(g_mesh_status))) {
      g_loaded_mesh = loaded_mesh;
      g_mesh_loaded = true;
      g_selected_mesh_index = option_index;
      snprintf(g_mesh_path_used, sizeof(g_mesh_path_used), "%s", path);
      g_viewport_transform = calculate_viewport_transform(g_loaded_mesh);
      reset_all_transforms();
      return true;
    }
  }

  g_loaded_mesh = Mesh();
  g_mesh_loaded = false;
  snprintf(g_mesh_path_used, sizeof(g_mesh_path_used), "No valid path found");
  g_viewport_transform = {};
  return false;
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

  g_background_mode = BACKGROUND_PLAIN;

  // Load a small OBJ mesh once at startup
  g_mesh_loaded = load_mesh_option(DEFAULT_MESH_INDEX);

  // Set up char input callback for textbox input
  mfb_set_char_input_callback(
      [](struct mfb_window *w, unsigned int c) {
        extern void ui_bridge_char_input(struct mfb_window *, unsigned int);

        if (c == 'p' || c == 'P') {
          cycle_background_mode();
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

    g_frame_counter++;

    update_line_drawing(ctx);

    // 2. Scene Rendering
    render_background();

    // 3. UI Logic
    static bool quit_requested = false;
    static bool reset_widgets_requested = false;
    static bool reset_canvas_requested = false;

    mu_begin(ctx);

    // Build all UI windows before rendering them into the framebuffer.
    handle_ui_reset_requests(ctx, &reset_widgets_requested,
                             &reset_canvas_requested);
    draw_widgets_window(ctx, &quit_requested);
    draw_creative_canvas_window(ctx);
    draw_window_controls_window(ctx, &reset_widgets_requested,
                                &reset_canvas_requested);
    cache_all_ui_window_states(ctx);

    mu_end(ctx);

    if (quit_requested) {
      mfb_close(window);
      break;
    }

    // Draw line work after UI logic but before the UI renderer overlays the panels.
    draw_mesh_wireframe();
    draw_line_overlay();

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
