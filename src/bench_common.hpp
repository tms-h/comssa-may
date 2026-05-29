// bench_common.hpp — shared microbenchmark scaffolding.
//
// Everything here exists to make the timing trustworthy:
//   * steady_clock for monotonic timing,
//   * a do_not_optimize() sink so the optimizer can't delete the work,
//   * warm-up + multiple trials, reporting MIN and median,
//   * a fixed RNG seed for reproducibility.
//
// Build with -O2 or -O3. A -O0/debug build makes every number here meaningless.

#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace bench {

using clock_t = std::chrono::steady_clock;

// ---- The sink ------------------------------------------------------------
// Force `value` to be treated as observable so the compiler cannot prove the
// computation that produced it is dead and delete it. This is the standard
// Google-Benchmark-style trick. The empty asm reads `value` and clobbers
// memory, so the loop that produced it must actually run.
template <class T>
inline void do_not_optimize(const T& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// A reproducible RNG. Same seed every run -> same arrays, same queries.
inline std::mt19937_64 make_rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) {
    return std::mt19937_64(seed);
}

// ---- Stats over a set of trial timings -----------------------------------
struct Stats {
    double min_ns = 0, median_ns = 0, mean_ns = 0, stddev_ns = 0;
    int trials = 0;
};

inline Stats summarize(std::vector<double> ns) {
    Stats s;
    s.trials = static_cast<int>(ns.size());
    if (ns.empty()) return s;
    std::sort(ns.begin(), ns.end());
    s.min_ns = ns.front();
    s.median_ns = ns[ns.size() / 2];
    s.mean_ns = std::accumulate(ns.begin(), ns.end(), 0.0) / ns.size();
    double acc = 0;
    for (double v : ns) acc += (v - s.mean_ns) * (v - s.mean_ns);
    s.stddev_ns = ns.size() > 1 ? std::sqrt(acc / (ns.size() - 1)) : 0.0;
    return s;
}

// ---- Trial runner --------------------------------------------------------
// `prep` runs UNTIMED before each timed `body` (use it to (re)build state, do
// allocation, etc. — anything that shouldn't be in the timed region).
// `body` returns a checksum that we feed to the sink so it can't be elided.
//
// We report the MIN as the headline number: for a microbenchmark the fastest
// observed run is the one least polluted by interrupts / scheduling noise.
template <class Prep, class Body>
Stats run_trials(int warmup, int trials, Prep prep, Body body) {
    for (int i = 0; i < warmup; ++i) {
        prep();
        auto r = body();
        do_not_optimize(r);
    }
    std::vector<double> times;
    times.reserve(trials);
    for (int i = 0; i < trials; ++i) {
        prep();
        auto t0 = clock_t::now();
        auto r = body();
        auto t1 = clock_t::now();
        do_not_optimize(r);
        times.push_back(
            std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return summarize(times);
}

// Convenience overload: no per-trial prep needed.
template <class Body>
Stats run_trials(int warmup, int trials, Body body) {
    return run_trials(warmup, trials, [] {}, body);
}

}  // namespace bench
