#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <array>
#include <vector>

// === VULKAN WRAPPER TYPES ===
struct VulkanBuffer {
    vk::Buffer handle;
    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;
    void* mapped_ptr = nullptr;
};

struct VulkanImage {
    vk::Image handle;
    vk::ImageView view;
    VmaAllocation allocation;
};

// === FRAME CONTEXT ===
struct FrameContext {
    vk::CommandBuffer command_buffer;
    vk::Semaphore image_available_semaphore;
    vk::Semaphore render_finished_semaphore;
    vk::Fence in_flight_fence;
};

// === RENDER STATE ===
struct CrowdSimulationBenchmark {
    // Window
    struct GLFWwindow* window = nullptr;
    uint32_t window_width = 1600;
    uint32_t window_height = 900;
    
    // Vulkan Core
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue graphics_queue;
    vk::Queue compute_queue;
    uint32_t graphics_queue_family = 0;
    uint32_t compute_queue_family = 0;
    
    // Memory Allocator
    VmaAllocator vma_allocator = nullptr;
    
    // Buffers
    VulkanBuffer soldier_buffer;           // Main SSBO: 1,000,000 soldiers
    VulkanBuffer atomic_counter_buffer;    // Current active soldiers count + frame counter
    
    // Pipelines
    vk::PipelineLayout compute_pipeline_layout;
    vk::Pipeline compute_pipeline;
    vk::PipelineLayout graphics_pipeline_layout;
    vk::Pipeline graphics_pipeline;
    
    // Descriptors
    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout compute_descriptor_layout;
    vk::DescriptorSet compute_descriptor_set;
    
    // Render Pass & Swapchain
    vk::RenderPass render_pass;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    vk::DebugUtilsMessengerEXT debug_messenger;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_image_views;
    std::vector<vk::Framebuffer> framebuffers;
    vk::Format swapchain_format;
    vk::Extent2D swapchain_extent;
    
    // Command Pools & Frames
    vk::CommandPool graphics_command_pool;
    vk::CommandPool compute_command_pool;
    std::array<FrameContext, 2> frames;
    uint32_t current_frame = 0;
    
    // Simulation State
    uint32_t max_soldiers = 1000000;        // 1 million max
    uint32_t current_active_soldiers = 0;   // Start at 0
    float spawn_rate = 10000.0f;            // 10k/sec (gradual buildup)
    bool running = true;
};
