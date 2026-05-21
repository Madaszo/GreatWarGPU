#include "greatwargpu/gpu/GPUContext.hpp"

namespace gwgpu {

GPUContext::GPUContext(GPUContextConfig config)
    : config_(config) {
}

void GPUContext::initialize() {
    ready_ = true;
}

void GPUContext::shutdown() noexcept {
    ready_ = false;
}

bool GPUContext::is_ready() const noexcept {
    return ready_;
}

} // namespace gwgpu