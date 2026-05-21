#include "greatwar/commands/CommandHierarchy.hpp"

namespace greatwar {

void CommandHierarchy::push_buffer(CommandBuffer buf) {
    buffers_.push_back(std::move(buf));
}

void CommandHierarchy::clear() {
    buffers_.clear();
}

const std::vector<CommandBuffer>& CommandHierarchy::buffers() const noexcept {
    return buffers_;
}

} // namespace greatwar
