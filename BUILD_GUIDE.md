# GreatWar GPU - Build & Implementation Guide

## Project Structure Created

```
GreatWarGPU/
├── include/
│   ├── soldier.hpp              ✓ Soldier data structure (std430 aligned)
│   ├── vulkan_types.hpp         ✓ Vulkan core types & state management
│   └── vulkan_utils.hpp         ✓ Vulkan utility function declarations
├── src/
│   ├── main.cpp                 ✓ Entry point with render loop (frame sync, dispatch, rendering)
│   ├── vulkan_utils.cpp         ✓ SSBO creation, descriptors, pipelines, cleanup
│   └── vulkan_init.cpp          ⚠ Vulkan initialization (partial - swapchain/renderpass stubbed)
├── shaders/
│   ├── crowd.comp               ✓ Compute shader (GLSL 4.60 - goals, collision, physics)
│   ├── crowd.vert               ✓ Vertex shader (reads from SSBO via gl_VertexIndex)
│   └── crowd.frag               ✓ Fragment shader (outputs team color)
├── CMakeLists.txt               ✓ Build configuration with shader compilation
├── README.md                    ✓ Full documentation
├── BUILD_GUIDE.md               ← You are here
└── .gitignore                   ✓ Standard ignores for C++ projects
```

## What's Implemented ✓

### 1. **Data Structures** (`include/soldier.hpp`, `include/vulkan_types.hpp`)
- `Soldier` struct: vec2 position, vec2 velocity, int team, int _pad (24 bytes, std430 aligned)
- `CrowdSimulationBenchmark`: Main state container with all Vulkan objects
- `VulkanBuffer`: Wrapper for VMA-allocated buffers with mapping support
- `FrameContext`: Command buffer + synchronization primitives (double-buffered)

### 2. **GLSL Shaders** (`shaders/`)
- **Compute Shader** (`crowd.comp`):
  - 256-thread workgroups
  - Spawning logic (Red @ -0.95, Blue @ +0.95)
  - Goal seeking towards enemy capital
  - Collision avoidance (100 nearest neighbors check)
  - Physics: velocity clamping, damping, boundary constraints
  - Output: Updated soldier positions/velocities

- **Vertex Shader** (`crowd.vert`):
  - Reads soldiers directly from SSBO using `gl_VertexIndex`
  - Sets `gl_Position` (NDC coordinates)
  - Outputs team color (Red or Blue)

- **Fragment Shader** (`crowd.frag`):
  - Simple color pass-through
  - 2x2 point rendering

### 3. **Vulkan Buffer Management** (`src/vulkan_utils.cpp`)
- `createSSBO()`: VMA-allocated buffer creation with host-coherent mapping
- `destroyBuffer()`: Proper cleanup
- `updateBufferData()`: CPU → GPU memory transfers with flush
- Soldier SSBO: 1M soldiers (24MB)
- Atomic Counter Buffer: 8 bytes (soldier count + frame counter)

### 4. **Descriptor Setup** (`src/vulkan_utils.cpp`)
- Descriptor set layout with 3 bindings:
  - Binding 0: Soldier SSBO (compute read/write, vertex read)
  - Binding 1: Atomic counter (compute read/write)
  - Binding 2: Frame counter (compute read/write)
- Descriptor pool + set allocation & update

### 5. **Pipeline Creation** (`src/vulkan_utils.cpp`)
- **Compute Pipeline**:
  - Module creation from SPIR-V binary
  - Layout with descriptor set
  - Full pipeline object

- **Graphics Pipeline**:
  - Vertex + Fragment shader modules
  - Point list topology (VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
  - No vertex buffer binding (uses gl_VertexIndex)
  - Dynamic viewport/scissor
  - Simple color blending (no blending)

### 6. **Main Render Loop** (`src/main.cpp`)
- **Frame synchronization**:
  - Wait on fence from previous frame
  - Double-buffering with 2 frame contexts
  
- **CPU-side updates**:
  - Increment soldier count based on spawn_rate
  - Update frame counter

- **Compute dispatch**:
  - Bind compute pipeline & descriptors
  - Calculate workgroup count: (active_soldiers + 255) / 256
  - Dispatch compute shader

- **Memory barrier**:
  - ShaderWrite → VertexAttributeRead | ShaderRead
  - Critical synchronization between compute and graphics

- **Graphics rendering**:
  - Acquire swapchain image
  - Begin render pass
  - Bind graphics pipeline
  - Set dynamic viewport/scissor
  - Draw soldiers: `draw(active_soldier_count, 1, 0, 0)`

- **Presentation**:
  - Submit to graphics queue
  - Present to swapchain
  - Frame statistics

### 7. **Vulkan Initialization** (`src/vulkan_init.cpp`)
- Instance creation with GLFW extensions
- Physical device selection (graphics + compute queues)
- Logical device creation
- VMA allocator setup
- Command pools for graphics & compute
- Synchronization primitives (semaphores, fences)
- ⚠ **STUBS** for swapchain and render pass (see below)

## What Needs Implementation ⚠

### 1. **Swapchain Creation** (`src/vulkan_init.cpp`)
**Location**: `initializeVulkan()` around line 150

Requires:
```cpp
// 1. Create VkSurfaceKHR from GLFW window
VkSurfaceKHR surface = /* createSurfaceKHR from bench.window */

// 2. Query capabilities
auto capabilities = bench.physical_device.getSurfaceCapabilitiesKHR(surface);

// 3. Choose format
auto formats = bench.physical_device.getSurfaceFormatsKHR(surface);
auto chosen_format = formats[0];  // Use preferred format

// 4. Choose present mode
auto modes = bench.physical_device.getSurfacePresentModesKHR(surface);
auto present_mode = vk::PresentModeKHR::eFifo;  // Most compatible

// 5. Determine extent
vk::Extent2D extent{
    std::clamp(bench.window_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
    std::clamp(bench.window_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
};

// 6. Create swapchain
vk::SwapchainCreateInfoKHR swapchain_info{};
swapchain_info.surface = surface;
swapchain_info.minImageCount = 2;
swapchain_info.imageFormat = chosen_format.format;
swapchain_info.imageColorSpace = chosen_format.colorSpace;
swapchain_info.imageExtent = extent;
// ... set other fields

bench.swapchain = bench.device.createSwapchainKHR(swapchain_info);
bench.swapchain_images = bench.device.getSwapchainImagesKHR(bench.swapchain);
```

**Also create image views:**
```cpp
for (auto& image : bench.swapchain_images) {
    vk::ImageViewCreateInfo view_info{};
    view_info.image = image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = chosen_format.format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    // ... other fields
    
    bench.swapchain_image_views.push_back(bench.device.createImageView(view_info));
}
```

### 2. **Render Pass Creation** (`src/vulkan_init.cpp`)
**Location**: After swapchain, around line 160

Requires:
```cpp
// 1. Color attachment description
vk::AttachmentDescription color_attachment{};
color_attachment.format = swapchain_format;
color_attachment.samples = vk::SampleCountFlagBits::e1;
color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
color_attachment.initialLayout = vk::ImageLayout::eUndefined;
color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

// 2. Attachment reference
vk::AttachmentReference color_ref{};
color_ref.attachment = 0;
color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

// 3. Subpass description
vk::AttachmentDescription attachments[] = {color_attachment};
vk::SubpassDescription subpass{};
subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
subpass.colorAttachmentCount = 1;
subpass.pColorAttachments = &color_ref;

// 4. Subpass dependency
vk::SubpassDependency dependency{};
dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
dependency.dstSubpass = 0;
dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
dependency.srcAccessMask = vk::AccessFlagBits{};
dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

// 5. Create render pass
vk::RenderPassCreateInfo render_pass_info{};
render_pass_info.attachmentCount = 1;
render_pass_info.pAttachments = attachments;
render_pass_info.subpassCount = 1;
render_pass_info.pSubpasses = &subpass;
render_pass_info.dependencyCount = 1;
render_pass_info.pDependencies = &dependency;

bench.render_pass = bench.device.createRenderPass(render_pass_info);
```

### 3. **Framebuffer Creation** (`src/vulkan_init.cpp`)
**Location**: After render pass

Requires:
```cpp
for (auto& image_view : bench.swapchain_image_views) {
    vk::FramebufferCreateInfo fb_info{};
    fb_info.renderPass = bench.render_pass;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &image_view;
    fb_info.width = bench.swapchain_extent.width;
    fb_info.height = bench.swapchain_extent.height;
    fb_info.layers = 1;
    
    bench.framebuffers.push_back(bench.device.createFramebuffer(fb_info));
}
```

### 4. **Shader Compilation**
**Action**: Build the project, then compile shaders manually:

```bash
mkdir build && cd build
cmake ..

# After successful CMake, compile shaders:
glslc ../shaders/crowd.comp -o shaders/crowd.comp.spv
glslc ../shaders/crowd.vert -o shaders/crowd.vert.spv
glslc ../shaders/crowd.frag -o shaders/crowd.frag.spv

cmake --build . --config Release
```

Or use CMake's shader compilation target (if glslc is found):
```bash
cmake .. -DCOMPILE_SHADERS=ON
cmake --build . --config Release
```

### 5. **Load & Create Pipelines in main.cpp**
**Location**: `main.cpp` around line 185

Replace TODO comment with:
```cpp
// Load pre-compiled SPIR-V shaders
auto compute_code = readShaderFile("shaders/crowd.comp.spv");
auto vert_code = readShaderFile("shaders/crowd.vert.spv");
auto frag_code = readShaderFile("shaders/crowd.frag.spv");

// Create pipelines
createComputePipeline(bench, compute_code);
createGraphicsPipeline(bench, vert_code, frag_code);
```

## Build Instructions

### Step 1: Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install vulkan-sdk glslang-tools libglfw3-dev libglm-dev
```

**Windows (vcpkg):**
```bash
vcpkg install glfw3:x64-windows glm:x64-windows
```

**macOS:**
```bash
brew install vulkan-sdk glfw glm
```

### Step 2: Configure Build

```bash
cd ~/Documents/GreatWarGPU
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Step 3: Compile Shaders

```bash
# Compile GLSL to SPIR-V
glslc ../shaders/crowd.comp -o shaders/crowd.comp.spv
glslc ../shaders/crowd.vert -o shaders/crowd.vert.spv
glslc ../shaders/crowd.frag -o shaders/crowd.frag.spv
```

### Step 4: Build C++

```bash
cmake --build . --config Release
```

### Step 5: Run

```bash
./GreatWarGPU  # or GreatWarGPU.exe on Windows
```

## Debugging Tips

### Validation Layers
Enable Vulkan validation layer for debugging:
```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./GreatWarGPU
```

### Shader Debugging
Use `glslangValidator` to check shader syntax:
```bash
glslangValidator -V shaders/crowd.comp
```

### GPU Profiling
Use native GPU profilers:
- **NVIDIA**: NVIDIA Nsight Systems or Nsight Graphics
- **AMD**: Radeon Graphics Profiler (RGP)
- **Intel**: Intel Graphics Metrics Discoverer

## Performance Optimization Checklist

- [ ] Replace naive collision check with spatial hash grid
- [ ] Implement hierarchical LOD for large crowds
- [ ] Add GPU-driven indirect dispatch
- [ ] Profile memory bandwidth usage
- [ ] Optimize barrier placement (avoid redundant barriers)
- [ ] Consider wave occupancy in compute shader

## Next Steps

1. **Complete Vulkan initialization** (swapchain + render pass)
2. **Compile shaders to SPIR-V**
3. **Build and test the project**
4. **Profile performance**
5. **Implement spatial hashing for >100K soldiers**
6. **Add real-time statistics UI (optional)**

## Common Issues

| Issue | Solution |
|-------|----------|
| "vk_mem_alloc.h not found" | Set `VMA_INCLUDE_DIR` in CMake or install from GitHub |
| Shaders fail to compile | Update Vulkan SDK and glslc |
| Black screen | Check render pass format matches swapchain format |
| GPU not detected | Run `vulkaninfo` to verify Vulkan support |
| Low FPS at startup | Increase spawn_rate gradually, check GPU usage |

---

**Last Updated**: May 2026
**Status**: ~80% complete (core logic done, platform-specific rendering needed)
