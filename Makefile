# Makefile for the "Time Complexity Isn't All You Need" benchmarks.
#
# Normal usage (most machines, with a working clang++/g++ in PATH):
#     make            # builds all three at -O2
#     make OPT=-O3    # try -O3
#     make NATIVE=1   # add the right -march/-mcpu=native flag for this CPU
#     make run        # build + run all three
#     make clean
#
# IMPORTANT: these benchmarks are only meaningful at -O2 or higher. A default
# /debug / -O0 build will produce numbers that tell you nothing about real
# hardware behaviour (no vectorization, no inlining, spills everywhere).
#
# --- Overriding the compiler -------------------------------------------------
# If `make` fails because your toolchain can't find headers/SDK (e.g. a macOS
# box where the full-Xcode license hasn't been accepted), point CXX and CXXFLAGS
# at a working toolchain, e.g. the standalone Command Line Tools:
#     make CXX=/Library/Developer/CommandLineTools/usr/bin/clang++ \
#          SYSROOT="-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
#                   -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1"
# (The published numbers in README.md / article.md were produced exactly this
#  way on an Apple M1 Pro — see the README for the full command.)

CXX      ?= c++
OPT      ?= -O2
STD      ?= -std=c++17
WARN     ?= -Wall -Wextra
SYSROOT  ?=
EXTRA    ?=

# NATIVE=1 adds the architecture-tuning flag appropriate to this CPU.
UNAME_M := $(shell uname -m)
ifeq ($(NATIVE),1)
  ifneq (,$(filter arm64 aarch64,$(UNAME_M)))
    EXTRA += -mcpu=native      # Apple/ARM clang uses -mcpu, not -march
  else
    EXTRA += -march=native
  endif
endif

CXXFLAGS := $(OPT) $(STD) $(WARN) $(SYSROOT) $(EXTRA)

BINDIR := bin
BINS   := $(BINDIR)/bench1_traversal \
          $(BINDIR)/bench2_search \
          $(BINDIR)/bench3_list_vs_vector

.PHONY: all run clean
all: $(BINS)

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BINDIR)/%: src/%.cpp src/bench_common.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	@echo "===================================================="
	@$(BINDIR)/bench1_traversal
	@echo "===================================================="
	@$(BINDIR)/bench2_search
	@echo "===================================================="
	@$(BINDIR)/bench3_list_vs_vector

clean:
	rm -rf $(BINDIR)
