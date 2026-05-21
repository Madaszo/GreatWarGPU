#pragma once

namespace greatwar {

class GPUContext {
public:
    GPUContext() = default;
    void initialize();
    void shutdown();
    [[nodiscard]] bool ready() const noexcept { return ready_; }

private:
    bool ready_ = false;
};

} // namespace greatwar
