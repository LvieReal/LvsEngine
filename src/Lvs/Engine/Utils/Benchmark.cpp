#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace Lvs::Engine::Utils::Benchmark {

namespace {

std::atomic<bool> g_enabled{false};

std::mutex g_registryMutex;
std::vector<std::unordered_map<const char*, Stats>*> g_threadStats;

thread_local bool t_registered = false;
thread_local std::unordered_map<const char*, Stats> t_stats;

void EnsureThreadRegistered() {
    if (t_registered) {
        return;
    }
    t_stats.reserve(128);
    std::scoped_lock lock(g_registryMutex);
    g_threadStats.push_back(&t_stats);
    t_registered = true;
}

} // namespace

void SetEnabled(const bool enabled) {
    g_enabled.store(enabled, std::memory_order_release);
}

bool Enabled() {
    return g_enabled.load(std::memory_order_acquire);
}

Scope::Scope(const char* tag)
    : tag_(tag) {
    if (!Enabled() || tag_ == nullptr) {
        return;
    }
    EnsureThreadRegistered();
    active_ = true;
    start_ = std::chrono::steady_clock::now();
}

Scope::~Scope() {
    if (!active_ || tag_ == nullptr) {
        return;
    }

    const auto end = std::chrono::steady_clock::now();
    const auto ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count()
    );

    auto& stat = t_stats[tag_];
    stat.Count += 1;
    stat.TotalNs += ns;
    stat.MaxNs = std::max(stat.MaxNs, ns);
}

void DumpToStdout() {
    if (!Enabled()) {
        return;
    }

    std::unordered_map<const char*, Stats> merged;
    merged.reserve(256);

    {
        std::scoped_lock lock(g_registryMutex);
        for (const auto* threadMap : g_threadStats) {
            if (threadMap == nullptr) {
                continue;
            }
            for (const auto& [tag, stat] : *threadMap) {
                auto& out = merged[tag];
                out.Count += stat.Count;
                out.TotalNs += stat.TotalNs;
                out.MaxNs = std::max(out.MaxNs, stat.MaxNs);
            }
        }
    }

    std::vector<std::pair<const char*, Stats>> rows;
    rows.reserve(merged.size());
    for (const auto& kv : merged) {
        rows.push_back(kv);
    }

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second.TotalNs > b.second.TotalNs;
    });

    std::printf("\n=== LVS BENCH (aggregated) ===\n");
    std::printf("%-44s %12s %14s %14s %14s\n", "Tag", "Count", "Total(ms)", "Avg(us)", "Max(us)");
    for (const auto& [tag, stat] : rows) {
        const double totalMs = static_cast<double>(stat.TotalNs) / 1.0e6;
        const double avgUs = stat.Count > 0 ? (static_cast<double>(stat.TotalNs) / 1.0e3) / static_cast<double>(stat.Count) : 0.0;
        const double maxUs = static_cast<double>(stat.MaxNs) / 1.0e3;
        std::printf("%-44s %12llu %14.3f %14.3f %14.3f\n",
            tag != nullptr ? tag : "(null)",
            static_cast<unsigned long long>(stat.Count),
            totalMs,
            avgUs,
            maxUs
        );
    }
    std::printf("=== LVS BENCH END ===\n");
    std::fflush(stdout);
}

} // namespace Lvs::Engine::Utils::Benchmark

