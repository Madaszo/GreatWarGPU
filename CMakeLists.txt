cmake_minimum_required(VERSION 3.16)
project(GreatWarGPU)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer the local vcpkg packages if they are installed on this machine.
if(EXISTS "C:/vcpkg/installed/x64-windows/share")
    list(APPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows/share")
endif()

# Find Vulkan
find_package(Vulkan REQUIRED COMPONENTS glslc glslangValidator)

# Find GLFW
find_package(glfw3 REQUIRED CONFIG)

# Find GLM
find_package(glm REQUIRED CONFIG)

# VMA (Vulkan Memory Allocator) - header only
find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    PATHS 
        ${CMAKE_SOURCE_DIR}/third_party/VulkanMemoryAllocator
        ${CMAKE_SOURCE_DIR}/third_party
        ${VULKAN_INCLUDE_DIR}
        $ENV{VULKAN_SDK}/Include
        /usr/include 
        /usr/local/include 
)

if(NOT VMA_INCLUDE_DIR)
    message(STATUS "VMA not found. Checking Vulkan SDK...")
    if(DEFINED ENV{VULKAN_SDK})
        message(STATUS "Using Vulkan SDK at: $ENV{VULKAN_SDK}")
        set(VMA_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
        # Verify it actually has VMA header
        if(NOT EXISTS "$ENV{VULKAN_SDK}/Include/vk_mem_alloc.h")
            message(WARNING "VMA header not found in Vulkan SDK either.")
            message(FATAL_ERROR "
    ===== MISSING: Vulkan Memory Allocator (VMA) =====
    VMA header file is required but not found.
    
    SOLUTIONS:
    1. [EASIEST] Run download script from project root:
       Windows: download_vma.bat
       Linux/Mac: bash download_vma.sh
       
    2. [MANUAL] Download from GitHub:
       https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases
       Extract vk_mem_alloc.h to:
       ${CMAKE_SOURCE_DIR}/third_party/VulkanMemoryAllocator/
       
    3. [COPY FROM SDK] If using Vulkan SDK:
       Copy <VULKAN_SDK>/Include/vk_mem_alloc.h to:
       ${CMAKE_SOURCE_DIR}/third_party/VulkanMemoryAllocator/
    ")
        endif()
    else()
        message(FATAL_ERROR "
    ===== ERROR: VMA Not Found =====
    VULKAN_SDK environment variable is not set.
    Please install Vulkan SDK or run download_vma.bat
        ")
    endif()
endif()

# Legacy source files
set(LEGACY_SOURCES
    src/main.cpp
    src/vulkan_utils.cpp
    src/vulkan_init.cpp
)

set(LEGACY_HEADERS
    include/soldier.hpp
    include/vulkan_types.hpp
    include/vulkan_utils.hpp
)

set(ARCHITECTURE_HEADERS
    include/greatwargpu/command/CommandHierarchy.hpp
    include/greatwargpu/gpu/GPUContext.hpp
    include/greatwargpu/render/Renderer.hpp
    include/greatwargpu/simulation/Simulation.hpp
)

set(ARCHITECTURE_SOURCES
    src/command/CommandHierarchy.cpp
    src/gpu/GPUContext.cpp
    src/render/Renderer.cpp
    src/simulation/Simulation.cpp
)

add_library(GreatWarGPUCore STATIC
    ${ARCHITECTURE_SOURCES}
    ${ARCHITECTURE_HEADERS}
)

# New GreatWar architecture (attachment layout)
set(GREATWAR_HEADERS
    include/greatwar/simulation/Simulation.hpp
    include/greatwar/simulation/Grid.hpp
    include/greatwar/simulation/WorldState.hpp
    include/greatwar/simulation/Tick.hpp

    include/greatwar/commands/CommandHierarchy.hpp
    include/greatwar/commands/Command.hpp
    include/greatwar/commands/CommandBuffer.hpp

    include/greatwar/gpu/GPUContext.hpp
    include/greatwar/gpu/TacticalExecutor.hpp
    include/greatwar/gpu/BufferSet.hpp

    include/greatwar/render/Renderer.hpp
    include/greatwar/render/Camera.hpp
    include/greatwar/render/RenderSnapshot.hpp

    include/greatwar/app/Application.hpp
)

set(GREATWAR_SOURCES
    src/simulation/Simulation.cpp
    src/simulation/Grid.cpp
    src/simulation/WorldState.cpp

    src/commands/CommandHierarchy.cpp
    src/commands/CommandBuffer.cpp

    src/gpu/GPUContext.cpp
    src/gpu/TacticalExecutor.cpp

    src/render/Renderer.cpp
    src/render/Camera.cpp

    src/app/Application.cpp
)

add_library(GreatWarCore STATIC
    ${GREATWAR_SOURCES}
    ${GREATWAR_HEADERS}
)

target_include_directories(GreatWarCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(GreatWarCore PUBLIC cxx_std_20)

target_include_directories(GreatWarGPUCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(GreatWarGPUCore PUBLIC cxx_std_20)

# Create executable
add_executable(GreatWarGPU ${LEGACY_SOURCES} ${LEGACY_HEADERS})

# Include directories
target_include_directories(GreatWarGPU PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIR}
    ${VMA_INCLUDE_DIR}
    C:/vcpkg/installed/x64-windows/include
)

# Link libraries
target_link_libraries(GreatWarGPU PRIVATE
    GreatWarGPUCore
    GreatWarCore
    Vulkan::Vulkan
    glfw
    glm::glm)

# Copy shader files to build directory
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
    COMMENT "Copying shader files to build directory"
)

add_dependencies(GreatWarGPU copy_shaders)

# Optional: Add shader compilation target
# Requires glslc to be installed
find_program(GLSLC glslc)

if(GLSLC)
    message(STATUS "glslc found at: ${GLSLC}")
    
    file(GLOB GLSL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/shaders/*.comp"
        "${CMAKE_SOURCE_DIR}/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    )
    
    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
        
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            COMMENT "Compiling shader: ${FILE_NAME}"
        )
        
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)
    
    add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(GreatWarGPU compile_shaders)
else()
    message(WARNING "glslc not found. Shaders will not be compiled automatically.")
    message(WARNING "Please install it using: sudo apt-get install glslang-tools (Linux) or download from Khronos")
endif()

# Print build information
message(STATUS "GreatWarGPU Build Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Vulkan: ${Vulkan_FOUND}")
message(STATUS "  GLFW: ${glfw3_FOUND}")
message(STATUS "  GLM: ${glm_FOUND}")
message(STATUS "  VMA Include Dir: ${VMA_INCLUDE_DIR}")
