// Benchmark 3: std::list vs std::vector on a FIND-AND-DELETE workload.
//
// This is the Stroustrup "are lists evil?" result. The workload is:
//
//   repeatedly: search for an element BY VALUE (linear traversal from begin),
//               then erase it.
//
// Finding the element is the whole point. std::list's per-node erase really is
// O(1) once you HOLD the iterator — but to find the node you must chase `next`
// pointers through nodes scattered across the heap, which is roughly a cache
// miss per node. std::vector has to memmove on erase (O(n)), but its search is
// a flat, prefetchable, vectorizable scan, and the memmove is a single bulk
// contiguous copy. The vector wins comfortably despite the "worse" erase.
//
// IMPORTANT: we do NOT pre-hold the iterator and time only the erase. That
// would measure the wrong thing (and would flatter the list). We time
// find-then-erase together, which is what real code does.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <list>
#include <vector>

#include <string>

#include "bench_common.hpp"

// One find-and-delete pass over a vector built from `build_order`, removing in
// `removal_order`. Returns elapsed milliseconds. Build is UNTIMED.
static double time_vector_pass(const std::vector<std::int32_t>& build_order,
                               const std::vector<std::int32_t>& removal_order) {
    std::vector<std::int32_t> vec = build_order;  // untimed
    auto t0 = bench::clock_t::now();
    std::int64_t checksum = 0;
    for (std::int32_t target : removal_order) {
        auto it = std::find(vec.begin(), vec.end(), target);
        checksum += static_cast<std::int64_t>(it - vec.begin());
        vec.erase(it);
    }
    auto t1 = bench::clock_t::now();
    bench::do_not_optimize(checksum);
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

static double time_list_pass(const std::vector<std::int32_t>& build_order,
                             const std::vector<std::int32_t>& removal_order) {
    std::list<std::int32_t> lst(build_order.begin(), build_order.end());  // untimed
    auto t0 = bench::clock_t::now();
    std::int64_t checksum = 0;
    for (std::int32_t target : removal_order) {
        auto it = std::find(lst.begin(), lst.end(), target);
        checksum += target;
        lst.erase(it);
    }
    auto t1 = bench::clock_t::now();
    bench::do_not_optimize(checksum);
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Monte Carlo mode: instead of timing one fixed instance many times and taking
// the min, we draw MANY independent RANDOM instances (each with its own shuffle
// of the build order and the removal order) and time a single pass of each. The
// spread of results then reflects real variation across inputs, not just timer
// noise. We emit every run so the distributions can be plotted on one graph.
static int run_montecarlo(int N, int runs) {
    std::printf("# Benchmark 3 (Monte Carlo): list vs vector, find-and-delete\n");
    std::printf("# N=%d, %d independent random instances, one timed pass each\n",
                N, runs);
    std::printf("# columns: MC,run,container,ms\n");

    std::vector<std::int32_t> base(N);
    for (int i = 0; i < N; ++i) base[i] = i;

    // A few untimed warm-up passes (allocator / cache warm-up).
    {
        auto rng = bench::make_rng(1);
        auto bo = base, ro = base;
        std::shuffle(bo.begin(), bo.end(), rng);
        std::shuffle(ro.begin(), ro.end(), rng);
        for (int w = 0; w < 2; ++w) {
            bench::do_not_optimize(time_vector_pass(bo, ro));
            bench::do_not_optimize(time_list_pass(bo, ro));
        }
    }

    std::vector<double> vec_ms, lst_ms;
    vec_ms.reserve(runs);
    lst_ms.reserve(runs);
    for (int r = 0; r < runs; ++r) {
        // Distinct, reproducible seed per run -> a distinct random instance.
        auto rng = bench::make_rng(0xABCDEF01ull + r);
        auto build_order = base, removal_order = base;
        std::shuffle(build_order.begin(), build_order.end(), rng);
        std::shuffle(removal_order.begin(), removal_order.end(), rng);

        double v = time_vector_pass(build_order, removal_order);
        double l = time_list_pass(build_order, removal_order);
        vec_ms.push_back(v);
        lst_ms.push_back(l);
        std::printf("MC,%d,vector,%.5f\n", r, v);
        std::printf("MC,%d,list,%.5f\n", r, l);
    }

    bench::Stats vs = bench::summarize(vec_ms);
    bench::Stats ls = bench::summarize(lst_ms);
    int vector_won = 0;
    for (int r = 0; r < runs; ++r)
        if (vec_ms[r] < lst_ms[r]) ++vector_won;
    std::printf("# vector  ms: min=%.2f median=%.2f mean=%.2f sd=%.2f\n",
                vs.min_ns, vs.median_ns, vs.mean_ns, vs.stddev_ns);
    std::printf("# list    ms: min=%.2f median=%.2f mean=%.2f sd=%.2f\n",
                ls.min_ns, ls.median_ns, ls.mean_ns, ls.stddev_ns);
    std::printf("# vector won %d / %d runs (median list/vector = %.2fx)\n",
                vector_won, runs, ls.median_ns / vs.median_ns);
    return 0;
}

int main(int argc, char** argv) {
    // Monte Carlo mode:  ./bench3_list_vs_vector mc [N] [runs]
    if (argc > 1 && std::string(argv[1]) == "mc") {
        int N = (argc > 2) ? std::atoi(argv[2]) : 8000;
        int runs = (argc > 3) ? std::atoi(argv[3]) : 200;
        return run_montecarlo(N, runs);
    }

    int N = (argc > 1) ? std::atoi(argv[1]) : 20000;
    int trials = (argc > 2) ? std::atoi(argv[2]) : 5;
    const int warmup = 1;

    std::printf("# Benchmark 3: std::list vs std::vector, find-and-delete\n");
    std::printf("# N = %d elements, removed one-by-one (find by value, erase)\n",
                N);
    std::printf("# warmup=%d trials=%d\n\n", warmup, trials);

    // A fixed, shuffled removal order: we remove every value exactly once, in a
    // random sequence. Same sequence for both containers. Built once, untimed.
    auto rng = bench::make_rng();
    std::vector<std::int32_t> values(N);
    for (int i = 0; i < N; ++i) values[i] = i;
    std::vector<std::int32_t> removal_order = values;
    std::shuffle(removal_order.begin(), removal_order.end(), rng);

    // --- vector path ---
    // prep: rebuild a fresh vector (untimed). body: find+erase every element.
    std::vector<std::int32_t> vec;
    auto vec_prep = [&] {
        vec = values;  // contiguous, fresh copy each trial
    };
    auto vec_body = [&] {
        std::int64_t checksum = 0;
        for (std::int32_t target : removal_order) {
            auto it = std::find(vec.begin(), vec.end(), target);
            checksum += static_cast<std::int64_t>(it - vec.begin());
            vec.erase(it);  // O(n) memmove of the tail
        }
        return checksum;
    };

    // --- list path ---
    // prep: rebuild a fresh list (untimed). body: find+erase every element.
    std::list<std::int32_t> lst;
    auto lst_prep = [&] {
        lst.assign(values.begin(), values.end());
    };
    auto lst_body = [&] {
        std::int64_t checksum = 0;
        for (std::int32_t target : removal_order) {
            auto it = std::find(lst.begin(), lst.end(), target);
            // position-as-distance, just to fold the traversal into the sink
            checksum += target;
            lst.erase(it);  // O(1) given the iterator...
        }
        return checksum;
    };

    bench::Stats vec_s = bench::run_trials(warmup, trials, vec_prep, vec_body);
    bench::Stats lst_s = bench::run_trials(warmup, trials, lst_prep, lst_body);

    auto to_ms = [](double ns) { return ns / 1e6; };
    std::printf("%-12s %12s %12s %12s %12s\n", "container", "min(ms)",
                "median(ms)", "mean(ms)", "stddev(ms)");
    std::printf("%-12s %12.2f %12.2f %12.2f %12.2f\n", "vector",
                to_ms(vec_s.min_ns), to_ms(vec_s.median_ns),
                to_ms(vec_s.mean_ns), to_ms(vec_s.stddev_ns));
    std::printf("%-12s %12.2f %12.2f %12.2f %12.2f\n", "list",
                to_ms(lst_s.min_ns), to_ms(lst_s.median_ns),
                to_ms(lst_s.mean_ns), to_ms(lst_s.stddev_ns));
    std::printf("\n# list / vector (min): %.2fx slower\n",
                lst_s.min_ns / vec_s.min_ns);

    std::printf("CSV,container,min_ms,median_ms,mean_ms,stddev_ms\n");
    std::printf("CSV,vector,%.4f,%.4f,%.4f,%.4f\n", to_ms(vec_s.min_ns),
                to_ms(vec_s.median_ns), to_ms(vec_s.mean_ns),
                to_ms(vec_s.stddev_ns));
    std::printf("CSV,list,%.4f,%.4f,%.4f,%.4f\n", to_ms(lst_s.min_ns),
                to_ms(lst_s.median_ns), to_ms(lst_s.mean_ns),
                to_ms(lst_s.stddev_ns));
    return 0;
}
