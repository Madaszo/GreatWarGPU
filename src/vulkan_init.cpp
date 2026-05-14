#include "vulkan_types.hpp"
#include <vulkan/vulkan.hpp>

// VMA implementation - must be defined in exactly one translation unit
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <algorithm>

// Debug callback for Vulkan validation
static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                      void* pUserData) {
    std::cerr << "Vulkan Validation: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// === HELPER FUNCTIONS ===

// Find queue family index supporting both graphics and compute
uint32_t findQueueFamilyIndex(vk::PhysicalDevice physical_device, 
                              vk::QueueFlagBits flags) {
    auto queue_families = physical_device.getQueueFamilyProperties();
    
    for (uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & flags) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find queue family with required flags");
}

// === VULKAN INITIALIZATION ===
void initializeVulkan(CrowdSimulationBenchmark& bench) {
    // ===== INSTANCE CREATION =====
    vk::ApplicationInfo app_info{};
    app_info.pApplicationName = "GreatWar GPU";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    // Get GLFW required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    std::vector<const char*> required_extensions(
        glfwExtensions, glfwExtensions + glfwExtensionCount
    );

    // Enable validation layers and debug utils for detailed error messages
    std::vector<const char*> validation_layers;
    const bool enableValidation = false; // disabled to avoid extension loader/link issues
    if (enableValidation) {
        validation_layers.push_back("VK_LAYER_KHRONOS_validation");
        required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    vk::InstanceCreateInfo instance_info{};
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    instance_info.ppEnabledExtensionNames = required_extensions.data();
    
    if (enableValidation) {
        instance_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        instance_info.ppEnabledLayerNames = validation_layers.data();
    }
    
    bench.instance = vk::createInstance(instance_info);
    std::cout << "✓ Vulkan instance created" << std::endl;

    // Setup debug messenger (if validation enabled)
    if (enableValidation) {
        vk::DebugUtilsMessengerCreateInfoEXT dbg_info{};
        dbg_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        dbg_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        dbg_info.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(vkDebugCallback);

        bench.debug_messenger = bench.instance.createDebugUtilsMessengerEXT(dbg_info);
    }
    
    // ===== PHYSICAL DEVICE SELECTION =====
    auto physical_devices = bench.instance.enumeratePhysicalDevices();
    if (physical_devices.empty()) {
        throw std::runtime_error("No GPU with Vulkan support found");
    }
    
    // Select first device with compute and graphics support
    for (const auto& device : physical_devices) {
        auto props = device.getProperties();
        auto features = device.getFeatures();
        
        std::cout << "Found GPU: " << props.deviceName.data() << std::endl;
        
        // Check for compute and graphics queue
        try {
            uint32_t gfx_queue = findQueueFamilyIndex(device, vk::QueueFlagBits::eGraphics);
            uint32_t compute_queue = findQueueFamilyIndex(device, vk::QueueFlagBits::eCompute);
            
            bench.physical_device = device;
            bench.graphics_queue_family = gfx_queue;
            bench.compute_queue_family = compute_queue;
            
            std::cout << "✓ Selected GPU with graphics queue=" << gfx_queue 
                     << ", compute queue=" << compute_queue << std::endl;
            break;
        } catch (...) {
            // Try next device
        }
    }
    
    if (!bench.physical_device) {
        throw std::runtime_error("No suitable GPU found");
    }
    
    // ===== LOGICAL DEVICE CREATION =====
    std::vector<vk::DeviceQueueCreateInfo> queue_infos;
    std::vector<uint32_t> unique_queues{bench.graphics_queue_family};
    
    // Add compute queue if different
    if (bench.compute_queue_family != bench.graphics_queue_family) {
        unique_queues.push_back(bench.compute_queue_family);
    }
    
    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queues) {
        vk::DeviceQueueCreateInfo queue_info{};
        queue_info.queueFamilyIndex = queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(queue_info);
    }
    
    // Enable required features
    vk::PhysicalDeviceFeatures device_features{};
    device_features.fillModeNonSolid = true;  // For debugging
    device_features.wideLines = true;         // For debugging
    
    // Get required device extensions (Swapchain)
    std::vector<const char*> device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    vk::DeviceCreateInfo device_info{};
    device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.pEnabledFeatures = &device_features;
    
    bench.device = bench.physical_device.createDevice(device_info);
    std::cout << "✓ Logical device created" << std::endl;
    
    // Get queue handles
    bench.graphics_queue = bench.device.getQueue(bench.graphics_queue_family, 0);
    bench.compute_queue = bench.device.getQueue(bench.compute_queue_family, 0);
    
    // ===== VMA ALLOCATOR INITIALIZATION =====
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
    allocator_info.physicalDevice = static_cast<VkPhysicalDevice>(bench.physical_device);
    allocator_info.device = static_cast<VkDevice>(bench.device);
    allocator_info.instance = static_cast<VkInstance>(bench.instance);
    
    vmaCreateAllocator(&allocator_info, &bench.vma_allocator);
    std::cout << "✓ VMA allocator initialized" << std::endl;
    
    // ===== COMMAND POOLS =====
    vk::CommandPoolCreateInfo graphics_pool_info{};
    graphics_pool_info.queueFamilyIndex = bench.graphics_queue_family;
    graphics_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    bench.graphics_command_pool = bench.device.createCommandPool(graphics_pool_info);
    
    vk::CommandPoolCreateInfo compute_pool_info{};
    compute_pool_info.queueFamilyIndex = bench.compute_queue_family;
    compute_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    bench.compute_command_pool = bench.device.createCommandPool(compute_pool_info);
    
    std::cout << "✓ Command pools created" << std::endl;
    
    // ===== SWAPCHAIN SETUP =====
    // Create a GLFW surface for the window
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(bench.instance), bench.window, nullptr, &raw_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface via GLFW");
    }
    bench.surface = vk::SurfaceKHR(raw_surface);

    // Query surface capabilities
    auto surface_caps = bench.physical_device.getSurfaceCapabilitiesKHR(bench.surface);
    auto surface_formats = bench.physical_device.getSurfaceFormatsKHR(bench.surface);
    auto present_modes = bench.physical_device.getSurfacePresentModesKHR(bench.surface);

    // Choose surface format (prefer SRGB)
    if (surface_formats.size() == 1 && surface_formats[0].format == vk::Format::eUndefined) {
        bench.swapchain_format = vk::Format::eB8G8R8A8Srgb;
    } else {
        bool found = false;
        for (const auto& fmt : surface_formats) {
            if (fmt.format == vk::Format::eB8G8R8A8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                bench.swapchain_format = fmt.format;
                found = true;
                break;
            }
        }
        if (!found) bench.swapchain_format = surface_formats[0].format;
    }

    // Choose present mode (prefer MAILBOX if available)
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    for (const auto& pm : present_modes) {
        if (pm == vk::PresentModeKHR::eMailbox) {
            present_mode = pm;
            break;
        }
    }

    // Determine swap extent (use window size if allowed)
    if (surface_caps.currentExtent.width != UINT32_MAX) {
        bench.swapchain_extent = surface_caps.currentExtent;
    } else {
        bench.swapchain_extent.width = std::max(surface_caps.minImageExtent.width, std::min(surface_caps.maxImageExtent.width, bench.window_width));
        bench.swapchain_extent.height = std::max(surface_caps.minImageExtent.height, std::min(surface_caps.maxImageExtent.height, bench.window_height));
    }

    // Choose number of images
    uint32_t image_count = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount)
        image_count = surface_caps.maxImageCount;

    // Create swapchain
    vk::SwapchainCreateInfoKHR sc_info{};
    sc_info.surface = bench.surface;
    sc_info.minImageCount = image_count;
    sc_info.imageFormat = bench.swapchain_format;
    sc_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    sc_info.imageExtent = bench.swapchain_extent;
    sc_info.imageArrayLayers = 1;
    sc_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    uint32_t queue_family_indices[] = { bench.graphics_queue_family, bench.compute_queue_family };
    if (bench.graphics_queue_family != bench.compute_queue_family) {
        sc_info.imageSharingMode = vk::SharingMode::eConcurrent;
        sc_info.queueFamilyIndexCount = 2;
        sc_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        sc_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    sc_info.preTransform = surface_caps.currentTransform;
    sc_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    sc_info.presentMode = present_mode;
    sc_info.clipped = VK_TRUE;

    bench.swapchain = bench.device.createSwapchainKHR(sc_info);
    bench.swapchain_images = bench.device.getSwapchainImagesKHR(bench.swapchain);

    // Create image views
    bench.swapchain_image_views.clear();
    for (const auto& img : bench.swapchain_images) {
        vk::ImageViewCreateInfo iv_info{};
        iv_info.image = img;
        iv_info.viewType = vk::ImageViewType::e2D;
        iv_info.format = bench.swapchain_format;
        iv_info.components = vk::ComponentMapping();
        iv_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        iv_info.subresourceRange.baseMipLevel = 0;
        iv_info.subresourceRange.levelCount = 1;
        iv_info.subresourceRange.baseArrayLayer = 0;
        iv_info.subresourceRange.layerCount = 1;

        bench.swapchain_image_views.push_back(bench.device.createImageView(iv_info));
    }

    std::cout << "✓ Swapchain and image views created" << std::endl;
    
    // ===== SYNCHRONIZATION PRIMITIVES =====
    vk::SemaphoreCreateInfo semaphore_info{};
    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;  // Start signaled
    
    vk::CommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.commandPool = bench.graphics_command_pool;
    cmd_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    cmd_alloc_info.commandBufferCount = 2;
    
    auto cmd_buffers = bench.device.allocateCommandBuffers(cmd_alloc_info);
    
    for (uint32_t i = 0; i < 2; ++i) {
        bench.frames[i].command_buffer = cmd_buffers[i];
        bench.frames[i].image_available_semaphore = bench.device.createSemaphore(semaphore_info);
        bench.frames[i].render_finished_semaphore = bench.device.createSemaphore(semaphore_info);
        bench.frames[i].in_flight_fence = bench.device.createFence(fence_info);
    }
    
    std::cout << "✓ Frame synchronization objects created" << std::endl;
    
    // ===== RENDER PASS SETUP =====
    vk::AttachmentDescription color_attachment{};
    color_attachment.format = bench.swapchain_format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlags();
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo rp_info{};
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &color_attachment;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dependency;

    bench.render_pass = bench.device.createRenderPass(rp_info);

    // ===== FRAMEBUFFERS SETUP =====
    bench.framebuffers.clear();
    for (const auto& view : bench.swapchain_image_views) {
        vk::ImageView attachments[] = { view };
        vk::FramebufferCreateInfo fb_info{};
        fb_info.renderPass = bench.render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = attachments;
        fb_info.width = bench.swapchain_extent.width;
        fb_info.height = bench.swapchain_extent.height;
        fb_info.layers = 1;

        bench.framebuffers.push_back(bench.device.createFramebuffer(fb_info));
    }

    std::cout << "✓ Render pass and framebuffers created" << std::endl;
    
    std::cout << "\n✓ Vulkan initialization complete (with stubs)" << std::endl;
}
