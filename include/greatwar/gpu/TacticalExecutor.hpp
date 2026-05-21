#pragma once

#include "greatwar/gpu/BufferSet.hpp"

namespace greatwar {

class TacticalExecutor {
public:
    TacticalExecutor() = default;
    void dispatch(BufferSet& buffers);
};

} // namespace greatwar
