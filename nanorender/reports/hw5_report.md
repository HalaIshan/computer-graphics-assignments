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
    <td>Reset the material ambient values</td>
    <td>The object returned to the default blue material color.</td>
    <td>The reset button restored the original material ambient color.</td>
  </tr>
  <tr>
    <td>Decreased the second RGB slider (green)</td>
    <td>The object became a deeper blue.</td>
    <td>Reducing green removes part of the cyan/bright component, so the final color becomes darker and more blue-dominant.</td>
  </tr>
</table>

#### Light Ambient Example

In this example, I kept the material color from the previous example and changed the light ambient color. This checks the light side of the equation.

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
    <td>Reset the light ambient values</td>
    <td>The object became clearer again.</td>
    <td>The light returned to white, so it stopped weakening the material color channels.</td>
  </tr>
  <tr>
    <td>Decreased the third RGB slider (blue)</td>
    <td>The blue became weaker, and the object shifted toward a dark lilac color.</td>
    <td>Since the material was blue-heavy, reducing the blue light channel strongly changed the final color.</td>
  </tr>
</table>


**Together, these examples show the ambient equation from both sides. Changing the material changes what color the object reflects, while changing the light changes how strongly each material color channel appears. The model stays flat because diffuse and specular lighting are not implemented yet.**