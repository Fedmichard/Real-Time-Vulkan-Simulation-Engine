# Vulkan Simulation Engine

An onging C++17 Vulkan renderer.

## Features
 *  Free Roam Camera System – Move and explore the scene freely
 *  Draw Sorting – Efficient batching of opaque and transparent objects
 *  Frustum Culling – Skip rendering objects outside the camera’s view
 *  Anti-Aliasing (MSAA) – Up to 8× for smoother edges on my personal device
 *  Texture Mipmapping – Better texture detail at varying distances
 *  Custom Texture System – Manage texture assets with custom loaders
 *  Mesh, GLTF, and GLB Loading – Import 3D models directly
 *  Custom Compute Pipelines – Procedural background and experiments
 *  Custom Graphics Pipelines – Opaque and transparent rendering paths
 *  GPU Resource Helpers – Creation of buffers, uniform buffers, images, textures, etc
 *  ImGui Integration – Real-time debug overlay and tools
 *  RenderDoc Debugging Support – Frame capture and GPU analysis
 *  Bindless rendering – Early work towards descriptor indexing and large-scale material/texture binding

## Screenshots
<img width="1698" height="898" alt="image" src="https://github.com/user-attachments/assets/020423ae-d9a4-4e38-98a2-419eada35446" />

![any alternative text you want]([./gifname.gif](https://media0.giphy.com/media/v1.Y2lkPTc5MGI3NjExMDF1eGYxdngyaXg2azc2bHZydmZldGg4Zm5sbzR1ZzJ5YnRweTJ5MSZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/BXsjD4ECRMFyKIgzW2/giphy.gif))


## Roadmap
- [ ] GPU Driven Rendering
- [ ] GPU Instancing
- [ ] PBR Lighting and Shadings
- [ ] Deferred Shading
- [ ] Occlusion Culling
- [ ] Anti-aliasing
    - [x] MSAA
    - [ ] TAA
    - [ ] SMAA
    - [ ] FXAA
    - [ ] SSAA
- [ ] UI Display
    - [ ] Scene Changer
    - [ ] Scene Graph View
    - [ ] Dynamically Import Textures and Objects Into a Scene
    - [ ] Debug Visualization
    - [ ] Depth Buffer View
    - [ ] Normal Map View
    - [ ] UV Coordinate View
    - [x] Scene Statistics
    - [ ] Anti-aliasing Selection
- [ ] GPU-Based Simulations
- [ ] Engine Architecture Refactoring
     
## Installation and Compiling
* CMake VERSION 3.26
* Vulkan SDK (with volk)
* C++17 Compiler
* Delete .EXE in Build Folder
* Include glfw3.dll in Build Folder (libraries/GLFW/bin)

## Additional Documentation and Acknowledgments
* VkGuide
* RenderDoc
* Vulkan-Tutorial
* Learnopengl
* Vkdoc
