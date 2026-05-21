#include "greatwargpu/command/CommandHierarchy.hpp"

namespace gwgpu {

void CommandHierarchy::clear() {
    layers_.clear();
}

void CommandHierarchy::push_layer(Layer layer) {
    layers_.push_back(std::move(layer));
}

const std::vector<CommandHierarchy::Layer>& CommandHierarchy::layers() const noexcept {
    return layers_;
}

} // namespace gwgpu