# Assignment 2 Report: Wireframe Viewer and Geometric Transformations

**Name:** Hala Ishan  
**Student ID:** 213692296

---

## Part 0: Introduction to GLM

In this part, I integrated GLM into the project and verified that it works correctly.

GLM is needed for this assignment because the next parts require 3D vectors, matrices, and transformations such as translation, rotation, and scaling.

I updated the project configuration so that CMake fetches GLM and links it with the application.  
Then I included GLM in `main.cpp` and added a small test to check that GLM vectors and transformation matrices work inside the running project.

For the test, I used a 3D point with the values `(1.0, 2.0, 3.0)` and applied a translation of `+10.0` on the X axis.

The expected result was:

`(11.0, 2.0, 3.0)`

This is correct because only the X coordinate changed, while the Y and Z coordinates stayed the same.

I displayed the result inside the GUI to make the test visible while the program is running.

![Part 0 GLM test](../assets/hw2/images/hw2_part0_glm_test.png)

After adding GLM, the project compiled and ran successfully, and the correct transformation result appeared in the GUI.

### Development Note

While testing Part 0, I also checked the mouse input alignment after changing the window size and UI layout.

The old Y-axis offsets that were used before were no longer needed, so I set them back to `0`.

After this change, the mouse position matched the UI controls correctly.

During this check, I also noticed a few possible UI improvements from the previous assignment, such as improving window behavior and making some controls more context-aware.

I decided to keep Part 0 focused on the GLM setup and return to these UI improvements later before continuing with the next parts.