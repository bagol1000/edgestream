# Benchmarks

Measured on a single Linux x86-64 machine, GCC 13, build flags
`-O3 -std=c++17 -fopenmp`. Numbers are indicative; absolute timings vary with
hardware. Reproduce with `python bench/compare.py` (comparison) and the
snippets at the bottom (micro-benchmarks).

## Streaming comparison vs NetworkX, igraph, rustworkx

Scenario: 100 000 random edges arrive **one at a time** over 50 000 nodes;
after every 1 000 edges the pipeline must answer, about the *current* graph:
number of connected components, global triangle count, and the degree of a
probe node (100 query checkpoints in total). This is the live-monitoring
pattern edgestream is built for.

edgestream answers from its incrementally maintained state; the other
libraries append edges to their native structures and run their
component/triangle routines at each checkpoint — the standard way to use a
static library on a growing graph. Checksums of all answers are compared to
guarantee every candidate computes the same thing.

| library    | time    | vs edgestream | notes |
|------------|---------|---------------|-------|
| edgestream | 0.07 s  | 1x            | all answers O(1) from maintained state |
| rustworkx  | 0.49 s  | 7x            | **components + degree only** — no triangle API, so strictly less work |
| igraph     | 1.40 s  | 21x           | identical answers (checksum-verified) |
| NetworkX   | 16.6 s  | 246x          | identical answers (checksum-verified) |

The gap is structural, not an implementation detail: recompute-on-query costs
O(checkpoints × (n + m)) and grows quadratically with stream length, while
edgestream's total cost is O(m) regardless of how often you query. Denser
checkpointing (query after *every* edge) makes the ratio arbitrarily large —
conversely, if you only query once at the end, use a static library.

This is not a claim that edgestream is "faster than igraph/rustworkx" in
general — their algorithm suites are broader and their static-graph
implementations excellent. It is faster *at being queried while the graph
changes*, which is the niche it exists for.

## Micro-benchmarks (core operations)

| Operation              | Result    | Notes                                   |
|------------------------|-----------|-----------------------------------------|
| `add_edge` (amortised) | ~640 ns   | worst-case uniform-random over 1M nodes, via Python loop |
| `add_edge` directed    | ~780 ns   | adds in-neighbour list + reverse-edge check |
| batch 1M edges (4 threads) | ~0.4 s | `add_edges`, two-phase                  |
| `same_component`       | O(alpha(n)) | effectively O(1) after path compression |
| `n_components`, `degree`, `triangle_count`, `local_triangles`, `max_degree`, `clustering_coefficient` | O(1) | maintained incrementally |
| `remove_edge`          | O(deg)    | + one O(n + m) Union-Find rebuild at the next component query |
| `betweenness_approx` (k=200) | ~3 ms | 1000 nodes / 5000 edges, 4 threads      |

### Notes on `add_edge` throughput

The add_edge figure is measured under a deliberately adversarial workload:
edges with endpoints drawn uniformly at random across 1M nodes, so almost
every operation is a cache miss (random `adj[u]`/`adj[v]`, random Union-Find
parent chains, random edge-set probes). Real streams with node-ID locality
run considerably faster. The in-house Robin Hood `FlatHashSet` (open
addressing, no per-edge allocation) is worth ~45% versus
`std::unordered_set` on this path.

## Reproducing

```python
import numpy as np, time, edgestream as es

# batch ingestion
us = np.random.default_rng(7).integers(0, 1_000_000, 1_000_000, dtype=np.uint32)
vs = np.random.default_rng(8).integers(0, 1_000_000, 1_000_000, dtype=np.uint32)
G = es.StreamGraph(n_nodes=1_000_000)
t = time.perf_counter(); G.add_edges(us, vs, n_threads=4)
print("batch 1M edges:", time.perf_counter() - t, "s")

# approximate betweenness
import random
random.seed(123)
H = es.StreamGraph(n_nodes=1000)
for _ in range(5000):
    u, v = random.randint(0, 999), random.randint(0, 999)
    if u != v:
        H.add_edge(u, v)
H.betweenness_approx(k=10, n_threads=4)              # warm up the thread pool
t = time.perf_counter(); H.betweenness_approx(k=200, n_threads=4, seed=42)
print("betweenness k=200:", time.perf_counter() - t, "s")
```

The comparison harness (`bench/compare.py`) prints per-library times and the
answer checksums; igraph and rustworkx are skipped automatically when not
installed.
