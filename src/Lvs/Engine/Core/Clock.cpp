#include "Lvs/Engine/Core/Clock.hpp"

#include <thread>

namespace Lvs::Engine::Core {

FrameClock::FrameClock()
    : last_(std::chrono::steady_clock::now()) {
}

void FrameClock::SetFPSLimit(const std::optional<int> fps) {
    if (!fps.has_value() || fps.value() <= 0) {
        targetFrameTime_.reset();
        return;
    }

    targetFrameTime_ = 1.0 / static_cast<double>(fps.value());
}

std::pair<double, double> FrameClock::Tick() {
    auto now = std::chrono::steady_clock::now();
    double deltaTime = std::chrono::duration<double>(now - last_).count();

    if (targetFrameTime_.has_value()) {
        const double remaining = targetFrameTime_.value() - deltaTime;
        if (remaining > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
            now = std::chrono::steady_clock::now();
            deltaTime = std::chrono::duration<double>(now - last_).count();
        }
    }

    total_ += deltaTime;
    last_ = now;
    delta_ = deltaTime;
    return {delta_, total_};
}

double FrameClock::Delta() const {
    return delta_;
}

double FrameClock::Total() const {
    return total_;
}

} // namespace Lvs::Engine::Core
