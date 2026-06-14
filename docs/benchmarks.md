# Benchmarks

Measured on a single Linux x86-64 machine, GCC 13, build flags
`-O3 -std=c++17 -fopenmp`. Numbers are indicative; absolute timings vary with
hardware. All figures are for **undirected** graphs.

| Operation              | Result    | Notes                                   |
|------------------------|-----------|-----------------------------------------|
| `add_edge` (amortised) | 637 ns    | worst-case uniform-random over 1M nodes |
| batch 1M edges (4 threads) | 0.59 s | `add_edges`, two-phase                   |
| `same_component`       | O(alpha(n)) | effectively O(1) after path compression |
| `component_id`         | O(alpha(n)) | effectively O(1)                       |
| `n_components`         | O(1)      | maintained incrementally                |
| `degree`               | O(1)      | maintained incrementally                |
| `triangle_count` (global) | O(1)   | maintained incrementally                |
| `local_triangles`      | O(1)      | maintained incrementally                |
| `betweenness_approx` (k=200) | 0.078 s | 1000 nodes, 5000 edges, 4 threads   |

## Notes on `add_edge` throughput

The 637 ns/edge figure is the **C++ core** under a deliberately adversarial
workload: 10M edges with endpoints drawn uniformly at random across 1M nodes, so
almost every operation is a cache miss (random `adj[u]`/`adj[v]`, random
Union-Find parent chains, random edge-set probes). It is measured with the
shipped flags (no `-march=native`, which made no measurable difference — the
bottleneck is memory latency, not compute).

Switching the edge set from `std::unordered_set` to the in-house Robin Hood
`FlatHashSet` cut this from ~1168 ns to ~637 ns (about 45%) by removing a heap
allocation per edge. The residual gap to the spec's aspirational 500 ns target
is dominated by sorted-adjacency insertion (`memmove`) plus random cache misses
inherent to this access pattern; real streams with node-ID locality run
considerably faster.

Calling `add_edge` 10M times from a Python loop measures ~1205 ns/edge; the
difference from the core figure is per-call pybind11/interpreter overhead, which
`add_edges` avoids for bulk ingestion.

## Reproducing

```python
import numpy as np, time, streamgraph as sg

#batch ingestion
us = np.random.default_rng(7).integers(0, 1_000_000, 1_000_000, dtype=np.uint32)
vs = np.random.default_rng(8).integers(0, 1_000_000, 1_000_000, dtype=np.uint32)
G = sg.StreamGraph(n_nodes=1_000_000)
t = time.perf_counter(); G.add_edges(us, vs, n_threads=4)
print("batch 1M edges:", time.perf_counter() - t, "s")

#approximate betweenness
import random
random.seed(123)
H = sg.StreamGraph(n_nodes=1000)
for _ in range(5000):
    u, v = random.randint(0, 999), random.randint(0, 999)
    if u != v:
        H.add_edge(u, v)
t = time.perf_counter(); H.betweenness_approx(k=200, n_threads=4, seed=42)
print("betweenness k=200:", time.perf_counter() - t, "s")
```
