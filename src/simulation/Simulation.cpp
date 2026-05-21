#include "greatwar/simulation/Simulation.hpp"

namespace greatwar {

Simulation::Simulation(const Grid& grid)
    : grid_(grid) {
}

void Simulation::reset(const Grid& grid) {
    grid_ = grid;
    state_ = {};
}

void Simulation::tick(const Tick& t) {
    state_.tick = t.index;
}

const WorldState& Simulation::state() const noexcept {
    return state_;
}

} // namespace greatwar
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