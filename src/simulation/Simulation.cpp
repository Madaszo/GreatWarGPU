#include "greatwargpu/simulation/Simulation.hpp"
#include "greatwargpu/command/CommandHierarchy.hpp"

namespace gwgpu {

Simulation::Simulation(SimulationConfig config)
    : config_(config) {
}

void Simulation::reset(SimulationConfig config) {
    config_ = config;
    frame_index_ = 0;
}

void Simulation::step(const CommandHierarchy&) {
    ++frame_index_;
}

uint64_t Simulation::frame_index() const noexcept {
    return frame_index_;
}

} // namespace gwgpu