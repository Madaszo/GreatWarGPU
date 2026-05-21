#pragma once

#include "greatwar/simulation/Grid.hpp"
#include "greatwar/simulation/WorldState.hpp"
#include "greatwar/simulation/Tick.hpp"

namespace greatwar {

class Simulation {
public:
    Simulation() = default;
    explicit Simulation(const Grid& grid);

    void reset(const Grid& grid);
    void tick(const Tick& t);

    [[nodiscard]] const WorldState& state() const noexcept;

private:
    Grid grid_{};
    WorldState state_{};
};

} // namespace greatwar
