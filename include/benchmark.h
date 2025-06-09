#pragma once

#include "doctest.h"
#include <chrono>
#include <string_view>

namespace bench {

/// @brief RAII timer used for simple benchmarks.
class BenchmarkGuard {
public:
    /// @brief Begin timing with a label.
    BenchmarkGuard(std::string_view name,
                   const char* file,
                   int line);

    /// @brief Stop timing and report the result.
    ~BenchmarkGuard();

    /// @brief Allow use in single iteration for loops.
    explicit operator bool() const { return active_; }

    /// @brief End the timing loop.
    void stop() { active_ = false; }

private:
    std::string_view name_{}; ///< Benchmark label.
    const char* file_{};      ///< Source file location.
    int line_{};              ///< Source line location.
    std::chrono::steady_clock::time_point start_{}; ///< Start point of timing.
    bool active_{true};       ///< Loop state flag.
};

inline BenchmarkGuard::BenchmarkGuard(std::string_view name,
                                      const char* file,
                                      int line)
    : name_{name}, file_{file}, line_{line}, start_{std::chrono::steady_clock::now()} {}

inline BenchmarkGuard::~BenchmarkGuard() {
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
    INFO(name_ << " ms: " << ms);
}

} // namespace bench

#define BENCHMARK(name) \
    for (bench::BenchmarkGuard bench_guard{name, __FILE__, __LINE__}; bench_guard; bench_guard.stop())

