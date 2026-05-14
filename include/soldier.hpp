#pragma once

#include <glm/glm.hpp>
#include <cstdint>

// === SOLDIER DATA STRUCTURE ===
// Must match GLSL std430 layout
// std430 alignment: vec2 = 8 bytes, int = 4 bytes
// Struct total: 8 + 8 + 4 + 4 = 24 bytes
struct alignas(8) Soldier {
    glm::vec2 position;    // offset 0, 8 bytes
    glm::vec2 velocity;    // offset 8, 8 bytes
    int32_t team;          // offset 16, 4 bytes (0 = Red, 1 = Blue)
    int32_t _pad;          // offset 20, 4 bytes (std430 alignment padding)
};

static_assert(sizeof(Soldier) == 24, "Soldier must be 24 bytes for std430");
static_assert(alignof(Soldier) == 8, "Soldier must be 8-byte aligned for std430");
