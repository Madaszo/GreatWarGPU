#include "vulkan_types.hpp"
#include "vulkan_utils.hpp"
#include "soldier.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

// Forward declarations (implemented in vulkan_init.cpp)
void initializeVulkan(CrowdSimulationBenchmark& bench);
std::vector<char> readShaderFile(const std::string& filename);
void cleanupVulkan(CrowdSimulationBenchmark& bench);

// === SHADER FILE LOADING ===
std::vector<char> readShaderFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    
    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);
    
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
    
    return buffer;
}

// === RENDER LOOP IMPLEMENTATION ===
void executeRenderLoop(CrowdSimulationBenchmark& bench) {
    auto frame_start_time = std::chrono::high_resolution_clock::now();
    uint64_t frame_count = 0;
    
    while (bench.running && !glfwWindowShouldClose(bench.window)) {
        glfwPollEvents();
        
        FrameContext& frame = bench.frames[bench.current_frame];
        
        // Wait for frame to be ready
        bench.device.waitForFences(frame.in_flight_fence, true, UINT64_MAX);
        bench.device.resetFences(frame.in_flight_fence);
        
        // === UPDATE SOLDIER COUNT ===
        float delta_time = 0.016f; // Assume 60 FPS
        uint32_t spawn_count = static_cast<uint32_t>(bench.spawn_rate * delta_time);
        
        uint32_t* counter = static_cast<uint32_t*>(bench.atomic_counter_buffer.mapped_ptr);
        counter[0] = std::min(bench.current_active_soldiers + spawn_count, bench.max_soldiers);
        counter[1]++;
        vmaFlushAllocation(bench.vma_allocator, bench.atomic_counter_buffer.allocation, 0, VK_WHOLE_SIZE);
        bench.current_active_soldiers = counter[0];
        
        // === RECORD COMMAND BUFFER ===
        frame.command_buffer.reset();
        
        vk::CommandBufferBeginInfo begin_info{};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        frame.command_buffer.begin(begin_info);
        
        // --- COMPUTE DISPATCH ---
        frame.command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, bench.compute_pipeline);
        frame.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                bench.compute_pipeline_layout,
                                                0, bench.compute_descriptor_set, nullptr);
        
        uint32_t workgroup_count = (bench.current_active_soldiers + 255) / 256;
        if (workgroup_count > 0) {
            frame.command_buffer.dispatch(workgroup_count, 1, 1);
        }
        
        // --- MEMORY BARRIER ---
        // Wait for compute writes to complete before reading in graphics
        vk::MemoryBarrier memory_barrier{};
        memory_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | 
                                       vk::AccessFlagBits::eVertexAttributeRead;
        
        frame.command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexInput | vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags{},
            memory_barrier, nullptr, nullptr);
        
        // --- GRAPHICS RENDERING ---
        vk::RenderPassBeginInfo render_pass_info{};
        render_pass_info.renderPass = bench.render_pass;
        
        // Acquire next swapchain image
        uint32_t image_index;
        try {
            auto result = bench.device.acquireNextImageKHR(bench.swapchain, UINT64_MAX, 
                                                           frame.image_available_semaphore);
            image_index = result.value;
        } catch (const vk::OutOfDateKHRError&) {
            // Swapchain out of date, need to recreate (not implemented in this stub)
            bench.running = false;
            break;
        }
        
        render_pass_info.framebuffer = bench.framebuffers[image_index];
        render_pass_info.renderArea.offset = vk::Offset2D{0, 0};
        render_pass_info.renderArea.extent = bench.swapchain_extent;
        
        vk::ClearValue clear_color{{0.0f, 0.0f, 0.0f, 1.0f}};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_color;
        
        frame.command_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
        
        frame.command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, bench.graphics_pipeline);
        
        // Bind descriptor sets so graphics pipeline can read from soldier SSBO
        frame.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                bench.graphics_pipeline_layout,
                                                0, bench.compute_descriptor_set, nullptr);
        
        vk::Viewport viewport{0.0f, 0.0f, 
                             static_cast<float>(bench.swapchain_extent.width),
                             static_cast<float>(bench.swapchain_extent.height),
                             0.0f, 1.0f};
        frame.command_buffer.setViewport(0, viewport);
        
        vk::Rect2D scissor{{0, 0}, bench.swapchain_extent};
        frame.command_buffer.setScissor(0, scissor);
        
        // Draw soldiers as point primitives
        if (bench.current_active_soldiers > 0) {
            frame.command_buffer.draw(bench.current_active_soldiers, 1, 0, 0);
        }
        
        frame.command_buffer.endRenderPass();
        frame.command_buffer.end();
        
        // === SUBMIT & PRESENT ===
        vk::SubmitInfo submit_info{};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &frame.command_buffer;
        
        vk::PipelineStageFlags wait_stages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &frame.image_available_semaphore;
        submit_info.pWaitDstStageMask = &wait_stages;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame.render_finished_semaphore;
        
        bench.graphics_queue.submit(submit_info, frame.in_flight_fence);
        
        // Present to swapchain
        vk::PresentInfoKHR present_info{};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &frame.render_finished_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &bench.swapchain;
        present_info.pImageIndices = &image_index;
        
        try {
            bench.graphics_queue.presentKHR(present_info);
        } catch (const vk::OutOfDateKHRError&) {
            // Swapchain out of date, need to recreate
            bench.running = false;
            break;
        }
        
        bench.current_frame = (bench.current_frame + 1) % 2;
        frame_count++;
        
        // Print stats every 60 frames
        if (frame_count % 60 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - frame_start_time).count();
            if (elapsed > 0) {
                double fps = frame_count / (double)elapsed;
                std::cout << "Frame: " << frame_count 
                         << " | FPS: " << fps 
                         << " | Active Soldiers: " << bench.current_active_soldiers << std::endl;
            }
        }
    }
    
    bench.device.waitIdle();
}

// === MAIN ===
int main(int argc, char* argv[]) {
    try {
        CrowdSimulationBenchmark bench;
        
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return 1;
        }
        
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        bench.window = glfwCreateWindow(bench.window_width, bench.window_height, 
                                       "GreatWar GPU - Crowd Simulation Benchmark", 
                                       nullptr, nullptr);
        
        if (!bench.window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return 1;
        }
        
        std::cout << "Initializing Vulkan..." << std::endl;
        initializeVulkan(bench);
        
        std::cout << "Initializing buffers..." << std::endl;
        initializeBuffers(bench);
        
        std::cout << "Setting up compute descriptors..." << std::endl;
        setupComputeDescriptors(bench);
        
        // Load SPIR-V shaders (compile GLSL to SPV if necessary)
        std::cout << "Loading shaders..." << std::endl;
        const std::string comp_spv = "shaders/crowd.comp.spv";
        const std::string vert_spv = "shaders/crowd.vert.spv";
        const std::string frag_spv = "shaders/crowd.frag.spv";

        auto file_exists = [](const std::string& p) {
            std::ifstream f(p, std::ios::binary);
            return f.is_open();
        };

        if (!file_exists(comp_spv) || !file_exists(vert_spv) || !file_exists(frag_spv)) {
            std::cout << "SPIR-V shader(s) missing, attempting to compile with glslc..." << std::endl;
            const char* vulkan_sdk = std::getenv("VULKAN_SDK");
            std::string glslc_cmd = "glslc"; // fallback to PATH
            if (vulkan_sdk) {
                std::string candidate = std::string(vulkan_sdk) + "\\Bin\\glslc.exe";
                // prefer SDK glslc if present
                std::ifstream test(candidate);
                if (test.good()) glslc_cmd = "\"" + candidate + "\"";
            }

            auto run = [&](const std::string& src, const std::string& out) {
                std::string cmd = glslc_cmd + " " + src + " -o " + out;
                std::cout << "Running: " << cmd << std::endl;
                int rc = std::system(cmd.c_str());
                return rc == 0;
            };

            bool ok = true;
            ok &= run("shaders/crowd.comp", comp_spv);
            ok &= run("shaders/crowd.vert", vert_spv);
            ok &= run("shaders/crowd.frag", frag_spv);

            if (!ok) {
                std::cerr << "Failed to compile shaders. Ensure glslc is installed or set VULKAN_SDK." << std::endl;
                bench.running = false;
            }
        }

        if (bench.running) {
            auto compute_code = readShaderFile(comp_spv);
            auto vert_code = readShaderFile(vert_spv);
            auto frag_code = readShaderFile(frag_spv);

            // Create pipelines
            createComputePipeline(bench, compute_code);
            createGraphicsPipeline(bench, vert_code, frag_code);
        }
        
        std::cout << "Starting render loop..." << std::endl;
        executeRenderLoop(bench);
        
        std::cout << "Cleaning up..." << std::endl;
        cleanupVulkan(bench);
        
        glfwDestroyWindow(bench.window);
        glfwTerminate();
        
        std::cout << "Shutdown complete." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        glfwTerminate();
        return 1;
    }
}
