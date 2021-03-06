# Setting up Development Environment

You can use [cmake](https://cmake.org/download/) (3.13 or later) to build the development environment for your platform. You will also need the [Vulkan SDK](https://vulkan.lunarg.com), [git](https://git-scm.com/downloads), and [python 3](https://www.python.org/downloads/) installed.

To compile the project you will run `cmake -D CMAKE_BUILD_TYPE=Release -H. -Bbuild` followed by `cmake --build build`. The binary will be in the `build` directory, along with a `resources` folder. You can customize the blocks, renderers, terrain generation, etc. by modifying the resources in those folders. Shaders should be in their glsl forms with an appropriate file extension (`.frag`, `.vert`, etc.) and will be compiled automatically when you run the program. 

If you're debugging a custom renderer, it is recommended to use [RenderDoc](https://renderdoc.org) to help visualize what's going on.

# Adding content

All of the program's content is dynamically loaded through lua files inside the `resources` folder next to the vecs binary. Documentation on these files will be coming at a later time, but for now you can use the default resources files as a reference. 
