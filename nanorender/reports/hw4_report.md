# Assignment 4 Report

**Name:** Hala Ishan
**Student ID:** 213692296

---
## Part 1: Bounding Box Rasterization

For this part, I added a debug rasterization mode for triangle bounding boxes. Instead of drawing the three wireframe edges of each triangle, the renderer projects the triangle to screen space, finds the smallest 2D rectangle that contains its three screen points, and fills that rectangle directly in `g_buffer`.

To implement this, I used the same projection path from the renderer. For each face, I projected its three vertices to screen coordinates, then calculated `min x`, `max x`, `min y`, and `max y`. These values define the 2D bounding rectangle. After that, I looped over the pixels inside the rectangle and wrote the triangle color directly to `g_buffer`.

Each triangle bounding box gets a different solid color. This is why the result looks like overlapping colored rectangles instead of filled triangles. This is expected for Part 1 because the goal is only to test the bounding box step before doing the inside-triangle test in the next part.

I added a UI toggle called `Show triangle boxes` so this debug view can be turned on and off without removing the old wireframe renderer.

<img src="../assets/hw4/images/hw4_part1_triangle_boxes_toggle.png" alt="Show triangle boxes UI toggle" width="220">

### Bounding Box Rasterization Results

The examples below show the debug view on the three different models. In each example, the model is drawn as overlapping colored rectangles. This shows that the renderer is calculating a screen-space bounding box for each projected triangle and drawing it to the framebuffer.

<table>
  <tr>
    <th>Pyramid</th>
    <th>Regular Cube</th>
    <th>Angled Box</th>
  </tr>
  <tr>
    <td><img src="../assets/hw4/gifs/hw4_part1_pyramid.gif" alt="Triangle bounding boxes on the pyramid model" width="360"></td>
    <td><img src="../assets/hw4/gifs/hw4_part1_regular_cube.gif" alt="Triangle bounding boxes on the regular cube model" width="360"></td>
    <td><img src="../assets/hw4/gifs/hw4_part1_angled_box.gif" alt="Triangle bounding boxes on the angled box model" width="360"></td>
  </tr>
  <tr>
    <td>
      The pyramid shows large overlapping boxes because each triangle face has its own screen rectangle.
    </td>
    <td>
      The cube shows that the boxes still follow the projected faces when the model rotates.
    </td>
    <td>
      The angled box makes the different projected triangle rectangles easy to see.
    </td>
  </tr>
</table>

### Toggle Comparison

I also recorded the regular cube rotating around the Y axis from `-180` to `180`. The first GIF shows the normal wireframe view with the debug mode off. The second GIF shows the same idea with `Show triangle boxes` enabled, where the cube is represented by the colored bounding rectangles instead of triangle edges.

<table>
  <tr>
    <th>Debug View Off</th>
    <th>Debug View On</th>
  </tr>
  <tr>
    <td><img src="../assets/hw4/gifs/hw4_part1_cube_rotation_without_boxes.gif" alt="Regular cube rotation without triangle bounding boxes" width="500"></td>
    <td><img src="../assets/hw4/gifs/hw4_part1_cube_rotation_with_boxes.gif" alt="Regular cube rotation with triangle bounding boxes" width="500"></td>
  </tr>
  <tr>
    <td>
      With the debug view off, the cube keeps the old wireframe rendering behavior.
    </td>
    <td>
      With the debug view on, each projected triangle is shown as its filled 2D bounding rectangle.
    </td>
  </tr>
</table>