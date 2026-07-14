# Assignment 3 Report: Virtual Cameras and Projections

**Name:** Hala Ishan
**Student ID:** 213692296

---
## Shared Model Assets

In Assignment 2, the OBJ test models were originally placed inside `nanorender/assets/hw2/models/` because they were created for that assignment.
When starting Assignment 3, I noticed that the same models would be reused in later parts and assignments, so I moved the reusable OBJ files to the shared folder `nanorender/assets/models/`.
I also updated the renderer's `MESH_OPTIONS` list to load models from this shared folder.

## Part 1: Coordinate Frames and Bounding Boxes

For this part, I added two visual debugging helpers: coordinate axes and a 3D bounding box. Both can be enabled or disabled from the `Debug drawing` section.

**1- Coordinate Axes:**
I draw one fixed set of world axes from `(0, 0, 0)` and one local set from the mesh center.
The axis colors are red for X, green for Y, and blue for Z.

**2- Bounding Box:**
I use the mesh minimum and maximum values to build the 8 bounding-box corners, then draw the 12 box edges as 3D lines.
I used the pyramid for the visual test because its shape does not already match a box. With the regular cube, the bounding box overlaps the cube almost exactly, so the bounding-box helper is harder to see.

### The implementation can be seen in the UI and in the rendered scene:
The first image shows the `Debug drawing` checkboxes used to show or hide the coordinate axes and bounding box. The second image shows the result on the pyramid model, where the colored axes and yellow bounding box are visible. Together, the images show that both helpers can be controlled from the UI and displayed on the model.

<img src="../assets/hw3/images/hw3_part1_debug_controls.png" alt="Debug drawing checkboxes" width="443">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<img src="../assets/hw3/images/hw3_part1_debug_visuals.png" alt="Coordinate axes and bounding box debug view" width="500">

### Transform Tests

After enabling both debug helpers, I tested them while transforming the pyramid. I used one local rotation test and one world translation test because they show the two main ideas from the assignment: the local helpers stay attached to the model, while the world axes stay fixed as a reference.

<table>
  <tr>
    <th>Local Rotation Test</th>
    <th>World Translation Test</th>
  </tr>
  <tr>
    <td><img src="../assets/hw3/gifs/hw3_part1_local_rotation_z_then_y.gif" alt="Local rotation with coordinate axes and bounding box" width="500"></td>
    <td><img src="../assets/hw3/gifs/hw3_part1_world_translation_x_then_y.gif" alt="World translation with coordinate axes and bounding box" width="500"></td>
  </tr>
  <tr>
    <td>
      I used <code>Local &gt; Rotation</code> and rotated the pyramid. The model, its local axes, and the yellow bounding box rotate together around the model. This happens because the local axes and bounding box use the same model matrix as the mesh.
    </td>
    <td>
      I used <code>World &gt; Translation</code> and moved the pyramid through the scene. The model, local axes, and bounding box move together, while the world axes remain fixed at <code>(0, 0, 0)</code>. This shows the world frame acting as a stable reference.
    </td>
  </tr>
</table>

---
## Part 2: The Virtual Camera (View Matrix)

### UI Preparation for Camera Work

Before starting the View Matrix implementation, I prepared the interface for camera testing:

- I reduced the default screen fitting size for loaded models to about half of the previous size. In earlier tests, I often had to manually lower the uniform scale before recording examples so there was enough room to see the model movement clearly. With this change, the model appears smaller by default, leaving more visible space for the upcoming View Matrix tests, while the original OBJ vertex data stays unchanged.
- I kept the `Creative Canvas` hidden by default because it is not needed for the camera parts, but it can still be opened from `Window Controls`.
- I added a separate `Camera Controls` panel on the right side of the window for the camera position and rotation sliders.
- I changed the default model to `simple_pyramid.obj` for this part because its shape makes the camera movement and rotation easier to see while testing.

### View Matrix Implementation

For Part 2, I added a `Camera Controls` panel with X, Y, and Z sliders for camera position and rotation, plus reset buttons for each axis and for the full camera.

<img src="../assets/hw3/images/hw3_part2_camera_controls_gui.png" alt="Camera Controls panel" width="300">

I build the camera matrix from translation and rotation, then use its inverse as the View Matrix:

`Camera Matrix = Translation * Rotation`<br>
`View Matrix = inverse(Camera Matrix)`

During rendering, the original OBJ vertices stay unchanged and are transformed temporarily each frame:

`vertex -> Model Matrix -> View Matrix -> viewport transform -> orthographic projection`

Because the View Matrix is the inverse camera transform, moving or rotating the camera makes the scene appear to move or rotate in the opposite direction.

### Camera Movement Tests

To isolate the camera effect, I started both tests from the normal model position: one test changes camera position, and the other changes camera rotation.

<table>
  <tr>
    <th>Camera Position X/Y</th>
    <th>Camera Rotation X/Y</th>
  </tr>
  <tr>
    <td><img src="../assets/hw3/gifs/hw3_part2_camera_position_xy.gif" alt="Camera position X and Y movement" width="500"></td>
    <td><img src="../assets/hw3/gifs/hw3_part2_camera_rotation_xy.gif" alt="Camera rotation X and Y movement" width="500"></td>
  </tr>
  <tr>
    <td>
      I moved the camera left on X, so the object appeared to move right. Then I moved the camera up on Y, so the object appeared to move down. This confirms the inverse movement caused by the View Matrix.
    </td>
    <td>
      I rotated the camera with a negative X rotation and a positive Y rotation. The mesh, axes, and bounding box changed view together because they all pass through the same View Matrix before projection.
    </td>
  </tr>
</table>

---
## Part 3: Perspective Projection

### UI Preparation for Projection Work

For this part, I added a projection mode toggle inside the Camera Controls panel. The renderer can now switch between `Orthographic` and `Perspective` mode.

I kept Orthographic mode as the previous renderer behavior. Perspective mode has extra controls because it needs projection parameters:

- `FOV / View zoom`
- `Near`
- `Far`
- `Reset Projection`

I used these controls for the Perspective tests. The FOV changes the view zoom, while Near and Far define the visible depth range of the perspective camera.

<img src="../assets/hw3/images/hw3_part3_projection_controls.png" alt="Perspective projection controls" width="300">

### Perspective Projection Implementation

In Orthographic mode, the model keeps the same behavior from the previous parts: depth does not make the model bigger or smaller.

In Perspective mode, I added a Projection Matrix after the Model Matrix and View Matrix. I used GLM's `glm::perspective` with the window aspect ratio `WIDTH / HEIGHT`. After multiplying by the projection matrix, I divide by `w` to get normalized device coordinates, then convert those coordinates to screen coordinates.

The vertex pipeline becomes:

`vertex -> Model Matrix -> View Matrix -> Projection Matrix -> perspective divide -> screen`

This is the main difference between the two modes. Orthographic mode keeps the old direct screen mapping, while Perspective mode uses depth. Because of that, objects look bigger when they are closer to the camera and smaller when they are farther away.

### Projection Tests

I used the same pyramid model for the tests so the projection changes are easy to see. The first test switches between the two projection modes. The other tests stay in Perspective mode and check the projection controls one by one.

#### Orthographic and Perspective Toggle

<img src="../assets/hw3/gifs/hw3_part3_projection_toggle.gif" alt="Toggling between Orthographic and Perspective projection" width="600">

I switched between Orthographic and Perspective mode with the same model and camera setup. Since only the projection mode changes, the GIF gives a direct comparison. Orthographic keeps the pyramid size almost constant, while Perspective changes the view based on depth.

#### Perspective Depth and FOV

<table>
  <tr>
    <th>Camera Position Z</th>
    <th>Field of View</th>
  </tr>
  <tr>
    <td><img src="../assets/hw3/gifs/hw3_part3_camera_position_z.gif" alt="Changing camera position on Z in perspective mode" width="500"></td>
    <td><img src="../assets/hw3/gifs/hw3_part3_fov_change.gif" alt="Changing the field of view in perspective mode" width="500"></td>
  </tr>
  <tr>
    <td>
      I moved the camera on the Z axis while staying in Perspective mode. When the camera gets closer to the pyramid, the pyramid looks bigger. When the camera moves farther away, the pyramid looks smaller. This shows the depth effect of the perspective projection.
    </td>
    <td>
      I changed only the FOV value. A smaller FOV makes the view more zoomed in, so the pyramid looks bigger. A larger FOV makes the view wider and more zoomed out, so the pyramid looks smaller. In this test, the camera and model stay in the same place; only the projection setting changes.
    </td>
  </tr>
</table>

#### View Matrix Check and Clipping Range

<table>
  <tr>
    <th>Camera Rotation Y</th>
    <th>Near and Far Planes</th>
  </tr>
  <tr>
    <td><img src="../assets/hw3/gifs/hw3_part3_camera_rotation_y.gif" alt="Rotating the camera around Y in perspective mode" width="500"></td>
    <td><img src="../assets/hw3/gifs/hw3_part3_near_far_clipping.gif" alt="Near and far clipping plane test in perspective mode" width="500"></td>
  </tr>
  <tr>
    <td>
      Part 2 already tested the View Matrix, so I used this only as a small extra check. I rotated the camera around Y and verified that the model is still projected correctly in Perspective mode. This shows that the old View Matrix logic still works together with the new Projection Matrix.
    </td>
    <td>
      I changed the Near and Far plane values to test the visible depth range. These values do not zoom the pyramid. They decide what depth range the perspective camera can see. When part of the model is outside this range, it gets clipped and is not drawn.
    </td>
  </tr>
</table>

### Part 3 Summary

This part adds Perspective projection without removing the old Orthographic renderer. The tests show that the projection toggle works, the perspective matrix uses depth, the FOV changes the zoomed-in or zoomed-out feeling, and the Near/Far values control the visible depth range. The extra camera rotation test also shows that the existing View Matrix still works correctly before the new Projection Matrix is applied.
