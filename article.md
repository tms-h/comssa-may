# Time Complexity Isn't All You Need

So here are two ways to sum the same array. The arithmetic is completely
identical, and both of them are O(n²). And yet, on my laptop, one of them runs
about 70 times slower than the other. Nothing about the algorithm itself changed
at all. The only thing that's actually different between them is the order the
elements get read in.

Big-O is the tool your university course preaches for choosing between your data
structures and algorithms, and the rule it gives you is dead simple: the much
lower asymptotic cost always wins. And that general idea isn't wrong, exactly,
but it's more so describing an idealised machine, a sort of perfect world where
every memory access costs exactly the same and branches are basically free. In
reality, no such machine actually exists. In real life, the way a CPU is
physically built matters a whole lot, and sometimes it's even the entire problem.
Memory is laid out in layers, with little fast caches sitting right up against the
core and the big slow main memory parked much further away, and on top of that
the hardware spends a lot of its time just guessing what you're going to do next.
So those constant factors that Big-O so happily throws away are very often the
things that actually end up deciding which option is faster at the sizes you're
realistically going to run in practice.

Anyway, I've got three little experiments for you below. Every single number here
was measured on the one machine, and the code is all sitting up at
[github.com/tms-h/comssa-may](https://github.com/tms-h/comssa-may) so you can just
run it yourself if you want. One warning before you do, though: make sure you
compile with optimisations turned on. At `-O0` the numbers basically fall apart
and none of this really holds.

## Experiment 1: same loop, two orders

Okay so take a 256 MB grid of ints and sum up every element. The two loops below
both do exactly that, and honestly the only thing that changes between them is
which index ends up sitting on the inner loop.

```cpp
// Version A: inner loop runs over j (the contiguous direction)
long long sum = 0;
for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
        sum += a[i * N + j];
```

```cpp
// Version B: inner loop runs over i (jumps N ints each step)
long long sum = 0;
for (int j = 0; j < N; ++j)
    for (int i = 0; i < N; ++i)
        sum += a[i * N + j];
```

Both of them touch every single element once, and both do the exact same
additions along the way. So before you scroll on down: is there even a difference
here at all? Reading across the rows or down the columns is surely the same amount
of work either way, and this is all probably just going to be noise, right?

Well, here's what actually happened.

| version | inner loop | time |
|---|---|---:|
| A (row-major) | `j` varies fastest | **5.06 ms** |
| B (column-major) | `i` varies fastest | **347.83 ms** |

![Bar chart on a log scale. Version A (row-major) takes about 5 ms and Version B (column-major) takes about 348 ms for the identical sum over a 256 MB array.](images/bench1_traversal.png)

A took 5.06 ms. B took a whopping 347.83 ms, for the exact same sum. That's about
70 times slower. And the reason really comes down to this: memory doesn't just
come back to you one int at a time. It actually arrives in 64-byte chunks called
cache lines, which works out to roughly sixteen ints each. Version A reads
straight along a row, so it ends up using all sixteen ints in a line before it
ever has to move on, and the hardware notices that nice steady forward march and
starts grabbing the next line early for you. Version B, on the other hand, reads
down a column, so it uses a single int out of a line and then immediately jumps
far enough away that the next read just misses the cache all over again. So almost
every read in B ends up basically sitting there, twiddling its thumbs, waiting on
main memory.

Same Big-O, both of them O(n²), and yet one is 70 times faster than the other.
The thing doing all the actual work here is the exact thing school never quite
gets around to mentioning.

## Experiment 2: when linear search wins

Right, here's another one. Binary search halves a sorted array on each step and
runs in O(log n), while a plain linear scan just checks the elements one at a
time, so it's O(n). And since log(n) < n for every n > 0, well, binary search
should obviously win at every single size, right? To actually check that, I swept
the array size N and ran a whole pile of random lookups against a sorted array,
timing each method per query.

| N | linear (ns/query) | binary (ns/query) | winner |
|---:|---:|---:|:--|
| 8 | 4.54 | 6.02 | **linear** |
| 64 | 4.09 | 7.46 | **linear** |
| 128 | 7.38 | 8.92 | **linear** |
| 256 | 13.97 | 10.40 | binary |
| 1024 | 53.67 | 14.28 | binary |
| 65536 | 4028.54 | 42.10 | binary |

![Log-log line chart of time per query against array size N. The linear-scan line is lower for small N, the two lines cross near N equals 256, and the linear line then rises far above binary search.](images/bench2_search.png)

Well, huh, that's a bit surprising. The linear scan is actually faster all the way
up to about 128 elements, and 128 is honestly not a small array at all. Plenty of
real lookups happen in arrays smaller than that, which means linear is quietly
winning across most of the everyday cases you're ever actually going to hit. The
two methods only cross over somewhere around N = 256 here, and past that point
binary search just runs away with it.

The reason is a pretty short one, actually. A small array sits entirely inside the
fast cache, so the scan is really just one tight little loop that the CPU rips
straight through. Binary search, on the other hand, jumps off to a spot that
depends on the comparison it just made, so the branch predictor can't really
guess where it's going to land, and the pipeline ends up stalling on basically
every step. It's only once the array gets properly large that those few clever
jumps start actually beating the cost of scanning through thousands of elements.

So yeah, a worse Big-O beat a better one. O(n) came out ahead of O(log n) over
pretty much the entire range a hobby project is ever realistically going to touch,
which is just about the exact opposite of what the proof tells you to expect.

## Experiment 3: the cost of finding the node

This is the one that surprised me the most, honestly. Deleting from a linked list
is O(1) once you're already holding the node, while deleting from an array is
O(n), because everything sitting after it has to shift down to fill the gap. So
run 20,000 deletes and the list does at most 20,000 nice cheap splices, while the
vector might have to shift nearly 20,000 elements on every single delete. On
paper, the vector is clearly doing orders of magnitude more work. But the catch
the textbook example always quietly skips right over is that you actually have to
find the element first before you can go and delete it. So the workload here walks
from the front to find a value, erases it, and then just repeats until the whole
container is empty.

| container | time |
|---|---:|
| `std::vector` | **49.33 ms** |
| `std::list` | **242.14 ms** |

![Bar chart. Finding and deleting all 20,000 elements takes about 49 ms with std::vector and about 242 ms with std::list.](images/bench3_list_vs_vector.png)

The vector took 49.33 ms and the list took 242.14 ms, so the list comes out about
five times slower, despite having the supposedly cheaper delete. Finding the node
is what really costs you. A list scatters all its nodes across the heap and links
them together with pointers, so searching through it means following those
pointers all over the place in memory, eating a fresh cache miss at nearly every
single node. The vector, meanwhile, just keeps everything packed into one
contiguous block, so the search is that same fast scan we already saw back in
Experiment 1, and the actual delete is just one bulk `memmove`.

This is basically the result Bjarne Stroustrup is famous for: arrays beat linked
lists for most everyday work. And you can really feel your intuition pulling the
other way hard on this one. Think about an order book, say, the live list of buy
and sell orders in a market. Orders get cancelled constantly, so a linked list
looks just about perfect for the job: O(1) to splice one right out. And yet plenty
of seriously fast order books keep their orders in contiguous arrays anyway,
because walking pointers around to find the order you want to cancel ends up
costing way more than all that array shuffling ever does.

Of course, one run could just be luck, so here's the same workload run 200 times
over, each one with a fresh random shuffle and delete order, timing a single pass
each time.

![Overlaid histogram of 200 random runs. std::vector times cluster tightly near 8 ms while std::list times spread into a long tail past 100 ms, and vector is faster in every run.](images/bench3_montecarlo.png)

The vector won all 200 runs. But the shapes are the really interesting part here.
The vector's times all bunch up nice and tight near 8 ms, while the list's spread
right out into a long tail that reaches well past 100 ms, basically because how
scattered the nodes end up is a bit different from one run to the next. And a tail
like that is exactly how a list quietly turns into a random latency spike in
production, long after the Big-O up on the whiteboard looked perfectly fine.

## What on earth is happening?

So, a handful of hardware details end up explaining all three of these results.

- **The cache hierarchy.** Memory isn't just one big flat pool. Your CPU has a
  few tiny, really fast caches sitting right up next to it, and then a big, slow
  main memory parked much further out. Reading from the nearest cache takes about
  a nanosecond; reaching all the way out to main memory takes more like a hundred.
  Data you used recently, or data sitting right beside it, lives up in the fast
  tier. Everything else just makes you wait.
- **Prefetching.** The CPU is always watching the way you read memory. When you
  move through it in a nice straight line, it starts fetching the chunks ahead of
  you before you've even asked for them. So walk forward through an array and it
  keeps pace with you for free. Jump around all over the place, though, like a
  column walk or a pointer chase, and it has no real way to guess what you'll need
  next.
- **Branch prediction.** The CPU doesn't just sit there and wait to learn which
  way an `if` is going to go. It guesses, and it runs on ahead anyway. Guess right
  and the branch was very nearly free; guess wrong, which is exactly what binary
  search's data-dependent jumps keep causing, and it has to throw away all the
  work it already started and just begin again.
- **The pattern.** In all three experiments, contiguous memory read in order beat
  the cleverer structure with the better complexity on paper. So reach for a plain
  flat array first, and only move over to pointers once you've actually gone and
  measured a real reason to.

Big-O is still the right way to think about how cost grows as your inputs get big,
and at a large enough N it does always win out in the end. The catch is just that
courses tend to treat that ranking as the final answer, when on real hardware it's
honestly a lot closer to the starting point. The real trick is to hold both ideas
in your head at the same time: Big-O tells you how things scale, and the cache and
the pipeline are the ones that actually decide who's faster at the size you happen
to be running today. And when it really matters, just measure it, with an
optimised build, a warm-up, a few runs, and the minimum taken. The two loops at
the very top of this page were only a few characters apart, and 70 times apart in
speed. Once you start actually measuring, you start finding that gap absolutely
everywhere.

## Further reading

- **Bjarne Stroustrup**, who created C++. His list versus vector demonstration is
  the direct source of Experiment 3, and honestly the fastest way to just see the
  effect for yourself.
- **Scott Meyers**. His book *Effective C++* is the classic next step in the
  language, and he's also got a talk on CPU caches that lines up really nicely with
  Experiments 1 and 2.
- **Mike Acton**. His CppCon talk on data-oriented design pushes this whole idea
  much, much further, building entire programs around how the data actually sits
  in memory.
- **Google Benchmark**, which is the library you'd actually reach for to measure
  this stuff properly, rather than the little harness I've thrown together here in
  this repo.
