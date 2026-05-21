#pragma once

#include "greatwar/render/Camera.hpp"
#include "greatwar/render/RenderSnapshot.hpp"

namespace greatwar {

class Renderer {
public:
    Renderer() = default;
    void initialize();
    void render(const RenderSnapshot& snap, const Camera& cam);
    void shutdown();
};

} // namespace greatwar
