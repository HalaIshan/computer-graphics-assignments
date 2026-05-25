# Assignment 1 Report: Basic Graphics and Immediate Mode GUI

**Name:** Hala Ishan  
**Student ID:** 213692296  

---

## Part 1: Manipulating the Framebuffer

### What was required

Modify the background rendering loop in `main.cpp` by writing new mathematical expressions for `r`, `g`, and `b`.  
The result must be a visible 2D pattern that uses both `x` and `y`, not a solid color and not a simple horizontal or vertical strip.

### What I did

I replaced the original gradient background with a concentric circles pattern.  
For each pixel, I used its position relative to the center of the window:

```cpp
int center_x = WIDTH / 2;
int center_y = HEIGHT / 2;

int dx = x - center_x;
int dy = y - center_y;

int distance_squared = dx * dx + dy * dy;
```

Then I used `distance_squared` to calculate the RGB values:

```cpp
uint8_t r = (uint8_t)((distance_squared / 900) % 256);
uint8_t g = (uint8_t)((distance_squared / 500) % 256);
uint8_t b = (uint8_t)((distance_squared / 300) % 256);

g_buffer[i] = MFB_RGB(r, g, b);
```

Because `distance_squared` depends on both `x` and `y`, the background becomes a 2D circular pattern centered in the window.

### Result

<table>
  <tr>
    <td align="center"><b>Before</b></td>
    <td align="center"><b>After</b></td>
  </tr>
  <tr>
    <td><img src="../assets/original_image.png" width="420"></td>
    <td><img src="../assets/concentric_circles_pattern_img.png" width="420"></td>
  </tr>
</table>

---

## Part 2: Immediate Mode UI Declaration

In this part, I added a new interactive MicroUI widget inside the `Widgets` window.

I added a checkbox that controls an external state variable:

```cpp
static int show_part2_label = 0;
```

Inside the UI window, the checkbox updates this variable:

```cpp
mu_layout_row(ctx, 1, w1, 0);
mu_checkbox(ctx, "Show Part 2 label", &show_part2_label);
```

When the checkbox is enabled, a custom label appears in the window:

```cpp
if (show_part2_label) {
  mu_Color old_text_color = ctx->style->colors[MU_COLOR_TEXT];
  ctx->style->colors[MU_COLOR_TEXT] = mu_color(255, 220, 80, 255);

  mu_layout_row(ctx, 1, w1, 0);
  mu_label(ctx, "Part 2 label is now visible");

  mu_layout_row(ctx, 1, w1, 0);
  mu_label(ctx, "mission accomplished!");

  ctx->style->colors[MU_COLOR_TEXT] = old_text_color;
}
```

This demonstrates Immediate Mode UI because the widget is declared every frame, while its state is stored in an external application variable.

**Note:** During testing, I noticed that the clickable area of some widgets did not perfectly match their visual position on the screen. The widget still works, but the click position is slightly offset. I kept this observation for the later renderer/input architecture part.

### Result

<table>
  <tr>
    <td align="center"><b>Checkbox Off</b></td>
    <td align="center"><b>Checkbox On</b></td>
  </tr>
  <tr>
    <td><img src="../assets/part2_img1.png" width="420"></td>
    <td><img src="../assets/part2_img2.png" width="420"></td>
  </tr>
</table>