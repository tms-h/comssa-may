#!/usr/bin/env python3
"""Generate the three benchmark charts used in article.html.

Numbers are the canonical measured results (Apple M1 Pro, macOS 26.5,
Apple clang 17, -O2) reported in README.md / article.md.
"""
import csv
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

plt.rcParams.update({
    "font.size": 11,
    "font.family": "serif",
    "font.serif": ["Palatino", "Palatino Linotype", "Georgia",
                   "DejaVu Serif"],
    "axes.grid": True,
    "grid.alpha": 0.5,
    "grid.color": "#dddddd",
    "figure.dpi": 300,
    "figure.facecolor": "#ffffff",
    "axes.facecolor": "#ffffff",
    "savefig.bbox": "tight",
    "savefig.facecolor": "#ffffff",
})

INK = "#211d17"   # warm near-black (titles, text)
ROW = "#14756a"   # deep teal   (fast / good access pattern)
COL = "#c2491d"   # burnt orange (slow)
EMPH = "#a01f1f"  # deep red for the "x slower" callouts
LIN = "#5e4b8b"   # plum        (linear)
BIN = "#cf3a7d"   # magenta     (binary)

# --- Benchmark 1: row-major vs column-major -------------------------------
def bench1():
    fig, ax = plt.subplots(figsize=(6.6, 4.0))
    labels = ["row-major\n(stride 1)", "column-major\n(stride N)"]
    vals = [5.06, 347.83]
    bars = ax.bar(labels, vals, color=[ROW, COL], width=0.55, zorder=3)
    ax.set_yscale("log")
    ax.set_ylabel("time to sum 256 MiB array (ms, log scale)")
    ax.set_title("Benchmark 1: same loop, access order only (N=8192)",
                 color=INK, fontweight="bold")
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v * 1.08, f"{v:.1f} ms",
                ha="center", va="bottom", fontweight="bold")
    ax.text(0.27, 0.6, "68.7×\nslower", transform=ax.transAxes,
            ha="center", color=EMPH, fontsize=15, fontweight="bold")
    fig.savefig("images/bench1_traversal.png")
    plt.close(fig)

# --- Benchmark 2: linear vs binary search sweep ---------------------------
def bench2():
    N   = [8, 16, 32, 64, 128, 256, 512, 1024, 4096, 16384, 65536]
    lin = [4.54, 1.95, 2.67, 4.09, 7.38, 13.97, 27.01, 53.67, 215.42,
           843.55, 4028.54]
    bn  = [6.02, 5.90, 6.47, 7.46, 8.92, 10.40, 12.30, 14.28, 18.50,
           23.33, 42.10]
    fig, ax = plt.subplots(figsize=(7.2, 4.4))
    ax.plot(N, lin, "-o", color=LIN, label="linear scan  O(n)", zorder=3)
    ax.plot(N, bn, "-s", color=BIN, label="binary search  O(log n)", zorder=3)
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(ScalarFormatter())
    ax.set_xticks(N)
    ax.set_xticklabels([str(n) for n in N], rotation=45, fontsize=8)
    ax.set_xlabel("array size N (log scale)")
    ax.set_ylabel("time per query (ns, log scale)")
    ax.set_title("Benchmark 2: linear beats binary below the crossover",
                 color=INK, fontweight="bold")
    # crossover band between 128 and 256
    ax.axvspan(128, 256, color="#888", alpha=0.12, zorder=0)
    ax.axvline(256, color="#555", ls="--", lw=1, zorder=1)
    ax.annotate("crossover ≈ 256",
                xy=(256, 10.4), xytext=(900, 4.2),
                arrowprops=dict(arrowstyle="->", color="#555"),
                fontsize=10, color="#333")
    ax.annotate("linear wins", xy=(20, 2.4), color=LIN, fontsize=10,
                fontweight="bold")
    ax.annotate("binary wins", xy=(9000, 30), color=BIN, fontsize=10,
                fontweight="bold")
    ax.legend(loc="upper left", framealpha=0.9)
    fig.savefig("images/bench2_search.png")
    plt.close(fig)

# --- Benchmark 3: list vs vector find-and-delete --------------------------
def bench3():
    fig, ax = plt.subplots(figsize=(6.6, 4.0))
    labels = ["std::vector\n(contiguous)", "std::list\n(pointer nodes)"]
    vals = [49.33, 242.14]
    bars = ax.bar(labels, vals, color=[ROW, COL], width=0.55, zorder=3)
    ax.set_ylabel("find-and-delete all 20,000 elements (ms)")
    ax.set_title("Benchmark 3: the cost of finding the node (find + erase)",
                 color=INK, fontweight="bold")
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 4, f"{v:.1f} ms",
                ha="center", va="bottom", fontweight="bold")
    ax.set_ylim(0, vals[1] * 1.18)
    ax.text(0.27, 0.62, "list\n4.9× slower", transform=ax.transAxes,
            ha="center", color=EMPH, fontsize=14, fontweight="bold")
    fig.savefig("images/bench3_list_vs_vector.png")
    plt.close(fig)

# --- Benchmark 3 Monte Carlo: distribution over random instances ----------
def bench3_montecarlo(csv_path="results/montecarlo_bench3.csv"):
    if not os.path.exists(csv_path):
        print(f"skip Monte Carlo chart: {csv_path} not found "
              "(run: ./bin/bench3_list_vs_vector mc 8000 200 > ...)")
        return
    vec, lst = [], []
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            (vec if row["container"] == "vector" else lst).append(
                float(row["ms"]))
    runs = min(len(vec), len(lst))
    wins = sum(1 for v, l in zip(vec, lst) if v < l)

    import statistics as st
    vmed, lmed = st.median(vec), st.median(lst)

    fig, ax = plt.subplots(figsize=(7.4, 4.5))
    lo = min(min(vec), min(lst))
    hi = max(max(vec), max(lst))
    bins = [lo + (hi - lo) * i / 45 for i in range(46)]
    ax.hist(vec, bins=bins, color=ROW, alpha=0.78,
            label=f"std::vector  (median {vmed:.1f} ms)", zorder=3)
    ax.hist(lst, bins=bins, color=COL, alpha=0.70,
            label=f"std::list  (median {lmed:.1f} ms)", zorder=3)
    ax.axvline(vmed, color=ROW, ls="--", lw=1.4, zorder=4)
    ax.axvline(lmed, color=COL, ls="--", lw=1.4, zorder=4)
    # individual runs as a rug along the bottom
    ax.plot(vec, [-1.2] * len(vec), "|", color=ROW, alpha=0.5, ms=8)
    ax.plot(lst, [-1.2] * len(lst), "|", color=COL, alpha=0.5, ms=8)

    ax.set_xlabel("time for one find-and-delete pass (ms)")
    ax.set_ylabel(f"number of runs (of {runs})")
    ax.set_title("Monte Carlo: vector vs list over random instances",
                 color=INK, fontweight="bold")
    ax.legend(loc="upper right", framealpha=0.92)
    gap_lo, gap_hi = max(vec), min(lst)
    if gap_hi > gap_lo:  # the distributions don't overlap -> shade the gap
        ax.axvspan(gap_lo, gap_hi, color="#888", alpha=0.10, zorder=0)
    ax.text(0.5, 0.55,
            f"vector wins\n{wins}/{runs} runs",
            transform=ax.transAxes, ha="center", color=INK, fontsize=12,
            fontweight="bold")
    fig.savefig("images/bench3_montecarlo.png")
    plt.close(fig)

if __name__ == "__main__":
    bench1()
    bench2()
    bench3()
    bench3_montecarlo()
    print("wrote images/bench1_traversal.png, bench2_search.png, "
          "bench3_list_vs_vector.png, bench3_montecarlo.png")
