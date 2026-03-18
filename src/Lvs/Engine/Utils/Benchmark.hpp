#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Utils::Benchmark {

struct Stats {
    std::uint64_t Count{0};
    std::uint64_t TotalNs{0};
    std::uint64_t MaxNs{0};
};

void SetEnabled(bool enabled);
[[nodiscard]] bool Enabled();

// Dumps aggregated stats to stdout (intended for terminal usage).
void DumpToStdout();

class Scope final {
public:
    explicit Scope(const char* tag);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

private:
    const char* tag_{nullptr};
    std::chrono::steady_clock::time_point start_{};
    bool active_{false};
};

} // namespace Lvs::Engine::Utils::Benchmark

#define LVS_BENCH__CONCAT_INNER(a, b) a##b
#define LVS_BENCH__CONCAT(a, b) LVS_BENCH__CONCAT_INNER(a, b)
#define LVS_BENCH_SCOPE(tag_literal) \
    ::Lvs::Engine::Utils::Benchmark::Scope LVS_BENCH__CONCAT(LVS_BENCH__scope_, __LINE__)(tag_literal)
