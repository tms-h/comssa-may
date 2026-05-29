// Benchmark 2: linear search vs binary search, SWEPT over array size N.
//
// For each N we build a sorted int array and run a big batch of lookups of
// random keys (~50% present, ~50% absent). We time:
//
//   linear:  hand-rolled scan from the front (returns lower_bound-style index)
//   binary:  std::lower_bound
//
// Big-O says binary (O(log n)) should crush linear (O(n)). The point of the
// SWEEP is the CROSSOVER: for small N linear actually wins, because the scan is
// sequential (cache-friendly), branch-predictable, and auto-vectorizable, while
// binary search jumps around memory and mispredicts at every step. We print the
// per-query time for both and mark where binary overtakes linear.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "bench_common.hpp"

// lower_bound, hand-rolled as a linear scan: index of first element >= key.
//
// We DON'T early-exit. On a sorted array, the lower-bound index is simply the
// count of elements strictly less than `key`, so we just sum a comparison over
// the whole array. That sum has no data-dependent branch, so the compiler
// auto-vectorizes it (it becomes a handful of SIMD compares + adds) and the
// branch predictor never misses. This is the "dumb but cache-friendly and
// vectorizable" sequential scan the article is about. An early-exit scan would
// instead mispredict ~50% of the time and lose its whole advantage.
static inline int linear_lb(const std::int32_t* a, int n, std::int32_t key) {
    int count = 0;
    for (int i = 0; i < n; ++i) count += (a[i] < key);
    return count;
}

int main(int argc, char** argv) {
    int trials = (argc > 1) ? std::atoi(argv[1]) : 9;
    const int warmup = 3;

    const std::vector<int> Ns = {8,    16,   32,    64,    128,   256,
                                 512,  1024, 4096,  16384, 65536};

    std::printf("# Benchmark 2: linear vs binary search, swept over N\n");
    std::printf("# ~50%% present / 50%% absent keys; warmup=%d trials=%d\n",
                warmup, trials);
    std::printf("# per-query time = batch time / number of queries\n\n");
    std::printf("%-8s %14s %14s %12s   %s\n", "N", "linear(ns/q)",
                "binary(ns/q)", "lin/bin", "winner");

    std::printf("CSV,N,linear_ns_per_query,binary_ns_per_query\n");

    auto rng = bench::make_rng();
    int crossover_N = -1;

    for (int N : Ns) {
        // Sorted array of even numbers 0,2,4,... so odd query keys are absent.
        std::vector<std::int32_t> a(N);
        for (int i = 0; i < N; ++i) a[i] = 2 * i;

        // Keep total linear work roughly constant so the sweep finishes fast,
        // while keeping enough queries for a stable measurement.
        long q = 50'000'000L / (N > 0 ? N : 1);
        if (q < 2000) q = 2000;
        if (q > 4'000'000L) q = 4'000'000L;
        const int Q = static_cast<int>(q);

        // Pre-generate the query keys OUTSIDE the timed region. Same keys for
        // both methods. Range [0, 2N) -> evens hit, odds miss.
        std::vector<std::int32_t> keys(Q);
        std::uniform_int_distribution<int> dist(0, 2 * N - 1);
        for (int i = 0; i < Q; ++i) keys[i] = dist(rng);

        const std::int32_t* ap = a.data();

        auto linear_batch = [&] {
            std::int64_t checksum = 0;
            for (int i = 0; i < Q; ++i) checksum += linear_lb(ap, N, keys[i]);
            return checksum;
        };
        auto binary_batch = [&] {
            std::int64_t checksum = 0;
            for (int i = 0; i < Q; ++i)
                checksum += std::lower_bound(ap, ap + N, keys[i]) - ap;
            return checksum;
        };

        bench::Stats lin = bench::run_trials(warmup, trials, linear_batch);
        bench::Stats bin = bench::run_trials(warmup, trials, binary_batch);

        double lin_per = lin.min_ns / Q;
        double bin_per = bin.min_ns / Q;
        const char* winner = (lin_per < bin_per) ? "linear" : "binary";
        if (crossover_N < 0 && bin_per < lin_per) crossover_N = N;

        std::printf("%-8d %14.3f %14.3f %12.2f   %s\n", N, lin_per, bin_per,
                    lin_per / bin_per, winner);
        std::printf("CSV,%d,%.4f,%.4f\n", N, lin_per, bin_per);
    }

    if (crossover_N > 0)
        std::printf("\n# Crossover: binary first wins at N = %d.\n",
                    crossover_N);
    else
        std::printf("\n# No crossover in this range (linear won throughout).\n");
    return 0;
}
