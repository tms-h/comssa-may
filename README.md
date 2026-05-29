# Time Complexity Isn't All You Need — benchmark kit

**Live site: <https://tms-h.github.io/comssa-may/>** — read the
[full article](https://tms-h.github.io/comssa-may/article.html) or the
[one-page version](https://tms-h.github.io/comssa-may/mini_article.html).

Three small, self-contained C++ benchmarks that show where Big-O stops
predicting real performance, because real hardware has a memory hierarchy and a
speculating pipeline. Companion to the article in [`article.md`](article.md).

1. **Row-major vs column-major** traversal of a big 2D array (spatial locality /
   cache lines / prefetching).
2. **Linear vs binary search, swept over N** — finds the *crossover* where the
   "worse" O(n) scan beats O(log n) binary search for small arrays.
3. **`std::list` vs `std::vector`** on a find-and-delete workload (the
   Stroustrup "lists are evil" result).

Everything is plain C++17 with no dependencies.

## Quick start

```sh
make run          # build all three at -O2 and run them
# or, individually:
make              # build only
./bin/bench1_traversal
./bin/bench2_search
./bin/bench3_list_vs_vector
```

Useful knobs:

```sh
make OPT=-O3      # try -O3
make NATIVE=1     # add -mcpu=native (ARM) / -march=native (x86) for this CPU
make clean
./bin/bench1_traversal 4096 9    # optional args: N, trials
./bin/bench3_list_vs_vector 50000 5
```

Each program also prints `CSV,...` lines so you can drop the numbers straight
into a spreadsheet: `./bin/bench2_search | grep '^CSV' | cut -d, -f2-`.

## Read this before trusting any number

- **Build optimized.** All numbers below are `-O2`. A `-O0`/debug build makes
  these benchmarks **meaningless** — see "Why -O0 lies" at the bottom. Always
  `-O2` or higher.
- **Dead-code elimination is prevented.** Every loop folds its result into a
  checksum that is fed to a `do_not_optimize()` sink (an
  `asm volatile("" : : "r,m"(x) : "memory")` escape). Without this the optimizer
  would simply delete the loop we're trying to time.
- **Timing:** `std::chrono::steady_clock`, warm-up runs before timing, ≥5 trials.
  We report **MIN** (most stable for microbenchmarks — the run least disturbed
  by the OS) plus median, mean, and stddev.
- **Fixed RNG seed** (`std::mt19937_64`, constant seed) so runs are reproducible.
- **Allocation is kept out of the timed region** (rebuilds happen in an untimed
  `prep` step) except where the work itself frees memory (list node erase).

The scaffolding lives in [`src/bench_common.hpp`](src/bench_common.hpp).

## Machine these numbers were measured on

| | |
|---|---|
| CPU | Apple M1 Pro (8 cores: 6 performance + 2 efficiency), ARM64 |
| Caches | L1d 64 KiB/core, L2 4 MiB (shared per cluster), ~24 MiB system-level cache |
| RAM | 16 GiB unified |
| OS | macOS 26.5 (build 25F71) |
| Compiler | Apple clang 17.0.0 (clang-1700.0.13.5), C++17 |
| Flags | `-O2 -std=c++17` (headline). `-O3 -mcpu=native` reproduced the same results within noise. |

> Build note for *this particular machine:* the full-Xcode license had not been
> accepted, which makes the default `clang++`/`make` refuse to run. The numbers
> were produced with the standalone Command Line Tools toolchain and GNU make:
> ```sh
> SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
> gmake CXX=/Library/Developer/CommandLineTools/usr/bin/clang++ \
>       SYSROOT="-isysroot $SDK -isystem $SDK/usr/include/c++/v1" OPT=-O2 run
> ```
> On a normal machine with a working toolchain, plain `make run` is all you need.

---

## Results

### Benchmark 1 — row-major vs column-major (N = 8192, 256 MiB int array)

Same array, same arithmetic, same O(n²) — only the access order differs.

| order | min (ms) | median (ms) | mean (ms) | stddev (ms) |
|---|---:|---:|---:|---:|
| row-major (stride 1) | 5.06 | 5.14 | 5.15 | 0.09 |
| col-major (stride N) | 347.83 | 360.19 | 358.28 | 7.80 |

**Column-major is 68.7× slower.** Walking down columns touches a fresh cache
line (and often a fresh page → TLB miss) on almost every step; walking along
rows lets one cache line feed ~16 consecutive ints and lets the prefetcher run
ahead.

### Benchmark 2 — linear vs binary search, swept over N

Sorted `int` array, ~50% present / 50% absent random keys. Linear search here is
the *branchless* lower-bound (count elements `< key`): no early exit, so it
auto-vectorizes and never mispredicts. Binary search is `std::lower_bound`.

| N | linear (ns/query) | binary (ns/query) | lin/bin | winner |
|---:|---:|---:|---:|:--|
| 8 | 4.54 | 6.02 | 0.75 | **linear** |
| 16 | 1.95 | 5.90 | 0.33 | **linear** |
| 32 | 2.67 | 6.47 | 0.41 | **linear** |
| 64 | 4.09 | 7.46 | 0.55 | **linear** |
| 128 | 7.38 | 8.92 | 0.83 | **linear** |
| 256 | 13.97 | 10.40 | 1.34 | binary |
| 512 | 27.01 | 12.30 | 2.20 | binary |
| 1024 | 53.67 | 14.28 | 3.76 | binary |
| 4096 | 215.42 | 18.50 | 11.64 | binary |
| 16384 | 843.55 | 23.33 | 36.16 | binary |
| 65536 | 4028.54 | 42.10 | 95.68 | binary |

**Crossover ≈ N = 256.** For arrays of ~128 elements or fewer, the O(n) scan
*beats* O(log n) binary search on this machine. Below the crossover the whole
array fits in a few cache lines, the scan is one tight vectorized loop, and
binary search's data-dependent jumps just mispredict and stall.

### Benchmark 3 — `std::list` vs `std::vector`, find-and-delete (N = 20000)

Workload: repeatedly **find an element by value (linear traversal)** then erase
it, until empty. Finding is included on purpose — that's the realistic part.

| container | min (ms) | median (ms) | mean (ms) | stddev (ms) |
|---|---:|---:|---:|---:|
| vector | 49.33 | 49.56 | 49.58 | 0.18 |
| list | 242.14 | 253.13 | 252.06 | 9.16 |

**The list is 4.9× slower** despite its genuinely O(1) per-node erase. The cost
is the *traversal*: chasing `next` pointers through heap-scattered nodes is
roughly a cache miss per node. The vector's scan is contiguous and vectorizable,
and its O(n) erase is a single bulk `memmove`. Contiguous-and-dumb beats
pointer-chasing-and-clever.

> If you instead pre-hold the iterator and time *only* `list::erase`, you measure
> the wrong thing and the list looks great. We don't do that — see the comment
> at the top of [`src/bench3_list_vs_vector.cpp`](src/bench3_list_vs_vector.cpp).

#### Monte Carlo (distribution, not a single number)

A `mc` mode draws **many independent random instances** (each a fresh shuffle of
build + deletion order) and times one pass of each, so you see the whole
*distribution* instead of one min:

```sh
./bin/bench3_list_vs_vector mc 8000 200 > results/montecarlo_bench3.csv
```

200 runs at N = 8000 on this machine:

| container | min (ms) | median (ms) | mean (ms) | stddev (ms) |
|---|---:|---:|---:|---:|
| vector | 7.80 | 8.04 | 8.52 | 1.95 |
| list | 31.24 | 37.65 | 40.65 | 13.03 |

**`std::vector` won all 200/200 runs** (median 4.7× faster). Note the stddev:
the list isn't just slower, it's ~7× more variable — its times smear out into a
long tail past 100 ms depending on how the nodes happen to scatter, while the
vector stays tight. See the overlaid-distribution chart in the article
([`images/bench3_montecarlo.png`](images/bench3_montecarlo.png)).

---

## Why -O0 lies (don't skip this)

Same code, same machine, just `-O0` instead of `-O2`:

- **Benchmark 1** at N = 2048: the row/col gap **collapses from 45.8× (-O2) to
  2.6× (-O0)**. The un-optimized inner loop is so swamped by bookkeeping that the
  memory-system effect — the whole point — nearly vanishes.
- **Benchmark 2** at N = 8: binary search goes from ~6 ns/query (-O2) to ~98
  ns/query (-O0), because the comparator and `lower_bound` internals stop being
  inlined and the linear scan stops vectorizing. Every absolute number is
  garbage.

Reproduce it yourself:

```sh
c++ -O0 -std=c++17 src/bench1_traversal.cpp -o /tmp/b1_O0 && /tmp/b1_O0 2048 3
c++ -O0 -std=c++17 src/bench2_search.cpp    -o /tmp/b2_O0 && /tmp/b2_O0
```

## For real measurement work

These are teaching toys. For anything you'd report seriously, use
[Google Benchmark](https://github.com/google/benchmark): it handles iteration
counts, statistics, `DoNotOptimize`/`ClobberMemory`, and CPU-frequency caveats
properly.

## The article (charts + HTML)

[`article.md`](article.md) is the newsletter draft with these numbers and three
charts. [`article.html`](article.html) is a **single self-contained file**
(CSS + chart images base64-inlined — open it in any browser, e-mail it, host it
anywhere, no assets needed).

To regenerate the charts and rebuild the HTML (needs Python+matplotlib and
pandoc):

```sh
# (optional) refresh the Monte Carlo data the distribution chart reads:
./bin/bench3_list_vs_vector mc 8000 200 > results/montecarlo_bench3.csv
python3 images/make_charts.py        # -> images/bench{1,2,3}_*.png + bench3_montecarlo.png
pandoc article.md -f markdown -t html5 --standalone --embed-resources \
  --css images/article.css --metadata pagetitle="Time Complexity Isn't All You Need" \
  --include-before-body=<(echo '<div class="page">') \
  --include-after-body=<(echo '</div>') -o article.html
```

The chart data in `images/make_charts.py` is hard-coded to the canonical
measured run; if you re-run the benchmarks and want fresh charts, paste your
numbers into that script.

## Files

```
src/bench_common.hpp           timing, sink, stats, trial runner
src/bench1_traversal.cpp       row-major vs column-major
src/bench2_search.cpp          linear vs binary, swept over N
src/bench3_list_vs_vector.cpp  find-and-delete: list vs vector
Makefile                       build / run / clean
README.md                      this file
article.md                     the newsletter article (with these numbers)
article.html                   self-contained rendered article (charts inlined)
images/make_charts.py          regenerates the chart PNGs
images/article.css             styling for article.html
images/bench*.png              the four charts (incl. Monte Carlo distribution)
results/montecarlo_bench3.csv  per-run Monte Carlo data (200 runs)
```
