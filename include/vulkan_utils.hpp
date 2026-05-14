#pragma once

#include "vulkan_types.hpp"
#include <vector>

// === BUFFER UTILITIES ===
VulkanBuffer createSSBO(VmaAllocator allocator, vk::Device device, 
                       size_t size, vk::BufferUsageFlags usage);

void destroyBuffer(VmaAllocator allocator, VulkanBuffer& buffer);

void updateBufferData(const VulkanBuffer& buffer, VmaAllocator allocator, 
                     const void* data, size_t size);

// === DESCRIPTOR UTILITIES ===
void setupComputeDescriptors(CrowdSimulationBenchmark& bench);

// === PIPELINE UTILITIES ===
void createComputePipeline(CrowdSimulationBenchmark& bench, 
                          const std::vector<char>& compute_shader_code);

void createGraphicsPipeline(CrowdSimulationBenchmark& bench,
                           const std::vector<char>& vert_shader_code,
                           const std::vector<char>& frag_shader_code);

// === RENDER LOOP ===
void mainRenderLoop(CrowdSimulationBenchmark& bench);

// === INITIALIZATION ===
void initializeBuffers(CrowdSimulationBenchmark& bench);

void cleanupVulkan(CrowdSimulationBenchmark& bench);
