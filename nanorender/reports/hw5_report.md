# Assignment 5 Report

**Name:** Hala Ishan
**Student ID:** 213692296

---

In this assignment, I started improving the solid models by adding lighting. Until now, the objects could be filled and depth-tested, but they still looked flat because the colors did not react to light.

Here, I start using light, material properties, and normals so the model can react to lighting. Part 1 begins with ambient lighting.

---
## Part 1: Light Sources and Material Properties

In Part 1, I set up the light and material data, then used only ambient lighting to color the object.

### Light and Material Data

The task asked for a point light and material properties, so I added two structs: `PointLight` and `Material`.

`PointLight` stores the light position and the ambient, diffuse, and specular light colors.

```cpp
struct PointLight {
  glm::vec3 position;
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
};
```

The `Material` stores the object's matching RGB properties, so the object has its own color response instead of using the light color directly.

```cpp
struct Material {
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
};
```

In Part 1, only the `ambient` values affect the rendered image. The `diffuse` and `specular` values are stored for the next lighting parts.

For the actual color in Part 1, I multiply the two ambient colors:

```text
ambient color = light ambient * material ambient
```

This means the final flat color depends on both the light and the material: the light ambient color controls the color and strength of the surrounding light, and the material ambient color controls the base color that the object reflects.

---

### Light and Material UI

I added a UI panel called `Light and Material` for the Part 1 controls.

<table>
  <tr>
    <td><img src="../assets/hw5/images/hw5_part1_light_material_ui.png" alt="Light and material controls" width="350"></td>
    <td>
      The default setup uses white ambient light and a blue material.<br>
      This makes the result easy to read because the white light keeps the material color clear.
      <br><br>
      The panel includes light position, light ambient color, and material ambient color controls. <br>
      Each color has RGB sliders, and each group has its own reset button.
      <br><br>
      The light position is shown because the assignment requires it, but it does not affect Part 1 yet because ambient lighting does not use light direction.
    </td>
  </tr>
</table>

---

### Ambient Display Toggle

<table>
  <tr>
    <td><img src="../assets/hw5/images/hw5_part1_debug_ambient_toggle.png" alt="Show ambient lighting toggle" width="300"></td>
    <td>
      I added `Show ambient lighting` to display the ambient result. <br><br>
      This mode still uses the filled triangle rasterizer and Z-buffer from Assignment 4, but replaces the random face colors with the ambient lighting color.<br>
      Since only ambient lighting is active, the object changes color without showing bright or dark sides.
    </td>
  </tr>
</table>

---

### Examples

I used two examples to check the material side and the light side separately.

#### Material Ambient Example

In this example, I tested the material side of the ambient equation. The ambient light stayed white, so the changes mostly show how the material color affects the object.

<img src="../assets/hw5/gifs/hw5_part1_material_ambient.gif" alt="Changing the material ambient color" width="500">

<table>
  <tr>
    <th>Action</th>
    <th>What happened</th>
    <th>Why it happened</th>
  </tr>
  <tr>
    <td>Started from the default material</td>
    <td>The object appeared blue.</td>
    <td>The white ambient light kept the blue material color clear.</td>
  </tr>
  <tr>
    <td>Increased the first RGB slider (red)</td>
    <td>The object became less pure blue and moved toward a more purple color.</td>
    <td>Adding more red to the material means the object reflects more red under the same white light.</td>
  </tr>
  <tr>
    <td>Reset material ambient</td>
    <td>The object returned to the default blue color.</td>
    <td>The values returned to the default material color.</td>
  </tr>
  <tr>
    <td>Decreased the second RGB slider (green)</td>
    <td>The object became a deeper blue.</td>
    <td>Reducing green removes part of the cyan/bright component, so the final color becomes darker and more blue-dominant.</td>
  </tr>
</table>

#### Light Ambient Example

In this example, I kept the material color fixed and changed the light ambient color. This checks the light side of the equation.

<img src="../assets/hw5/gifs/hw5_part1_light_ambient.gif" alt="Changing the light ambient color" width="500">

<table>
  <tr>
    <th>Action</th>
    <th>What happened</th>
    <th>Why it happened</th>
  </tr>
  <tr>
    <td>Started with the material color from the previous example</td>
    <td>The object kept the darker blue material result.</td>
    <td>The material side stayed fixed, so the next changes come from the light side.</td>
  </tr>
  <tr>
    <td>Decreased the first RGB slider (red)</td>
    <td>The red contribution became weaker.</td>
    <td>The light ambient color multiplies the material color, so reducing red light reduces the red part of the final color.</td>
  </tr>
  <tr>
    <td>Decreased the second RGB slider (green)</td>
    <td>The object became darker overall.</td>
    <td>Now both red and green light channels are weaker, so fewer color components remain strong in the final result.</td>
  </tr>
  <tr>
    <td>Reset light ambient</td>
    <td>The object returned to the clearer default result.</td>
    <td>The light returned to white.</td>
  </tr>
  <tr>
    <td>Decreased the third RGB slider (blue)</td>
    <td>The blue became weaker, and the object shifted toward a dark lilac color.</td>
    <td>Since the material was blue-heavy, reducing the blue light channel strongly changed the final color.</td>
  </tr>
</table>


**Together, these examples show both sides of the ambient equation. The material controls what color the object reflects, and the light controls how strongly those color channels appear. The model stays flat because diffuse and specular lighting are not implemented yet.**

---

## Render Mode Organization

After Part 1, I organized the main drawing options into one `Render mode` selector.

Before this change, the renderer already made sure that only one main render view could be active at a time, which was correct. But after adding more views, the separate options became less clear. The selector keeps the same behavior in a cleaner way because I choose one main result at a time: `Wireframe`, `Filled triangles`, `Depth map`, or `Ambient lighting`.

The debug drawings, such as axes, bounding box, and normals, stayed as checkboxes because they are overlays that can appear on top of the selected render mode.

The image and GIF show the new selector and how it switches between the render modes one at a time.

<img src="../assets/hw5/images/hw5_render_mode_selector.png" alt="Render mode selector" width="450">&nbsp;&nbsp;&nbsp;<img src="../assets/hw5/gifs/hw5_render_mode_selector.gif" alt="Changing render modes" width="580">




---

## Part 2: Flat Shading (Diffuse Lighting)

In Part 2, I added diffuse lighting using flat shading. The renderer now has a `Flat shading` render mode that still uses the filled triangle rasterizer and Z-buffer from Assignment 4.

For each triangle, I calculate the lighting once using the face normal and triangle center. The same color is then used for all pixels inside that triangle, so the model looks faceted.

```text
diffuse strength = max(dot(face normal, light direction), 0)
final color = ambient + diffuse
```

The diffuse part is calculated from the light diffuse color, the material diffuse color, and the diffuse strength:

```text
diffuse = light diffuse * material diffuse * diffuse strength
```

<img src="../assets/hw5/gifs/hw5_part2_flat_shading_mode.gif" alt="Switching to flat shading mode and rotating the model" width="520">

The GIF shows the model in `Flat shading` mode. When the model rotates, the faces keep a flat color per triangle, but different faces have different brightness depending on their normals and the light direction.

---

### Light Position Helper

The light is stored as a point in world space. For each triangle, I calculate the light direction from the triangle center toward the light position:

```text
light direction = light position - triangle center
```

To make the light position easier to see, I added a debug checkbox called `Show light position`, so I can turn the helper on only when I need it. It draws a small yellow marker at the point light position and a line from the light toward the model center. This helper is only for visualization; the diffuse calculation still uses each triangle center.

<img src="../assets/hw5/images/hw5_part2_light_position_checkbox.png" alt="Show light position checkbox" width="260">&nbsp;&nbsp;&nbsp;<img src="../assets/hw5/images/hw5_part2_light_position_values.png" alt="Light position values" width="300">

The screenshot shows the light position controls. In the example, I moved the Y value toward the positive side, while X and Z stayed at their default values. This makes the light come more from one vertical side of the model, so it is easier to see the lighting direction change.

<img src="../assets/hw5/gifs/hw5_part2_light_position.gif" alt="Moving the light position while showing the light helper" width="520">

In this example, I moved the light position on the Y axis. Faces facing the light become brighter, while other faces become darker. This shows that the diffuse value depends on the angle between the face normal and the light direction. I did not add distance attenuation yet, so the important change here is direction, not distance.

---

### Light Diffuse Color

The `Light diffuse` controls the color and strength of the diffuse light. Its default value is white, `(1, 1, 1)`, meaning full red, green, and blue intensity. This keeps the material color clear before testing the sliders.

<img src="../assets/hw5/images/hw5_part2_light_diffuse_values.png" alt="Light diffuse values" width="300">
<img src="../assets/hw5/gifs/hw5_part2_light_diffuse.gif" alt="Changing the light diffuse color" width="520">

In this example, I first tried changing the blue diffuse value, but the change was hard to see because the ambient blue color was still strong. To make the diffuse effect clearer, I lowered the blue ambient value, then changed the blue diffuse value again. After that, the lit faces changed more clearly. This shows that `Light diffuse` is working, but a strong ambient color can hide small diffuse changes. Resetting `Light diffuse` returns it to the default white diffuse light.

---

### Material Diffuse Color

The `Material diffuse` controls what diffuse color the object reflects. Its default matches the blue material color from Part 1, `(0.25, 0.55, 0.90)`. Since the blue value is high and the red value is lower, changes in the material channels are easy to see.

<img src="../assets/hw5/images/hw5_part2_material_diffuse_values.png" alt="Material diffuse values" width="300">
<img src="../assets/hw5/gifs/hw5_part2_material_diffuse.gif" alt="Changing the material diffuse color" width="520">

In this example, I changed the material diffuse channels. Lowering the blue value made the diffuse part less blue, and changing the red value changed how much red the material could reflect in the lit faces. Resetting `Material diffuse` returns the material to the default blue diffuse color.

These examples show that Part 2 adds direction-based diffuse lighting. Ambient lighting gives the base color, while diffuse lighting changes the face brightness using the light direction and the light/material diffuse colors. Specular lighting is not implemented yet.
