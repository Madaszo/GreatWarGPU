#include "vulkan_utils.hpp"
#include "soldier.hpp"
#include <cstring>

// === BUFFER UTILITIES ===
VulkanBuffer createSSBO(VmaAllocator allocator, vk::Device device, 
                       size_t size, vk::BufferUsageFlags usage) {
    VulkanBuffer buffer;
    
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;
    
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | 
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    
    vmaCreateBuffer(allocator, 
                    reinterpret_cast<const VkBufferCreateInfo*>(&buffer_info),
                    &alloc_info,
                    reinterpret_cast<VkBuffer*>(&buffer.handle),
                    &buffer.allocation,
                    &buffer.alloc_info);
    
    buffer.mapped_ptr = buffer.alloc_info.pMappedData;
    return buffer;
}

void destroyBuffer(VmaAllocator allocator, VulkanBuffer& buffer) {
    if (buffer.handle) {
        vmaDestroyBuffer(allocator, static_cast<VkBuffer>(buffer.handle), buffer.allocation);
        buffer.handle = nullptr;
        buffer.allocation = nullptr;
    }
}

void updateBufferData(const VulkanBuffer& buffer, VmaAllocator allocator, 
                     const void* data, size_t size) {
    if (buffer.mapped_ptr) {
        std::memcpy(buffer.mapped_ptr, data, size);
        vmaFlushAllocation(allocator, buffer.allocation, 0, VK_WHOLE_SIZE);
    }
}

// === INITIALIZATION ===
void initializeBuffers(CrowdSimulationBenchmark& bench) {
    // Soldier SSBO: 1M soldiers
    size_t soldier_buffer_size = bench.max_soldiers * sizeof(Soldier);
    bench.soldier_buffer = createSSBO(
        bench.vma_allocator, 
        bench.device,
        soldier_buffer_size,
        vk::BufferUsageFlagBits::eStorageBuffer | 
        vk::BufferUsageFlagBits::eVertexBuffer
    );
    
    // Initialize all soldiers to zero (will be spawned by compute shader)
    std::memset(bench.soldier_buffer.mapped_ptr, 0, soldier_buffer_size);
    vmaFlushAllocation(bench.vma_allocator, bench.soldier_buffer.allocation, 0, VK_WHOLE_SIZE);
    
    // Atomic Counter SSBO (2x uint32: current_active_soldiers, frame_counter)
    bench.atomic_counter_buffer = createSSBO(
        bench.vma_allocator,
        bench.device,
        sizeof(uint32_t) * 2,
        vk::BufferUsageFlagBits::eStorageBuffer
    );
    
    uint32_t* counter = static_cast<uint32_t*>(bench.atomic_counter_buffer.mapped_ptr);
    counter[0] = 0;  // Start with 0 soldiers
    counter[1] = 0;  // Frame counter
    vmaFlushAllocation(bench.vma_allocator, bench.atomic_counter_buffer.allocation, 0, VK_WHOLE_SIZE);
}

// === DESCRIPTOR UTILITIES ===
void setupComputeDescriptors(CrowdSimulationBenchmark& bench) {
    // Descriptor Set Layout
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};
    
    // Binding 0: Soldier Buffer (Read/Write) - used by compute and vertex shader
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex;
    
    // Binding 1: Atomic Counter Buffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;
    
    // Binding 2: Frame Counter
    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;
    
    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();
    
    bench.compute_descriptor_layout = bench.device.createDescriptorSetLayout(layout_info);
    
    // Descriptor Pool
    std::array<vk::DescriptorPoolSize, 1> pool_sizes{};
    pool_sizes[0].type = vk::DescriptorType::eStorageBuffer;
    pool_sizes[0].descriptorCount = 3;
    
    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 1;
    
    bench.descriptor_pool = bench.device.createDescriptorPool(pool_info);
    
    // Allocate Descriptor Set
    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = bench.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &bench.compute_descriptor_layout;
    
    bench.compute_descriptor_set = bench.device.allocateDescriptorSets(alloc_info)[0];
    
    // Update Descriptor Set
    std::array<vk::WriteDescriptorSet, 3> writes{};
    
    vk::DescriptorBufferInfo soldier_info{
        bench.soldier_buffer.handle, 0, VK_WHOLE_SIZE
    };
    writes[0].dstSet = bench.compute_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[0].pBufferInfo = &soldier_info;
    
    vk::DescriptorBufferInfo counter_info{
        bench.atomic_counter_buffer.handle, 0, VK_WHOLE_SIZE
    };
    writes[1].dstSet = bench.compute_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[1].pBufferInfo = &counter_info;
    
    writes[2].dstSet = bench.compute_descriptor_set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[2].pBufferInfo = &counter_info;
    
    bench.device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), 
                                      writes.data(), 0, nullptr);
}

// === PIPELINE UTILITIES ===
void createComputePipeline(CrowdSimulationBenchmark& bench, 
                          const std::vector<char>& compute_shader_code) {
    // Shader Module
    vk::ShaderModuleCreateInfo shader_info{};
    shader_info.codeSize = compute_shader_code.size();
    shader_info.pCode = reinterpret_cast<const uint32_t*>(compute_shader_code.data());
    
    vk::ShaderModule compute_module = bench.device.createShaderModule(shader_info);
    
    // Pipeline Layout
    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &bench.compute_descriptor_layout;
    
    bench.compute_pipeline_layout = bench.device.createPipelineLayout(layout_info);
    
    // Compute Pipeline
    vk::PipelineShaderStageCreateInfo stage_info{};
    stage_info.stage = vk::ShaderStageFlagBits::eCompute;
    stage_info.module = compute_module;
    stage_info.pName = "main";
    
    vk::ComputePipelineCreateInfo pipeline_info{};
    pipeline_info.stage = stage_info;
    pipeline_info.layout = bench.compute_pipeline_layout;
    
    bench.compute_pipeline = bench.device.createComputePipeline(nullptr, pipeline_info).value;
    
    // Cleanup shader module
    bench.device.destroyShaderModule(compute_module);
}

void createGraphicsPipeline(CrowdSimulationBenchmark& bench,
                           const std::vector<char>& vert_shader_code,
                           const std::vector<char>& frag_shader_code) {
    // Create shader modules
    vk::ShaderModuleCreateInfo vert_module_info{};
    vert_module_info.codeSize = vert_shader_code.size();
    vert_module_info.pCode = reinterpret_cast<const uint32_t*>(vert_shader_code.data());
    vk::ShaderModule vert_module = bench.device.createShaderModule(vert_module_info);
    
    vk::ShaderModuleCreateInfo frag_module_info{};
    frag_module_info.codeSize = frag_shader_code.size();
    frag_module_info.pCode = reinterpret_cast<const uint32_t*>(frag_shader_code.data());
    vk::ShaderModule frag_module = bench.device.createShaderModule(frag_module_info);
    
    // Shader stages
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_module;
    stages[0].pName = "main";
    
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_module;
    stages[1].pName = "main";
    
    // Vertex input: no vertex buffers, using gl_VertexIndex to index into SSBO
    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    
    // Input assembly: point list topology
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.topology = vk::PrimitiveTopology::ePointList;
    input_assembly.primitiveRestartEnable = false;
    
    // Viewport and scissor (will be set dynamically)
    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    // Rasterization
    vk::PipelineRasterizationStateCreateInfo rasterization{};
    rasterization.depthClampEnable = false;
    rasterization.rasterizerDiscardEnable = false;
    rasterization.polygonMode = vk::PolygonMode::eFill;
    rasterization.cullMode = vk::CullModeFlagBits::eNone;
    rasterization.frontFace = vk::FrontFace::eCounterClockwise;
    rasterization.depthBiasEnable = false;
    rasterization.lineWidth = 1.0f;
    
    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = false;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    
    // Color blending
    vk::PipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = 
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = false;
    
    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.logicOpEnable = false;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    
    // Dynamic states
    std::array<vk::DynamicState, 2> dynamic_states{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    
    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();
    
    // Pipeline layout: graphics pipeline needs access to the same descriptor set
    // used by compute (soldier SSBO), so reuse compute_descriptor_layout if present.
    vk::PipelineLayoutCreateInfo layout_info{};
    if (bench.compute_descriptor_layout) {
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &bench.compute_descriptor_layout;
    } else {
        layout_info.setLayoutCount = 0;
    }
    bench.graphics_pipeline_layout = bench.device.createPipelineLayout(layout_info);
    
    // Create graphics pipeline
    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = bench.graphics_pipeline_layout;
    pipeline_info.renderPass = bench.render_pass;
    pipeline_info.subpass = 0;
    
    auto result = bench.device.createGraphicsPipeline(nullptr, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        bench.device.destroyShaderModule(vert_module);
        bench.device.destroyShaderModule(frag_module);
        throw std::runtime_error(std::string("createGraphicsPipeline failed: ") + vk::to_string(result.result));
    }
    bench.graphics_pipeline = result.value;

    // Cleanup shader modules
    bench.device.destroyShaderModule(vert_module);
    bench.device.destroyShaderModule(frag_module);
}

// === RENDER LOOP (Stub - see main.cpp for full implementation) ===
void mainRenderLoop(CrowdSimulationBenchmark& bench) {
    // Placeholder: see main.cpp for actual implementation
    // This will be called from the main loop
}

// === CLEANUP ===
void cleanupVulkan(CrowdSimulationBenchmark& bench) {
    if (bench.device) {
        bench.device.waitIdle();
        
        // Destroy pipelines
        if (bench.compute_pipeline) {
            bench.device.destroyPipeline(bench.compute_pipeline);
        }
        if (bench.graphics_pipeline) {
            bench.device.destroyPipeline(bench.graphics_pipeline);
        }
        
        // Destroy pipeline layouts
        if (bench.compute_pipeline_layout) {
            bench.device.destroyPipelineLayout(bench.compute_pipeline_layout);
        }
        if (bench.graphics_pipeline_layout) {
            bench.device.destroyPipelineLayout(bench.graphics_pipeline_layout);
        }
        
        // Destroy descriptor resources
        if (bench.descriptor_pool) {
            bench.device.destroyDescriptorPool(bench.descriptor_pool);
        }
        if (bench.compute_descriptor_layout) {
            bench.device.destroyDescriptorSetLayout(bench.compute_descriptor_layout);
        }
        
        // Destroy buffers
        destroyBuffer(bench.vma_allocator, bench.soldier_buffer);
        destroyBuffer(bench.vma_allocator, bench.atomic_counter_buffer);
        
        // Destroy render pass
        if (bench.render_pass) {
            bench.device.destroyRenderPass(bench.render_pass);
        }
        
        // Destroy framebuffers and image views
        for (auto& framebuffer : bench.framebuffers) {
            bench.device.destroyFramebuffer(framebuffer);
        }
        for (auto& image_view : bench.swapchain_image_views) {
            bench.device.destroyImageView(image_view);
        }
        
        // Destroy swapchain
        if (bench.swapchain) {
            bench.device.destroySwapchainKHR(bench.swapchain);
        }
        
        // Destroy command pools
        if (bench.graphics_command_pool) {
            bench.device.destroyCommandPool(bench.graphics_command_pool);
        }
        if (bench.compute_command_pool) {
            bench.device.destroyCommandPool(bench.compute_command_pool);
        }
        
        // Destroy synchronization primitives
        for (auto& frame : bench.frames) {
            bench.device.destroySemaphore(frame.image_available_semaphore);
            bench.device.destroySemaphore(frame.render_finished_semaphore);
            bench.device.destroyFence(frame.in_flight_fence);
        }
    }
    
    // Destroy VMA allocator
    if (bench.vma_allocator) {
        vmaDestroyAllocator(bench.vma_allocator);
    }
    
    // Destroy Vulkan device and instance
    if (bench.device) {
        bench.device.destroy();
    }
    if (bench.instance) {
        bench.instance.destroy();
    }
}
