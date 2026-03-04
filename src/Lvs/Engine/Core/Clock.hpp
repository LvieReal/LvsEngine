#pragma once

#include <chrono>
#include <optional>
#include <utility>

namespace Lvs::Engine::Core {

class FrameClock {
public:
    FrameClock();

    void SetFPSLimit(std::optional<int> fps);
    std::pair<double, double> Tick();

    [[nodiscard]] double Delta() const;
    [[nodiscard]] double Total() const;

private:
    std::chrono::steady_clock::time_point last_;
    double delta_{0.0};
    double total_{0.0};
    std::optional<double> targetFrameTime_;
};

} // namespace Lvs::Engine::Core
