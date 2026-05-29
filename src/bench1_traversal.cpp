// Benchmark 1: row-major vs column-major traversal of a large 2D array.
//
// We allocate ONE flat N*N array of ints (so the storage is genuinely
// contiguous) and sum every element two ways:
//
//   row-major: inner loop walks the contiguous dimension  (stride 1)
//   col-major: inner loop walks down columns               (stride N)
//
// Identical O(n^2), identical arithmetic, identical data. The ONLY difference
// is access order. Row-major touches memory sequentially (one cache line feeds
// ~16 ints, and the prefetcher sees the pattern); col-major jumps N ints each
// step, so at large N almost every access is a fresh cache line.
//
// With N = 8192, the array is 8192*8192*4 = 256 MiB, far larger than any cache
// on this machine, so the effect is not hidden by caching.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "bench_common.hpp"

int main(int argc, char** argv) {
    int N = (argc > 1) ? std::atoi(argv[1]) : 8192;
    int trials = (argc > 2) ? std::atoi(argv[2]) : 7;
    const int warmup = 2;

    const std::size_t total = static_cast<std::size_t>(N) * N;
    std::printf("# Benchmark 1: row-major vs column-major traversal\n");
    std::printf("# N = %d  (%zu elements, %.1f MiB, int32)\n", N, total,
                total * sizeof(std::int32_t) / (1024.0 * 1024.0));
    std::printf("# warmup=%d trials=%d\n\n", warmup, trials);

    // Allocation is OUTSIDE the timed region. Fill with a fixed pattern so the
    // checksums are deterministic and nonzero.
    std::vector<std::int32_t> a(total);
    auto rng = bench::make_rng();
    for (auto& x : a) x = static_cast<std::int32_t>(rng() & 0xFF);

    auto row_major = [&] {
        std::int64_t sum = 0;
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                sum += a[static_cast<std::size_t>(i) * N + j];
        return sum;
    };
    auto col_major = [&] {
        std::int64_t sum = 0;
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i)
                sum += a[static_cast<std::size_t>(i) * N + j];
        return sum;
    };

    bench::Stats row = bench::run_trials(warmup, trials, row_major);
    bench::Stats col = bench::run_trials(warmup, trials, col_major);

    auto to_ms = [](double ns) { return ns / 1e6; };
    std::printf("%-12s %12s %12s %12s %12s\n", "order", "min(ms)",
                "median(ms)", "mean(ms)", "stddev(ms)");
    std::printf("%-12s %12.2f %12.2f %12.2f %12.2f\n", "row-major",
                to_ms(row.min_ns), to_ms(row.median_ns), to_ms(row.mean_ns),
                to_ms(row.stddev_ns));
    std::printf("%-12s %12.2f %12.2f %12.2f %12.2f\n", "col-major",
                to_ms(col.min_ns), to_ms(col.median_ns), to_ms(col.mean_ns),
                to_ms(col.stddev_ns));
    std::printf("\n# col-major / row-major (min): %.2fx slower\n",
                col.min_ns / row.min_ns);

    // CSV (optional): redirect or grep on the CSV: prefix.
    std::printf("CSV,order,min_ms,median_ms,mean_ms,stddev_ms\n");
    std::printf("CSV,row-major,%.4f,%.4f,%.4f,%.4f\n", to_ms(row.min_ns),
                to_ms(row.median_ns), to_ms(row.mean_ns), to_ms(row.stddev_ns));
    std::printf("CSV,col-major,%.4f,%.4f,%.4f,%.4f\n", to_ms(col.min_ns),
                to_ms(col.median_ns), to_ms(col.mean_ns), to_ms(col.stddev_ns));
    return 0;
}
