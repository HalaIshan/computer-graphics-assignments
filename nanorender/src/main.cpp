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
  std::vector<glm::vec3> face_normals;
  std::vector<glm::vec3> vertex_normals;
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
};

struct TransformParameters {
  glm::vec3 translation;
  glm::vec3 rotation_degrees;
  glm::vec3 scale;
};

struct CameraParameters {
  glm::vec3 position;
  glm::vec3 rotation_degrees;
};

struct PointLight {
  glm::vec3 position; // light position in world space
  glm::vec3 ambient;  // ambient light color/intensity
  glm::vec3 diffuse;  // diffuse light color for later parts
  glm::vec3 specular; // specular light color for later parts
};

struct Material {
  glm::vec3 ambient;  // material color reflected by ambient light
  glm::vec3 diffuse;  // material diffuse color for later parts
  glm::vec3 specular; // material specular color for later parts
};

struct ScreenPoint {
  int x;
  int y;
};

struct ScreenRect {
  int min_x;
  int min_y;
  int max_x;
  int max_y;
};

struct RasterVertex {
  ScreenPoint screen;
  float depth;
};

struct BarycentricWeights {
  float alpha;
  float beta;
  float gamma;
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

enum ProjectionMode {
  PROJECTION_ORTHOGRAPHIC,
  PROJECTION_PERSPECTIVE
};

enum RenderMode {
  RENDER_WIREFRAME,
  RENDER_FILLED_TRIANGLES,
  RENDER_DEPTH_MAP,
  RENDER_AMBIENT_LIGHTING,
  RENDER_FLAT_SHADING
};

enum TriangleColorMode {
  TRIANGLE_DEBUG_COLORS,
  TRIANGLE_AMBIENT_LIGHTING,
  TRIANGLE_FLAT_SHADING
};

static const char *WIDGETS_WINDOW_TITLE = "Scene Controls";            // Left scene panel
static const char *CANVAS_WINDOW_TITLE = "Creative Canvas";           // Drawing tools panel
static const char *CAMERA_CONTROLS_TITLE = "Camera Controls";         // Camera setup panel
static const char *LIGHTING_CONTROLS_TITLE = "Light and Material";       // Light and material panel
static const char *WINDOW_CONTROLS_TITLE = "Window Controls";         // Reset/show buttons panel
static const char *MESH_DETAILS_WINDOW_TITLE = "Mesh Details";       // Optional mesh debug info

static const char *MODEL_SEARCH_PATHS[] = {
    "assets/models/",
    "../assets/models/",
    "../../assets/models/"};

static const MeshOption MESH_OPTIONS[] = {
    {"simple_pyramid.obj"},
    {"regular_cube.obj"},
    {"angled_box.obj"}};

static const int MODEL_SEARCH_PATH_COUNT = sizeof(MODEL_SEARCH_PATHS) / sizeof(MODEL_SEARCH_PATHS[0]);
static const int MESH_OPTION_COUNT = sizeof(MESH_OPTIONS) / sizeof(MESH_OPTIONS[0]);
static const int DEFAULT_MESH_INDEX = 0; // Start with the pyramid for camera tests

static const char *RENDER_MODE_NAMES[] = {
    "Wireframe",
    "Filled triangles",
    "Depth map",
    "Ambient lighting",
    "Flat shading"};
static const int RENDER_MODE_COUNT = sizeof(RENDER_MODE_NAMES) / sizeof(RENDER_MODE_NAMES[0]);
static const float TRANSLATION_MIN = -2.0f;
static const float TRANSLATION_MAX = 2.0f;
static const float ROTATION_MIN = -180.0f;
static const float ROTATION_MAX = 180.0f;
static const float SCALE_MIN = 0.1f;
static const float SCALE_MAX = 2.0f;
static const float KEYBOARD_TRANSLATION_STEP = 0.03f; // Direct input movement speed
static const float KEYBOARD_ROTATION_STEP = 2.0f;     // Direct input rotation speed
static const float DEBUG_AXIS_LENGTH_RATIO = 0.65f;   // Axis length relative to mesh size
static const float NORMAL_LENGTH_RATIO = 0.18f;       // Normal debug length relative to mesh size
static const float NORMAL_ARROW_HEAD_RATIO = 0.35f;   // Arrow head length relative to normal line
static const float VIEWPORT_TARGET_SIZE = 300.0f;      // Half-size default fit for camera tests
static const float CAMERA_POSITION_MIN = -4.0f;        // Camera movement range
static const float CAMERA_POSITION_MAX = 8.0f;
static const float CAMERA_ROTATION_MIN = -180.0f;      // Camera rotation range
static const float CAMERA_ROTATION_MAX = 180.0f;
static const float DEFAULT_CAMERA_DISTANCE = 5.0f;     // Keeps perspective mode visible
static const float DEFAULT_FOV_DEGREES = 60.0f;
static const float DEFAULT_NEAR_PLANE = 0.1f;
static const float DEFAULT_FAR_PLANE = 50.0f;
static const float FOV_MIN = 30.0f;
static const float FOV_MAX = 120.0f;
static const float NEAR_PLANE_MIN = 0.1f;
static const float NEAR_PLANE_MAX = 5.0f;
static const float FAR_PLANE_MIN = 2.0f;
static const float FAR_PLANE_MAX = 50.0f;
static const float LIGHT_POSITION_MIN = -5.0f;
static const float LIGHT_POSITION_MAX = 5.0f;
static const float COLOR_INTENSITY_MIN = 0.0f;
static const float COLOR_INTENSITY_MAX = 1.0f;

static const mu_Rect DEFAULT_WIDGETS_RECT = {20, 20, 460, 860};             // Left scene controls
static const mu_Rect DEFAULT_WINDOW_CONTROLS_RECT = {1220, 20, 360, 250};   // Upper right
static const mu_Rect DEFAULT_CAMERA_CONTROLS_RECT = {1220, 270, 360, 600};  // Camera setup panel
static const mu_Rect DEFAULT_LIGHTING_CONTROLS_RECT = {1220, 270, 360, 620}; // Light/material panel
static const mu_Rect DEFAULT_CANVAS_RECT = {520, 260, 640, 360};             // Hidden drawing tools panel
static const mu_Rect DEFAULT_MESH_DETAILS_RECT = {500, 20, 420, 230};       // Hidden until requested

static const mu_Color KEYBOARD_COLOR = {100, 200, 255, 255};         // Light blue
static const mu_Color BACKGROUND_COLOR = {180, 255, 180, 255};       // Light green
static const mu_Color MESH_COLOR = {200, 180, 255, 255};             // Light purple
static const mu_Color TRANSFORM_COLOR = {255, 220, 120, 255};        // Warm yellow
static const mu_Color LINE_TOOLS_COLOR = {255, 180, 120, 255};       // Soft orange
static const mu_Color CAMERA_COLOR = {170, 230, 255, 255};           // Light cyan
static const mu_Color WINDOW_CONTROLS_COLOR = {220, 220, 255, 255};  // Pale blue
static const mu_Color DEBUG_COLOR = {130, 230, 255, 255};            // Cyan debug controls
static const mu_Color LIGHTING_COLOR = {255, 235, 150, 255};         // Warm lighting controls

static const uint32_t AXIS_X_COLOR = MFB_RGB(255, 70, 70);    // Red X axis
static const uint32_t AXIS_Y_COLOR = MFB_RGB(80, 230, 80);    // Green Y axis
static const uint32_t AXIS_Z_COLOR = MFB_RGB(90, 140, 255);   // Blue Z axis
static const uint32_t BOUNDING_BOX_COLOR = MFB_RGB(255, 225, 90); // Yellow box edges
static const uint32_t FACE_NORMAL_COLOR = MFB_RGB(255, 120, 220);  // Pink face normals
static const uint32_t VERTEX_NORMAL_COLOR = MFB_RGB(80, 255, 230); // Cyan vertex normals
static const uint32_t LIGHT_POSITION_COLOR = MFB_RGB(255, 245, 120); // Light helper

static const PointLight DEFAULT_POINT_LIGHT = {
    glm::vec3(1.5f, 1.5f, 2.0f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 1.0f)};
static const Material DEFAULT_MATERIAL = {
    glm::vec3(0.25f, 0.55f, 0.9f),
    glm::vec3(0.25f, 0.55f, 0.9f),
    glm::vec3(1.0f, 1.0f, 1.0f)};

// Framebuffer state
static uint32_t g_buffer[WIDTH * HEIGHT];
static float g_z_buffer[WIDTH * HEIGHT];

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
static bool g_render_mode_selector_open = false;

// Part 4 transformation state
static TransformParameters g_local_transform = {
    glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
static TransformParameters g_world_transform = {
    glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
static TransformSpace g_transform_space = TRANSFORM_LOCAL;
static TransformOperation g_transform_operation = TRANSFORM_TRANSLATION;
static float g_model_size = 1.0f; // Uniform size multiplier for the whole model
static TransformSpace g_keyboard_transform_space = TRANSFORM_WORLD; // Part 6 keyboard mode

// Assignment 3 camera state
static CameraParameters g_camera = {
    glm::vec3(0.0f, 0.0f, DEFAULT_CAMERA_DISTANCE), glm::vec3(0.0f)};

// Assignment 3 projection state
static ProjectionMode g_projection_mode = PROJECTION_PERSPECTIVE;
static float g_projection_fov_degrees = DEFAULT_FOV_DEGREES;
static float g_projection_near_plane = DEFAULT_NEAR_PLANE;
static float g_projection_far_plane = DEFAULT_FAR_PLANE;

// Assignment 5 lighting state
static PointLight g_point_light = DEFAULT_POINT_LIGHT;
static Material g_material = DEFAULT_MATERIAL;

// Assignment 3 visual debugging state
static int g_show_coordinate_axes = 0; // Draw world and local coordinate axes
static int g_show_bounding_box = 0;     // Draw transformed mesh bounding box
static int g_show_face_normals = 0;     // Draw one normal from each triangle center
static int g_show_vertex_normals = 0;   // Draw averaged normals from mesh vertices
static int g_show_light_position = 0;   // Draw point light helper

// Main render mode state
static RenderMode g_render_mode = RENDER_WIREFRAME;

// Line drawing state
static std::vector<DrawnLine> g_drawn_lines;
static int g_manual_line_drawing_enabled = 0;
static bool g_is_drawing_line = false;
static int g_line_start_x = 0;
static int g_line_start_y = 0;
static int g_line_current_x = 0;
static int g_line_current_y = 0;
static bool g_previous_left_down = false;
static bool g_previous_s_down = false;

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
static mu_Rect g_camera_controls_window_rect = DEFAULT_CAMERA_CONTROLS_RECT;
static mu_Rect g_lighting_controls_window_rect = DEFAULT_LIGHTING_CONTROLS_RECT;
static mu_Rect g_window_controls_rect = DEFAULT_WINDOW_CONTROLS_RECT;
static mu_Rect g_mesh_details_window_rect = DEFAULT_MESH_DETAILS_RECT;

static bool g_widgets_window_open = true;
static bool g_canvas_window_open = false;
static bool g_camera_controls_window_open = false;
static bool g_show_camera_controls_window = false;
static bool g_lighting_controls_window_open = true;
static bool g_window_controls_open = true;
static bool g_show_canvas_window = false;
static bool g_mesh_details_window_open = false;
static bool g_show_mesh_details_window = false;
static bool g_reset_mesh_details_window = false;

// General helpers
static uint32_t get_current_line_color() {
  return MFB_RGB((uint8_t)g_line_color_r,
                 (uint8_t)g_line_color_g,
                 (uint8_t)g_line_color_b);
}

static glm::vec3 normalize_or_zero(const glm::vec3 &value) {
  if (glm::length(value) <= 0.0001f) {
    return glm::vec3(0.0f);
  }

  return glm::normalize(value);
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

  if (g_show_canvas_window && g_canvas_window_open &&
      point_inside_rect(x, y, g_canvas_window_rect)) {
    return true;
  }

  if (g_camera_controls_window_open &&
      point_inside_rect(x, y, g_camera_controls_window_rect)) {
    return true;
  }

  if (g_window_controls_open &&
      point_inside_rect(x, y, g_window_controls_rect)) {
    return true;
  }

  if (g_show_mesh_details_window && g_mesh_details_window_open &&
      point_inside_rect(x, y, g_mesh_details_window_rect)) {
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
  if (!g_manual_line_drawing_enabled) {
    g_is_drawing_line = false;
    g_previous_left_down = (ctx->mouse_down & MU_MOUSE_LEFT) != 0;
    return;
  }

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

static void compact_axis_slider_row(mu_Context *ctx, const char *axis,
                                    float *value, float low, float high,
                                    const char *reset_label,
                                    float default_value = 0.0f) {
  int widths[] = {24, 210, 92};
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

static void keep_projection_planes_valid() {
  if (g_projection_near_plane >= g_projection_far_plane) {
    g_projection_far_plane = g_projection_near_plane + 0.1f;
  }

  if (g_projection_far_plane > FAR_PLANE_MAX) {
    g_projection_far_plane = FAR_PLANE_MAX;
    g_projection_near_plane = g_projection_far_plane - 0.1f;
  }
}

static void projection_value_slider_row(mu_Context *ctx, int *widths,
                                        const char *label, float *value,
                                        float low, float high) {
  char text[96];
  snprintf(text, sizeof(text), "%s %.2f", label, *value);
  label_row(ctx, widths, text);
  mu_slider(ctx, value, low, high);
}

static void reset_projection_parameters() {
  g_projection_fov_degrees = DEFAULT_FOV_DEGREES;
  g_projection_near_plane = DEFAULT_NEAR_PLANE;
  g_projection_far_plane = DEFAULT_FAR_PLANE;
}

static void reset_light_position() {
  g_point_light.position = DEFAULT_POINT_LIGHT.position;
}

static void reset_light_ambient() {
  g_point_light.ambient = DEFAULT_POINT_LIGHT.ambient;
}

static void reset_light_diffuse() {
  g_point_light.diffuse = DEFAULT_POINT_LIGHT.diffuse;
}

static void reset_material_ambient() {
  g_material.ambient = DEFAULT_MATERIAL.ambient;
}

static void reset_material_diffuse() {
  g_material.diffuse = DEFAULT_MATERIAL.diffuse;
}

static float clamp_color_value(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

static uint8_t color_float_to_byte(float value) {
  return (uint8_t)std::lround(clamp_color_value(value) * 255.0f);
}

static uint32_t rgb_float_color(const glm::vec3 &color) {
  return MFB_RGB(color_float_to_byte(color.r),
                 color_float_to_byte(color.g),
                 color_float_to_byte(color.b));
}

static glm::vec3 apply_model_transform(const glm::vec3 &vertex,
                                       const glm::mat4 &model_matrix);
static glm::vec3 transform_normal_direction(const glm::vec3 &normal,
                                            const glm::mat4 &model_matrix);

static glm::vec3 multiply_color(const glm::vec3 &a, const glm::vec3 &b) {
  return glm::vec3(a.r * b.r, a.g * b.g, a.b * b.b);
}

static glm::vec3 ambient_lighting_color() {
  return multiply_color(g_point_light.ambient, g_material.ambient);
}

static glm::vec3 flat_shading_color(size_t face_index,
                                    const glm::mat4 &model_matrix) {
  glm::vec3 color = ambient_lighting_color();

  if (face_index >= g_loaded_mesh.faces.size() ||
      face_index >= g_loaded_mesh.face_normals.size()) {
    return color;
  }

  const Face &face = g_loaded_mesh.faces[face_index];
  glm::vec3 world_v0 = apply_model_transform(g_loaded_mesh.vertices[face.v0],
                                             model_matrix);
  glm::vec3 world_v1 = apply_model_transform(g_loaded_mesh.vertices[face.v1],
                                             model_matrix);
  glm::vec3 world_v2 = apply_model_transform(g_loaded_mesh.vertices[face.v2],
                                             model_matrix);
  glm::vec3 face_center = (world_v0 + world_v1 + world_v2) / 3.0f;
  glm::vec3 normal = transform_normal_direction(g_loaded_mesh.face_normals[face_index],
                                                model_matrix);
  glm::vec3 light_direction = normalize_or_zero(g_point_light.position - face_center);
  float diffuse_strength = std::max(glm::dot(normal, light_direction), 0.0f);

  glm::vec3 diffuse = multiply_color(g_point_light.diffuse, g_material.diffuse) *
                      diffuse_strength;
  return color + diffuse;
}

static uint32_t ambient_lighting_debug_color() {
  return rgb_float_color(ambient_lighting_color());
}

static void compact_value_slider_row(mu_Context *ctx, const char *label,
                                     float *value, float low, float high) {
  int widths[] = {24, 230, 60};
  char value_text[32];
  snprintf(value_text, sizeof(value_text), "%.2f", *value);

  mu_layout_row(ctx, 3, widths, 0);
  mu_label(ctx, label);
  mu_slider(ctx, value, low, high);
  mu_label(ctx, value_text);
}

static void light_position_sliders(mu_Context *ctx, int *widths) {
  colored_label_row(ctx, widths, "Light position:", LIGHTING_COLOR);
  compact_value_slider_row(ctx, "X:", &g_point_light.position.x,
                           LIGHT_POSITION_MIN, LIGHT_POSITION_MAX);
  compact_value_slider_row(ctx, "Y:", &g_point_light.position.y,
                           LIGHT_POSITION_MIN, LIGHT_POSITION_MAX);
  compact_value_slider_row(ctx, "Z:", &g_point_light.position.z,
                           LIGHT_POSITION_MIN, LIGHT_POSITION_MAX);
}

static void color_group_sliders(mu_Context *ctx, int *widths,
                                const char *title, glm::vec3 *color) {
  colored_label_row(ctx, widths, title, LIGHTING_COLOR);
  compact_value_slider_row(ctx, "R:", &color->r,
                           COLOR_INTENSITY_MIN, COLOR_INTENSITY_MAX);
  compact_value_slider_row(ctx, "G:", &color->g,
                           COLOR_INTENSITY_MIN, COLOR_INTENSITY_MAX);
  compact_value_slider_row(ctx, "B:", &color->b,
                           COLOR_INTENSITY_MIN, COLOR_INTENSITY_MAX);
}

static bool compact_button(mu_Context *ctx, const char *label) {
  int widths[] = {55, 220, 55};
  mu_layout_row(ctx, 3, widths, 0);
  mu_label(ctx, "");
  bool clicked = mu_button(ctx, label);
  mu_label(ctx, "");
  return clicked;
}

static bool load_mesh_option(int option_index);

static void draw_keyboard_controls(mu_Context *ctx, int *widths) {
  mu_Color keyboard_color = KEYBOARD_COLOR;
  colored_label_row(ctx, widths, "Keyboard controls:", keyboard_color);
  colored_label_row(ctx, widths, "P: background (Plain > Circle > Checker)", keyboard_color);
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

  float target_size = VIEWPORT_TARGET_SIZE; // Leaves room for camera movement tests
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

static glm::mat4 build_projection_matrix() {
  float aspect_ratio = (float)WIDTH / (float)HEIGHT;
  return glm::perspective(glm::radians(g_projection_fov_degrees),
                          aspect_ratio,
                          g_projection_near_plane,
                          g_projection_far_plane);
}

static ScreenPoint ndc_to_screen(const glm::vec3 &ndc) {
  float screen_x = (ndc.x + 1.0f) * 0.5f * (float)(WIDTH - 1);
  float screen_y = (1.0f - (ndc.y + 1.0f) * 0.5f) * (float)(HEIGHT - 1);

  return {
      (int)std::lround(screen_x),
      (int)std::lround(screen_y)};
}

static bool project_vertex_perspective(const glm::vec3 &view_vertex,
                                       ScreenPoint &screen_point) {
  glm::vec4 clip = build_projection_matrix() * glm::vec4(view_vertex, 1.0f);

  if (std::abs(clip.w) < 0.0001f) {
    return false;
  }

  glm::vec3 ndc = glm::vec3(clip) / clip.w; // Perspective divide
  if (ndc.z < -1.0f || ndc.z > 1.0f) {
    return false;
  }

  screen_point = ndc_to_screen(ndc);
  return true;
}

static bool project_transformed_vertex(const glm::vec3 &view_vertex,
                                       ScreenPoint &screen_point) {
  if (g_projection_mode == PROJECTION_PERSPECTIVE) {
    return project_vertex_perspective(view_vertex, screen_point);
  }

  screen_point = project_vertex_orthographic(view_vertex, g_viewport_transform);
  return true;
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

static glm::mat4 build_camera_matrix() {
  glm::mat4 translation = glm::translate(glm::mat4(1.0f), g_camera.position);
  glm::mat4 rotation = build_rotation_matrix(g_camera.rotation_degrees);
  return translation * rotation;
}

static glm::mat4 build_view_matrix() {
  return glm::inverse(build_camera_matrix());
}

static glm::mat4 build_model_view_matrix(const glm::mat4 &model_matrix) {
  return build_view_matrix() * model_matrix;
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

static void reset_camera() {
  g_camera.position = glm::vec3(0.0f, 0.0f, DEFAULT_CAMERA_DISTANCE);
  g_camera.rotation_degrees = glm::vec3(0.0f);
}

static TransformParameters &transform_for_space(TransformSpace space) {
  return space == TRANSFORM_LOCAL ? g_local_transform : g_world_transform;
}

static TransformParameters &keyboard_transform() {
  return transform_for_space(g_keyboard_transform_space);
}

static void toggle_keyboard_transform_space() {
  g_keyboard_transform_space = g_keyboard_transform_space == TRANSFORM_LOCAL
                                   ? TRANSFORM_WORLD
                                   : TRANSFORM_LOCAL;
}

static void sync_transform_controls_to_keyboard(TransformOperation operation) {
  g_transform_space = g_keyboard_transform_space;
  g_transform_operation = operation;
}

static void update_keyboard_transform_input(struct mfb_window *window) {
  const uint8_t *keys = mfb_get_key_buffer(window);
  if (!keys) {
    return;
  }

  bool s_down = keys[MFB_KB_KEY_S] != 0;
  if (s_down && !g_previous_s_down) {
    toggle_keyboard_transform_space();
    g_transform_space = g_keyboard_transform_space;
  }
  g_previous_s_down = s_down;

  TransformParameters &transform = keyboard_transform();
  bool translation_changed = false;
  bool rotation_changed = false;

  if (keys[MFB_KB_KEY_LEFT]) {
    transform.translation.x -= KEYBOARD_TRANSLATION_STEP;
    translation_changed = true;
  }
  if (keys[MFB_KB_KEY_RIGHT]) {
    transform.translation.x += KEYBOARD_TRANSLATION_STEP;
    translation_changed = true;
  }
  if (keys[MFB_KB_KEY_UP]) {
    transform.translation.y += KEYBOARD_TRANSLATION_STEP;
    translation_changed = true;
  }
  if (keys[MFB_KB_KEY_DOWN]) {
    transform.translation.y -= KEYBOARD_TRANSLATION_STEP;
    translation_changed = true;
  }
  if (keys[MFB_KB_KEY_Q]) {
    transform.rotation_degrees.z -= KEYBOARD_ROTATION_STEP;
    rotation_changed = true;
  }
  if (keys[MFB_KB_KEY_E]) {
    transform.rotation_degrees.z += KEYBOARD_ROTATION_STEP;
    rotation_changed = true;
  }

  if (translation_changed) {
    sync_transform_controls_to_keyboard(TRANSFORM_TRANSLATION);
  } else if (rotation_changed) {
    sync_transform_controls_to_keyboard(TRANSFORM_ROTATION);
  }
}

static TransformParameters &selected_transform() {
  return transform_for_space(g_transform_space);
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
  colored_label_row(ctx, widths, "Model and transforms:", mesh_color);

  char selector_text[128];
  snprintf(selector_text, sizeof(selector_text),
           "Choose model: %s", MESH_OPTIONS[g_selected_mesh_index].name);

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
}

static void draw_mesh_details_window(mu_Context *ctx) {
  if (!g_show_mesh_details_window) {
    return;
  }

  if (g_reset_mesh_details_window) {
    reset_ui_window(ctx, MESH_DETAILS_WINDOW_TITLE, DEFAULT_MESH_DETAILS_RECT);
    g_reset_mesh_details_window = false;
  }

  if (mu_begin_window_ex(ctx, MESH_DETAILS_WINDOW_TITLE,
                         DEFAULT_MESH_DETAILS_RECT, 0)) {
    int widths[] = {-1};
    g_mesh_details_window_open = true;

    colored_label_row(ctx, widths, "Mesh details:", MESH_COLOR);

    char mesh_summary_text[128];
    snprintf(mesh_summary_text, sizeof(mesh_summary_text),
             "Loaded: %s | V: %zu | F: %zu",
             g_mesh_loaded ? "yes" : "no",
             g_loaded_mesh.vertices.size(),
             g_loaded_mesh.faces.size());
    label_row(ctx, widths, mesh_summary_text);

    if (!g_viewport_transform.valid) {
      label_row(ctx, widths, "Viewport: not available");
      mu_end_window(ctx);
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

    mu_end_window(ctx);
  } else {
    g_mesh_details_window_open = false;
    g_show_mesh_details_window = false;
  }
}

static void draw_transform_controls(mu_Context *ctx, int *widths) {
  int space_widths[] = {210, 210};
  mu_layout_row(ctx, 2, space_widths, 0);
  if (mu_button(ctx, g_transform_space == TRANSFORM_WORLD ? "> World" : "World")) {
    g_transform_space = TRANSFORM_WORLD;
  }
  if (mu_button(ctx, g_transform_space == TRANSFORM_LOCAL ? "> Local" : "Local")) {
    g_transform_space = TRANSFORM_LOCAL;
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

  spacer_row(ctx, widths, 8);
  colored_label_row(ctx, widths, "Debug drawing:", DEBUG_COLOR);
  checkbox_row(ctx, widths, "Show coordinate axes", &g_show_coordinate_axes);
  checkbox_row(ctx, widths, "Show bounding box", &g_show_bounding_box);
  checkbox_row(ctx, widths, "Show face normals", &g_show_face_normals);
  checkbox_row(ctx, widths, "Show vertex normals", &g_show_vertex_normals);
  checkbox_row(ctx, widths, "Show light position", &g_show_light_position);

  spacer_row(ctx, widths, 8);
  colored_label_row(ctx, widths, "Render mode:", DEBUG_COLOR);
  char render_mode_text[128];
  snprintf(render_mode_text, sizeof(render_mode_text),
           "Mode: %s", RENDER_MODE_NAMES[(int)g_render_mode]);
  if (button_row(ctx, widths, render_mode_text)) {
    g_render_mode_selector_open = !g_render_mode_selector_open;
  }

  if (g_render_mode_selector_open) {
    for (int i = 0; i < RENDER_MODE_COUNT; i++) {
      char option_text[128];
      snprintf(option_text, sizeof(option_text),
               "%s%s",
               i == (int)g_render_mode ? "> " : "  ",
               RENDER_MODE_NAMES[i]);

      if (button_row(ctx, widths, option_text)) {
        g_render_mode = (RenderMode)i;
        g_render_mode_selector_open = false;
      }
    }
  }

  const char *keyboard_mode =
      g_keyboard_transform_space == TRANSFORM_LOCAL ? "Local" : "World";
  char keyboard_mode_text[64];
  snprintf(keyboard_mode_text, sizeof(keyboard_mode_text),
           "Mode: %s", keyboard_mode);

  colored_label_row(ctx, widths, "Keyboard input:", TRANSFORM_COLOR);
  label_row(ctx, widths, "S: toggle Local/World");
  label_row(ctx, widths, "Arrows: translate X/Y");
  label_row(ctx, widths, "Q/E: rotate Z");
  label_row(ctx, widths, keyboard_mode_text);
}

static void draw_widgets_window(mu_Context *ctx, bool *quit_requested) {
  if (mu_begin_window(ctx, WIDGETS_WINDOW_TITLE, DEFAULT_WIDGETS_RECT)) {
    int widths[] = {-1};

    draw_keyboard_controls(ctx, widths);

    if (g_background_mode != BACKGROUND_PLAIN) {
      spacer_row(ctx, widths, 8);
      draw_background_controls(ctx, widths);
    }

    draw_mesh_controls(ctx, widths);
    draw_transform_controls(ctx, widths);

    if (button_row(ctx, widths, "Quit")) {
      *quit_requested = true;
    }

    mu_end_window(ctx);
  }
}

static void draw_creative_canvas_window(mu_Context *ctx) {
  if (!g_show_canvas_window) {
    return;
  }

  if (mu_begin_window(ctx, CANVAS_WINDOW_TITLE, DEFAULT_CANVAS_RECT)) {
    int widths[] = {-1};
    mu_Color line_tools_color = LINE_TOOLS_COLOR;

    colored_label_row(ctx, widths, "Line drawing tools:", line_tools_color);
    checkbox_row(ctx, widths, "Enable manual drawing", &g_manual_line_drawing_enabled);
    if (g_manual_line_drawing_enabled) {
      colored_label_row(ctx, widths, "Drag in the empty area to draw a line", line_tools_color);
    } else {
      colored_label_row(ctx, widths, "Manual drawing is off", line_tools_color);
      g_is_drawing_line = false;
    }

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
  } else {
    g_canvas_window_open = false;
    g_show_canvas_window = false;
  }
}

static void draw_camera_controls_window(mu_Context *ctx) {
  if (!g_show_camera_controls_window) {
    return;
  }

  if (mu_begin_window(ctx, CAMERA_CONTROLS_TITLE, DEFAULT_CAMERA_CONTROLS_RECT)) {
    int widths[] = {-1};

    colored_label_row(ctx, widths, "Camera controls:", CAMERA_COLOR);
    colored_label_row(ctx, widths, "Projection mode:", CAMERA_COLOR);
    int projection_widths[] = {160, 160};
    mu_layout_row(ctx, 2, projection_widths, 0);
    if (mu_button(ctx, g_projection_mode == PROJECTION_ORTHOGRAPHIC
                           ? "> Orthographic" : "Orthographic")) {
      g_projection_mode = PROJECTION_ORTHOGRAPHIC;
    }
    if (mu_button(ctx, g_projection_mode == PROJECTION_PERSPECTIVE
                           ? "> Perspective" : "Perspective")) {
      g_projection_mode = PROJECTION_PERSPECTIVE;
    }

    if (g_projection_mode == PROJECTION_PERSPECTIVE) {
      projection_value_slider_row(ctx, widths, "FOV / View zoom:",
                                  &g_projection_fov_degrees, FOV_MIN, FOV_MAX);
      projection_value_slider_row(ctx, widths, "Near:",
                                  &g_projection_near_plane,
                                  NEAR_PLANE_MIN, NEAR_PLANE_MAX);
      projection_value_slider_row(ctx, widths, "Far:",
                                  &g_projection_far_plane,
                                  FAR_PLANE_MIN, FAR_PLANE_MAX);
      keep_projection_planes_valid();

      if (button_row(ctx, widths, "Reset Projection")) {
        reset_projection_parameters();
      }
    }

    spacer_row(ctx, widths, 6);
    colored_label_row(ctx, widths, "Camera position:", CAMERA_COLOR);
    compact_axis_slider_row(ctx, "X:", &g_camera.position.x,
                            CAMERA_POSITION_MIN, CAMERA_POSITION_MAX, "Reset PX");
    compact_axis_slider_row(ctx, "Y:", &g_camera.position.y,
                            CAMERA_POSITION_MIN, CAMERA_POSITION_MAX, "Reset PY");
    compact_axis_slider_row(ctx, "Z:", &g_camera.position.z,
                            CAMERA_POSITION_MIN, CAMERA_POSITION_MAX, "Reset PZ",
                            DEFAULT_CAMERA_DISTANCE);

    spacer_row(ctx, widths, 6);
    colored_label_row(ctx, widths, "Camera rotation:", CAMERA_COLOR);
    compact_axis_slider_row(ctx, "X:", &g_camera.rotation_degrees.x,
                            CAMERA_ROTATION_MIN, CAMERA_ROTATION_MAX, "Reset RX");
    compact_axis_slider_row(ctx, "Y:", &g_camera.rotation_degrees.y,
                            CAMERA_ROTATION_MIN, CAMERA_ROTATION_MAX, "Reset RY");
    compact_axis_slider_row(ctx, "Z:", &g_camera.rotation_degrees.z,
                            CAMERA_ROTATION_MIN, CAMERA_ROTATION_MAX, "Reset RZ");

    if (button_row(ctx, widths, "Reset Camera")) {
      reset_camera();
    }

    mu_end_window(ctx);
  } else {
    g_camera_controls_window_open = false;
    g_show_camera_controls_window = false;
  }
}

static void draw_lighting_controls_window(mu_Context *ctx) {
  if (mu_begin_window(ctx, LIGHTING_CONTROLS_TITLE, DEFAULT_LIGHTING_CONTROLS_RECT)) {
    int widths[] = {-1};

    colored_label_row(ctx, widths, "Light controls:", LIGHTING_COLOR);
    light_position_sliders(ctx, widths);
    if (compact_button(ctx, "Reset Light Position")) {
      reset_light_position();
    }

    spacer_row(ctx, widths, 6);
    color_group_sliders(ctx, widths, "Light ambient:", &g_point_light.ambient);
    if (compact_button(ctx, "Reset Light Ambient")) {
      reset_light_ambient();
    }

    spacer_row(ctx, widths, 6);
    color_group_sliders(ctx, widths, "Light diffuse:", &g_point_light.diffuse);
    if (compact_button(ctx, "Reset Light Diffuse")) {
      reset_light_diffuse();
    }

    spacer_row(ctx, widths, 8);
    colored_label_row(ctx, widths, "Material controls:", LIGHTING_COLOR);
    color_group_sliders(ctx, widths, "Material ambient:", &g_material.ambient);
    if (compact_button(ctx, "Reset Material Ambient")) {
      reset_material_ambient();
    }

    spacer_row(ctx, widths, 6);
    color_group_sliders(ctx, widths, "Material diffuse:", &g_material.diffuse);
    if (compact_button(ctx, "Reset Material Diffuse")) {
      reset_material_diffuse();
    }

    mu_end_window(ctx);
  } else {
    g_lighting_controls_window_open = false;
  }
}

static void draw_window_controls_window(mu_Context *ctx,
                                        bool *reset_widgets_requested,
                                        bool *reset_canvas_requested,
                                        bool *reset_camera_controls_requested,
                                        bool *reset_lighting_controls_requested,
                                        bool *show_mesh_details_requested) {
  int window_controls_options = MU_OPT_NOCLOSE | MU_OPT_NORESIZE;

  if (mu_begin_window_ex(ctx, WINDOW_CONTROLS_TITLE,
                         DEFAULT_WINDOW_CONTROLS_RECT, window_controls_options)) {
    int widths[] = {-1};

    colored_label_row(ctx, widths, "Window controls:", WINDOW_CONTROLS_COLOR);

    if (button_row(ctx, widths, "Show / Reset Scene Controls")) {
      *reset_widgets_requested = true;
    }

    if (button_row(ctx, widths, "Show / Reset Camera Controls")) {
      *reset_camera_controls_requested = true;
    }

    if (button_row(ctx, widths, "Show / Reset Lighting Controls")) {
      *reset_lighting_controls_requested = true;
    }

    if (button_row(ctx, widths, "Show / Reset Canvas")) {
      *reset_canvas_requested = true;
    }

    if (button_row(ctx, widths, "Show Mesh Details")) {
      *show_mesh_details_requested = true;
    }

    mu_end_window(ctx);
  }
}

static void handle_ui_reset_requests(mu_Context *ctx,
                                     bool *reset_widgets_requested,
                                     bool *reset_canvas_requested,
                                     bool *reset_camera_controls_requested,
                                     bool *reset_lighting_controls_requested,
                                     bool *show_mesh_details_requested) {
  // Reset requests are set by buttons, then applied at the start of the next UI frame.
  if (*reset_widgets_requested) {
    reset_ui_window(ctx, WIDGETS_WINDOW_TITLE, DEFAULT_WIDGETS_RECT);
    *reset_widgets_requested = false;
  }

  if (*reset_canvas_requested) {
    g_show_canvas_window = true;
    g_canvas_window_open = true;
    reset_ui_window(ctx, CANVAS_WINDOW_TITLE, DEFAULT_CANVAS_RECT);
    *reset_canvas_requested = false;
  }

  if (*reset_camera_controls_requested) {
    g_show_camera_controls_window = true;
    g_camera_controls_window_open = true;
    reset_ui_window(ctx, CAMERA_CONTROLS_TITLE, DEFAULT_CAMERA_CONTROLS_RECT);
    *reset_camera_controls_requested = false;
  }

  if (*reset_lighting_controls_requested) {
    g_lighting_controls_window_open = true;
    reset_ui_window(ctx, LIGHTING_CONTROLS_TITLE, DEFAULT_LIGHTING_CONTROLS_RECT);
    *reset_lighting_controls_requested = false;
  }

  if (*show_mesh_details_requested) {
    g_show_mesh_details_window = true;
    g_mesh_details_window_open = true;
    g_reset_mesh_details_window = true;
    *show_mesh_details_requested = false;
  }
}

static void cache_all_ui_window_states(mu_Context *ctx) {
  // Cached rectangles let drawing input ignore clicks that begin on top of UI.
  cache_ui_window_state(ctx, WIDGETS_WINDOW_TITLE,
                        &g_widgets_window_rect, &g_widgets_window_open, false);

  if (g_show_canvas_window) {
    cache_ui_window_state(ctx, CANVAS_WINDOW_TITLE,
                          &g_canvas_window_rect, &g_canvas_window_open, false);
  }

  if (g_show_camera_controls_window) {
    cache_ui_window_state(ctx, CAMERA_CONTROLS_TITLE,
                          &g_camera_controls_window_rect, &g_camera_controls_window_open, false);
  }
  cache_ui_window_state(ctx, LIGHTING_CONTROLS_TITLE,
                        &g_lighting_controls_window_rect, &g_lighting_controls_window_open, false);
  cache_ui_window_state(ctx, WINDOW_CONTROLS_TITLE,
                        &g_window_controls_rect, &g_window_controls_open, true);

  if (g_show_mesh_details_window) {
    cache_ui_window_state(ctx, MESH_DETAILS_WINDOW_TITLE,
                          &g_mesh_details_window_rect, &g_mesh_details_window_open, false);
  }
}

// Drawing helpers
static void draw_pixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  g_buffer[y * WIDTH + x] = color;
}

static const float Z_BUFFER_INFINITY = 1.0e30f;

static void clear_z_buffer() {
  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    g_z_buffer[i] = Z_BUFFER_INFINITY;
  }
}

static bool depth_test_and_write(int x, int y, float depth) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return false;
  }

  int index = y * WIDTH + x;
  if (depth < g_z_buffer[index]) {
    g_z_buffer[index] = depth;
    return true;
  }

  return false;
}

static void draw_depth_map() {
  float nearest = Z_BUFFER_INFINITY;
  float farthest = 0.0f;

  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    float depth = g_z_buffer[i];
    if (depth < Z_BUFFER_INFINITY) {
      nearest = std::min(nearest, depth);
      farthest = std::max(farthest, depth);
    }
  }

  if (nearest == Z_BUFFER_INFINITY) {
    return;
  }

  float range = farthest - nearest;
  if (range < 0.0001f) {
    range = 1.0f;
  }

  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    float depth = g_z_buffer[i];
    uint8_t gray = 255;

    if (depth < Z_BUFFER_INFINITY) {
      float normalized = (depth - nearest) / range;
      normalized = std::max(0.0f, std::min(1.0f, normalized));
      gray = (uint8_t)std::lround(normalized * 255.0f);
    }

    g_buffer[i] = MFB_RGB(gray, gray, gray);
  }
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

static void draw_view_space_3d_line(const glm::vec3 &start,
                                    const glm::vec3 &end,
                                    uint32_t color) {
  ScreenPoint p0;
  ScreenPoint p1;
  if (!project_transformed_vertex(start, p0) ||
      !project_transformed_vertex(end, p1)) {
    return;
  }

  draw_line(p0.x, p0.y, p1.x, p1.y, color);
}

static void draw_transformed_3d_line(const glm::vec3 &start,
                                     const glm::vec3 &end,
                                     const glm::mat4 &model_matrix,
                                     uint32_t color) {
  glm::vec3 transformed_start = apply_model_transform(start, model_matrix);
  glm::vec3 transformed_end = apply_model_transform(end, model_matrix);
  draw_view_space_3d_line(transformed_start, transformed_end, color);
}

static uint32_t triangle_debug_color(size_t face_index) {
  uint32_t value = (uint32_t)(face_index + 1) * 2654435761u;
  uint8_t r = 80 + (uint8_t)(value & 0x7f);
  uint8_t g = 80 + (uint8_t)((value >> 8) & 0x7f);
  uint8_t b = 80 + (uint8_t)((value >> 16) & 0x7f);
  return MFB_RGB(r, g, b);
}

static ScreenRect triangle_screen_bounds(const RasterVertex &v0,
                                         const RasterVertex &v1,
                                         const RasterVertex &v2) {
  ScreenRect rect;
  rect.min_x = std::min(v0.screen.x, std::min(v1.screen.x, v2.screen.x));
  rect.min_y = std::min(v0.screen.y, std::min(v1.screen.y, v2.screen.y));
  rect.max_x = std::max(v0.screen.x, std::max(v1.screen.x, v2.screen.x));
  rect.max_y = std::max(v0.screen.y, std::max(v1.screen.y, v2.screen.y));
  return rect;
}

static bool clip_screen_rect(ScreenRect &rect) {
  if (rect.max_x < 0 || rect.max_y < 0 ||
      rect.min_x >= WIDTH || rect.min_y >= HEIGHT) {
    return false;
  }

  rect.min_x = std::max(rect.min_x, 0);
  rect.min_y = std::max(rect.min_y, 0);
  rect.max_x = std::min(rect.max_x, WIDTH - 1);
  rect.max_y = std::min(rect.max_y, HEIGHT - 1);
  return rect.min_x <= rect.max_x && rect.min_y <= rect.max_y;
}

static BarycentricWeights barycentric_coordinates(float x, float y,
                                                  const ScreenPoint &p0,
                                                  const ScreenPoint &p1,
                                                  const ScreenPoint &p2) {
  float denominator = (float)((p1.y - p2.y) * (p0.x - p2.x) +
                              (p2.x - p1.x) * (p0.y - p2.y));

  if (std::abs(denominator) < 0.0001f) {
    return {-1.0f, -1.0f, -1.0f};
  }

  float alpha = ((float)(p1.y - p2.y) * (x - (float)p2.x) +
                 (float)(p2.x - p1.x) * (y - (float)p2.y)) / denominator;
  float beta = ((float)(p2.y - p0.y) * (x - (float)p2.x) +
                (float)(p0.x - p2.x) * (y - (float)p2.y)) / denominator;
  float gamma = 1.0f - alpha - beta;

  return {alpha, beta, gamma};
}

static bool is_inside_triangle(const BarycentricWeights &weights) {
  return weights.alpha >= 0.0f && weights.alpha <= 1.0f &&
         weights.beta >= 0.0f && weights.beta <= 1.0f &&
         weights.gamma >= 0.0f && weights.gamma <= 1.0f;
}

static float interpolate_depth(const BarycentricWeights &weights,
                               const RasterVertex &v0,
                               const RasterVertex &v1,
                               const RasterVertex &v2) {
  return weights.alpha * v0.depth +
         weights.beta * v1.depth +
         weights.gamma * v2.depth;
}

static void fill_triangle_in_bounds(const ScreenRect &rect,
                                    const RasterVertex &v0,
                                    const RasterVertex &v1,
                                    const RasterVertex &v2,
                                    uint32_t color) {
  for (int y = rect.min_y; y <= rect.max_y; y++) {
    for (int x = rect.min_x; x <= rect.max_x; x++) {
      BarycentricWeights weights = barycentric_coordinates((float)x + 0.5f,
                                                           (float)y + 0.5f,
                                                           v0.screen,
                                                           v1.screen,
                                                           v2.screen);
      if (!is_inside_triangle(weights)) {
        continue;
      }

      float depth = interpolate_depth(weights, v0, v1, v2);
      if (depth_test_and_write(x, y, depth)) {
        draw_pixel(x, y, color);
      }
    }
  }
}

static float view_depth(const glm::vec3 &view_vertex) {
  return std::abs(view_vertex.z);
}

static bool project_face_to_screen(const Face &face,
                                   const glm::mat4 &model_view_matrix,
                                   RasterVertex &v0,
                                   RasterVertex &v1,
                                   RasterVertex &v2) {
  glm::vec3 view_v0 = apply_model_transform(g_loaded_mesh.vertices[face.v0],
                                            model_view_matrix);
  glm::vec3 view_v1 = apply_model_transform(g_loaded_mesh.vertices[face.v1],
                                            model_view_matrix);
  glm::vec3 view_v2 = apply_model_transform(g_loaded_mesh.vertices[face.v2],
                                            model_view_matrix);

  if (!project_transformed_vertex(view_v0, v0.screen) ||
      !project_transformed_vertex(view_v1, v1.screen) ||
      !project_transformed_vertex(view_v2, v2.screen)) {
    return false;
  }

  v0.depth = view_depth(view_v0);
  v1.depth = view_depth(view_v1);
  v2.depth = view_depth(view_v2);
  return true;
}

static uint32_t raster_triangle_color(size_t face_index,
                                      TriangleColorMode color_mode,
                                      const glm::mat4 &model_matrix) {
  switch (color_mode) {
  case TRIANGLE_AMBIENT_LIGHTING:
    return ambient_lighting_debug_color();
  case TRIANGLE_FLAT_SHADING:
    return rgb_float_color(flat_shading_color(face_index, model_matrix));
  case TRIANGLE_DEBUG_COLORS:
  default:
    return triangle_debug_color(face_index);
  }
}

static void draw_rasterized_triangles(TriangleColorMode color_mode) {
  if (!g_mesh_loaded || !g_viewport_transform.valid) {
    return;
  }

  glm::mat4 model_matrix = build_model_matrix();
  glm::mat4 model_view_matrix = build_model_view_matrix(model_matrix);

  for (size_t i = 0; i < g_loaded_mesh.faces.size(); i++) {
    RasterVertex v0;
    RasterVertex v1;
    RasterVertex v2;
    if (!project_face_to_screen(g_loaded_mesh.faces[i], model_view_matrix,
                                v0, v1, v2)) {
      continue;
    }

    ScreenRect rect = triangle_screen_bounds(v0, v1, v2);
    if (!clip_screen_rect(rect)) {
      continue;
    }

    uint32_t color = raster_triangle_color(i, color_mode, model_matrix);
    fill_triangle_in_bounds(rect, v0, v1, v2, color);
  }
}

static void draw_filled_triangles() {
  draw_rasterized_triangles(TRIANGLE_DEBUG_COLORS);
}

static void draw_ambient_lighting() {
  draw_rasterized_triangles(TRIANGLE_AMBIENT_LIGHTING);
}

static void draw_flat_shading() {
  draw_rasterized_triangles(TRIANGLE_FLAT_SHADING);
}

static void draw_mesh_wireframe() {
  if (!g_mesh_loaded || !g_viewport_transform.valid) {
    return;
  }

  uint32_t wireframe_color = get_wireframe_color();
  glm::mat4 model_matrix = build_model_view_matrix(build_model_matrix());

  for (const Face &face : g_loaded_mesh.faces) {
    const glm::vec3 &v0 = g_loaded_mesh.vertices[face.v0];
    const glm::vec3 &v1 = g_loaded_mesh.vertices[face.v1];
    const glm::vec3 &v2 = g_loaded_mesh.vertices[face.v2];

    draw_transformed_3d_line(v0, v1, model_matrix, wireframe_color);
    draw_transformed_3d_line(v1, v2, model_matrix, wireframe_color);
    draw_transformed_3d_line(v2, v0, model_matrix, wireframe_color);
  }
}

static float mesh_max_extent() {
  glm::vec3 mesh_size = g_viewport_transform.box.max - g_viewport_transform.box.min;
  return std::max(mesh_size.x, std::max(mesh_size.y, mesh_size.z));
}

static float debug_length(float ratio) {
  return mesh_max_extent() * ratio;
}

static float debug_axis_length() {
  return debug_length(DEBUG_AXIS_LENGTH_RATIO);
}

static float normal_debug_length() {
  return debug_length(NORMAL_LENGTH_RATIO);
}

static void draw_projected_3d_line(const glm::vec3 &start,
                                   const glm::vec3 &end,
                                   uint32_t color) {
  draw_transformed_3d_line(start, end, build_view_matrix(), color);
}

static void draw_axis_tripod(const glm::vec3 &origin,
                             const glm::vec3 &x_end,
                             const glm::vec3 &y_end,
                             const glm::vec3 &z_end) {
  draw_projected_3d_line(origin, x_end, AXIS_X_COLOR);
  draw_projected_3d_line(origin, y_end, AXIS_Y_COLOR);
  draw_projected_3d_line(origin, z_end, AXIS_Z_COLOR);
}

static void draw_coordinate_axes() {
  if (!g_mesh_loaded || !g_viewport_transform.valid || !g_show_coordinate_axes) {
    return;
  }

  float axis_length = debug_axis_length();
  if (axis_length <= 0.0f) {
    return;
  }

  glm::vec3 world_origin(0.0f);
  draw_axis_tripod(world_origin,
                   world_origin + glm::vec3(axis_length, 0.0f, 0.0f),
                   world_origin + glm::vec3(0.0f, axis_length, 0.0f),
                   world_origin + glm::vec3(0.0f, 0.0f, axis_length));

  glm::mat4 model_matrix = build_model_view_matrix(build_model_matrix());
  glm::vec3 local_origin = g_viewport_transform.center;
  glm::vec3 local_x = local_origin + glm::vec3(axis_length, 0.0f, 0.0f);
  glm::vec3 local_y = local_origin + glm::vec3(0.0f, axis_length, 0.0f);
  glm::vec3 local_z = local_origin + glm::vec3(0.0f, 0.0f, axis_length);

  draw_transformed_3d_line(local_origin, local_x, model_matrix, AXIS_X_COLOR);
  draw_transformed_3d_line(local_origin, local_y, model_matrix, AXIS_Y_COLOR);
  draw_transformed_3d_line(local_origin, local_z, model_matrix, AXIS_Z_COLOR);
}

static void draw_bounding_box() {
  if (!g_mesh_loaded || !g_viewport_transform.valid || !g_show_bounding_box) {
    return;
  }

  const BoundingBox &box = g_viewport_transform.box;
  const glm::vec3 corners[] = {
      {box.min.x, box.min.y, box.min.z},
      {box.max.x, box.min.y, box.min.z},
      {box.min.x, box.max.y, box.min.z},
      {box.max.x, box.max.y, box.min.z},
      {box.min.x, box.min.y, box.max.z},
      {box.max.x, box.min.y, box.max.z},
      {box.min.x, box.max.y, box.max.z},
      {box.max.x, box.max.y, box.max.z}};

  const int edges[][2] = {
      {0, 1}, {1, 3}, {3, 2}, {2, 0},
      {4, 5}, {5, 7}, {7, 6}, {6, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7}};

  glm::mat4 model_matrix = build_model_view_matrix(build_model_matrix());
  for (const auto &edge : edges) {
    draw_transformed_3d_line(corners[edge[0]], corners[edge[1]],
                             model_matrix, BOUNDING_BOX_COLOR);
  }
}

static glm::vec3 transform_normal_direction(const glm::vec3 &normal,
                                            const glm::mat4 &model_view_matrix) {
  glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model_view_matrix)));
  return normalize_or_zero(normal_matrix * normal);
}

static glm::vec3 normal_arrow_side_direction(const glm::vec3 &view_normal) {
  glm::vec3 reference_axis = std::abs(view_normal.y) < 0.9f
                                 ? glm::vec3(0.0f, 1.0f, 0.0f)
                                 : glm::vec3(1.0f, 0.0f, 0.0f);
  return normalize_or_zero(glm::cross(view_normal, reference_axis));
}

static void draw_normal_arrow_head(const glm::vec3 &tip,
                                   const glm::vec3 &view_normal,
                                   float length,
                                   uint32_t color) {
  float head_length = length * NORMAL_ARROW_HEAD_RATIO;
  glm::vec3 side = normal_arrow_side_direction(view_normal);

  if (head_length <= 0.0f || glm::length(side) <= 0.0001f) {
    return;
  }

  glm::vec3 base = tip - view_normal * head_length;
  draw_view_space_3d_line(tip, base + side * head_length * 0.5f, color);
  draw_view_space_3d_line(tip, base - side * head_length * 0.5f, color);
}

static void draw_normal_debug_line(const glm::vec3 &origin,
                                   const glm::vec3 &normal,
                                   const glm::mat4 &model_view_matrix,
                                   float length,
                                   uint32_t color) {
  glm::vec3 view_origin = apply_model_transform(origin, model_view_matrix);
  glm::vec3 view_normal = transform_normal_direction(normal, model_view_matrix);

  if (glm::length(view_normal) <= 0.0001f) {
    return;
  }

  glm::vec3 tip = view_origin + view_normal * length;
  draw_view_space_3d_line(view_origin, tip, color);
  draw_normal_arrow_head(tip, view_normal, length, color);
}

static void draw_face_normals() {
  if (!g_mesh_loaded || !g_viewport_transform.valid || !g_show_face_normals) {
    return;
  }

  if (g_loaded_mesh.face_normals.size() != g_loaded_mesh.faces.size()) {
    return;
  }

  float normal_length = normal_debug_length();
  if (normal_length <= 0.0f) {
    return;
  }

  glm::mat4 model_view_matrix = build_model_view_matrix(build_model_matrix());
  for (size_t i = 0; i < g_loaded_mesh.faces.size(); i++) {
    const Face &face = g_loaded_mesh.faces[i];
    glm::vec3 center = (g_loaded_mesh.vertices[face.v0] +
                        g_loaded_mesh.vertices[face.v1] +
                        g_loaded_mesh.vertices[face.v2]) / 3.0f;

    draw_normal_debug_line(center, g_loaded_mesh.face_normals[i],
                           model_view_matrix, normal_length, FACE_NORMAL_COLOR);
  }
}

static void draw_light_position() {
  if (!g_mesh_loaded || !g_viewport_transform.valid || !g_show_light_position) {
    return;
  }

  glm::mat4 model_matrix = build_model_matrix();
  glm::mat4 view_matrix = build_view_matrix();
  glm::vec3 world_center = apply_model_transform(g_viewport_transform.center,
                                                 model_matrix);
  glm::vec3 view_light = apply_model_transform(g_point_light.position,
                                               view_matrix);
  glm::vec3 view_center = apply_model_transform(world_center, view_matrix);
  float marker_size = 0.12f;

  draw_view_space_3d_line(view_light, view_center, LIGHT_POSITION_COLOR);
  draw_view_space_3d_line(view_light + glm::vec3(-marker_size, 0.0f, 0.0f),
                          view_light + glm::vec3(marker_size, 0.0f, 0.0f),
                          LIGHT_POSITION_COLOR);
  draw_view_space_3d_line(view_light + glm::vec3(0.0f, -marker_size, 0.0f),
                          view_light + glm::vec3(0.0f, marker_size, 0.0f),
                          LIGHT_POSITION_COLOR);
  draw_view_space_3d_line(view_light + glm::vec3(0.0f, 0.0f, -marker_size),
                          view_light + glm::vec3(0.0f, 0.0f, marker_size),
                          LIGHT_POSITION_COLOR);
}

static void draw_vertex_normals() {
  if (!g_mesh_loaded || !g_viewport_transform.valid || !g_show_vertex_normals) {
    return;
  }

  if (g_loaded_mesh.vertex_normals.size() != g_loaded_mesh.vertices.size()) {
    return;
  }

  float normal_length = normal_debug_length();
  if (normal_length <= 0.0f) {
    return;
  }

  glm::mat4 model_view_matrix = build_model_view_matrix(build_model_matrix());
  for (size_t i = 0; i < g_loaded_mesh.vertices.size(); i++) {
    draw_normal_debug_line(g_loaded_mesh.vertices[i], g_loaded_mesh.vertex_normals[i],
                           model_view_matrix, normal_length, VERTEX_NORMAL_COLOR);
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

static void compute_mesh_normals(Mesh &mesh) {
  mesh.face_normals.assign(mesh.faces.size(), glm::vec3(0.0f));
  mesh.vertex_normals.assign(mesh.vertices.size(), glm::vec3(0.0f));

  for (size_t i = 0; i < mesh.faces.size(); i++) {
    const Face &face = mesh.faces[i];
    const glm::vec3 &v0 = mesh.vertices[face.v0];
    const glm::vec3 &v1 = mesh.vertices[face.v1];
    const glm::vec3 &v2 = mesh.vertices[face.v2];

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 face_normal = normalize_or_zero(glm::cross(edge1, edge2));

    mesh.face_normals[i] = face_normal;
    mesh.vertex_normals[face.v0] += face_normal;
    mesh.vertex_normals[face.v1] += face_normal;
    mesh.vertex_normals[face.v2] += face_normal;
  }

  for (glm::vec3 &vertex_normal : mesh.vertex_normals) {
    vertex_normal = normalize_or_zero(vertex_normal);
  }
}

static bool load_obj_file(const char *filename, Mesh &mesh, char *status, int status_size) {
  mesh.vertices.clear();
  mesh.faces.clear();
  mesh.face_normals.clear();
  mesh.vertex_normals.clear();

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

  compute_mesh_normals(mesh);

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

  for (int i = 0; i < MODEL_SEARCH_PATH_COUNT; i++) {
    std::string path = std::string(MODEL_SEARCH_PATHS[i]) + option.name;
    if (load_obj_file(path.c_str(), loaded_mesh, g_mesh_status, sizeof(g_mesh_status))) {
      g_loaded_mesh = loaded_mesh;
      g_mesh_loaded = true;
      g_selected_mesh_index = option_index;
      snprintf(g_mesh_path_used, sizeof(g_mesh_path_used), "%s", path.c_str());
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

        if (c == 's' || c == 'S' || c == 'q' || c == 'Q' ||
            c == 'e' || c == 'E') {
          return; // consume transform shortcut keys
        }

        ui_bridge_char_input(w, c);
      },
      window);

  while (mfb_update_events(window) != MFB_STATE_EXIT) {
    // 1. Input
    ui_bridge_input(ctx, window);

    g_frame_counter++;

    update_line_drawing(ctx);
    update_keyboard_transform_input(window);

    // 2. Scene Rendering
    render_background();
    clear_z_buffer();

    // 3. UI Logic
    static bool quit_requested = false;
    static bool reset_widgets_requested = false;
    static bool reset_canvas_requested = false;
    static bool reset_camera_controls_requested = false;
    static bool reset_lighting_controls_requested = false;
    static bool show_mesh_details_requested = false;

    mu_begin(ctx);

    // Build all UI windows before rendering them into the framebuffer.
    handle_ui_reset_requests(ctx, &reset_widgets_requested,
                             &reset_canvas_requested,
                             &reset_camera_controls_requested,
                             &reset_lighting_controls_requested,
                             &show_mesh_details_requested);
    draw_widgets_window(ctx, &quit_requested);
    draw_camera_controls_window(ctx);
    draw_lighting_controls_window(ctx);
    draw_creative_canvas_window(ctx);
    draw_mesh_details_window(ctx);
    draw_window_controls_window(ctx, &reset_widgets_requested,
                                &reset_canvas_requested,
                                &reset_camera_controls_requested,
                                &reset_lighting_controls_requested,
                                &show_mesh_details_requested);
    cache_all_ui_window_states(ctx);

    mu_end(ctx);

    if (quit_requested) {
      mfb_close(window);
      break;
    }

    // Draw line work after UI logic but before the UI renderer overlays the panels.
    switch (g_render_mode) {
    case RENDER_FILLED_TRIANGLES:
      draw_filled_triangles();
      break;
    case RENDER_DEPTH_MAP:
      draw_filled_triangles();
      draw_depth_map();
      break;
    case RENDER_AMBIENT_LIGHTING:
      draw_ambient_lighting();
      break;
    case RENDER_FLAT_SHADING:
      draw_flat_shading();
      break;
    case RENDER_WIREFRAME:
    default:
      draw_mesh_wireframe();
      break;
    }
    draw_bounding_box();
    draw_coordinate_axes();
    draw_face_normals();
    draw_vertex_normals();
    draw_light_position();
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
