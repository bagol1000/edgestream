# edgestream

Streaming (dynamic) graph analytics with Python and R bindings backed by a
C++17 core — maintain connected components, triangle counts, clustering,
degrees and weights incrementally as edges arrive, and compute centrality
(betweenness, PageRank, SCC) on demand.

Most graph tools (NetworkX, igraph) assume a static graph: load everything,
run an algorithm, done. edgestream keeps its metrics up to date on **every
edge**, so you can query a graph that never stops growing — and remove edges
again when your stream has a sliding window.

## Why edgestream

- **O(1) answers while streaming.** Components (Union-Find), triangles,
  degrees, clustering and weight totals update on each `add_edge` /
  `remove_edge`; queries never trigger recomputation (the only exception:
  the first component query after a removal rebuilds Union-Find once).
- **One core, two ecosystems.** The same C++17 engine drives a pybind11
  module and an Rcpp package with mirrored APIs.
- **Plays well with others.** Convert to/from NetworkX, scipy.sparse and
  igraph whenever you need an algorithm edgestream does not have.
- **Verified.** Property-based tests compare every metric against NetworkX on
  random graphs; the core is fuzzed under AddressSanitizer/UBSan in CI.

## Install

```sh
pip install edgestream        # Python (from source until wheels are published)
```

```r
# R, from the repository root
R CMD INSTALL .
```

Both need a C++17 compiler with OpenMP.

## 60-second tour (Python)

```python
import edgestream as es

G = es.StreamGraph()
idx = es.NodeIndex()

for sender, receiver in [("a", "b"), ("b", "c"), ("c", "a")]:
    G.add_edge(idx[sender], idx[receiver])

G.n_components()                 # 1
G.triangle_count()               # 1
G.clustering_coefficient(idx["a"])   # 1.0
G.pagerank()                     # on-demand centrality
```

See the [Python guide](python.md) and [R guide](r.md) for the full API, and
[Benchmarks](benchmarks.md) for methodology and numbers against NetworkX,
igraph and rustworkx.
