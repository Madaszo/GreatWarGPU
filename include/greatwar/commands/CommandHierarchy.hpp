#pragma once

#include "greatwar/commands/CommandBuffer.hpp"
#include <vector>

namespace greatwar {

class CommandHierarchy {
public:
    void push_buffer(CommandBuffer buf);
    void clear();

    [[nodiscard]] const std::vector<CommandBuffer>& buffers() const noexcept;

private:
    std::vector<CommandBuffer> buffers_;
};

} // namespace greatwar
