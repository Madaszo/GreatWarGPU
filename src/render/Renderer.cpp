#include "greatwargpu/render/Renderer.hpp"
#include "greatwargpu/gpu/GPUContext.hpp"
#include "greatwargpu/simulation/Simulation.hpp"

namespace gwgpu {

void Renderer::initialize(GPUContext&) {
    ready_ = true;
}

void Renderer::render(const Simulation&, GPUContext&) {
}

void Renderer::shutdown() noexcept {
    ready_ = false;
}

bool Renderer::is_ready() const noexcept {
    return ready_;
}

} // namespace gwgpu