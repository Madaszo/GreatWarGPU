#pragma once

#include "greatwar/commands/Command.hpp"
#include <vector>

namespace greatwar {

struct CommandBuffer {
    std::vector<Command> commands;
};

} // namespace greatwar
