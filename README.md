# edgestream

[![PyPI version](https://img.shields.io/pypi/v/edgestream.svg)](https://pypi.org/project/edgestream/)
[![CI](https://github.com/bagol1000/edgestream/actions/workflows/ci.yml/badge.svg)](https://github.com/bagol1000/edgestream/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Streaming (dynamic) graph analytics with Python and R bindings backed by a
C++17 core — maintain connected components, triangle counts, clustering,
degrees and edge weights incrementally as edges arrive (and leave), and
compute centrality on demand.

Most Python/R graph tools (NetworkX, igraph) assume a static graph: load
everything, run an algorithm, done. `edgestream` instead keeps its metrics up
to date on **every edge**, so you can query a graph that never stops growing.

## Features

- Incremental connected components via Union-Find — `O(alpha(n))` per edge
- Streaming triangle counting and clustering coefficients — `O(1)` to query
- **Edge removal** — triangles, degrees and weights stay exact; components
  rebuild lazily on the next query
- **Edge weights** — `strength()`, weighted PageRank, Dijkstra-based betweenness
- Degree tracking, degree histogram and `O(1)` max degree
- Neighbourhood queries — sorted (in-)neighbours, `O(1)` edge existence
- On-demand centrality: approximate betweenness (sampled, OpenMP-parallel,
  direction- and weight-aware), PageRank, strongly connected components (Tarjan)
- Batch edge ingestion with OpenMP; binary save / load (`.esg`)
- Explicit isolated nodes, snapshots, storage reservation and canonical component IDs
- Automatic timestamp-based sliding windows in Python
- Interop: NetworkX / scipy.sparse (Python), igraph (R), plain edge lists
- `NodeIndex` helper to map arbitrary keys (strings, etc.) to node IDs
- Verified: property-based tests against NetworkX, core fuzzed under
  ASan/UBSan in CI

Node IDs are non-negative integers, **0-indexed in both Python and R**.

## Installation

### Python

```sh
pip install .
```

Requires a C++17 compiler. OpenMP is enabled for parallel batch/centrality
paths; set `EDGESTREAM_NO_OPENMP=1` for a portable serial build.
Build dependencies (`pybind11`, `numpy`) are declared in `pyproject.toml`.

### R

```r
# from the package root
install.packages(".", repos = NULL, type = "source")
# or: R CMD INSTALL .
```

Requires `Rcpp` (LinkingTo) and a C++17 toolchain; R uses OpenMP when the
toolchain exposes it.

## Quick start — fraud-detection style

Watch money flow between accounts and ask, in real time, which accounts are
now connected and how tightly clustered an account is.

```python
import edgestream as es

G = es.StreamGraph(weighted=True)
idx = es.NodeIndex()        # maps account names -> integer node IDs

transactions = [
    ("acc_001", "acc_017", 250.0),
    ("acc_017", "acc_999", 900.0),
    ("acc_999", "acc_001", 120.0),   # closes a triangle 001-017-999
    ("acc_500", "acc_777",  40.0),
]

for sender, receiver, amount in transactions:
    G.add_edge(idx[sender], idx[receiver], amount)

G.same_component(idx["acc_001"], idx["acc_999"])   # True
G.n_components()                                   # 2
G.triangle_count()                                 # 1
G.clustering_coefficient(idx["acc_001"])           # 1.0
G.strength(idx["acc_017"])                         # 1150.0 (money through 017)
```

The same workflow in R:

```r
library(edgestream)

G   <- stream_graph(weighted = TRUE)
idx <- node_index()

add_edge(G, node_id(idx, "acc_001"), node_id(idx, "acc_017"), w = 250)
add_edge(G, node_id(idx, "acc_017"), node_id(idx, "acc_999"), w = 900)
add_edge(G, node_id(idx, "acc_999"), node_id(idx, "acc_001"), w = 120)

same_component(G, get_id(idx, "acc_001"), get_id(idx, "acc_999"))  # TRUE
triangle_count(G)                                                  # 1
strength(G, get_id(idx, "acc_017"))                                # 1150
```

## Usage

All Python examples assume `import numpy as np, edgestream as es`.

### Building and mutating a graph

```python
G = es.StreamGraph()              # auto-growing; StreamGraph(n_nodes=N) pre-allocates
D = es.StreamGraph(directed=True)
W = es.StreamGraph(weighted=True) # positive weight per edge

G.add_edge(0, 1)                  # True  (new edge)
G.add_edge(0, 1)                  # False (duplicate; self-loops also return False)
W.add_edge(0, 1, 2.5)             # duplicates keep the original weight
G.remove_edge(0, 1)               # True if it was present
G.add_node(10)                    # explicit isolated node
W.update_edge_weight(0, 1, 3.5)   # replace an existing weight

# bulk ingestion from uint32 arrays (intra-batch duplicates and self-loops dropped)
us = np.array([0, 1, 2], dtype=np.uint32)
vs = np.array([1, 2, 0], dtype=np.uint32)
G.add_edges(us, vs, n_threads=4)                    # -> 3 new edges
W.add_edges(us, vs, ws=np.array([1.0, 2.0, 3.0]))   # weighted batch
```

`remove_edge` keeps triangles, degrees and weights exact. Union-Find cannot
split, so the next component query after a removal rebuilds it in O(n + m) —
everything else stays incremental. Nodes stay "touched" after losing their
edges (they become singleton components), which is what a sliding-window
stream wants.

### Components

```python
G = es.StreamGraph()
for u, v in [(0, 1), (1, 2), (5, 6)]:
    G.add_edge(u, v)

G.n_components()          # 2   (only touched nodes count)
G.same_component(0, 2)    # True
G.component_id(2)         # 0   (smallest ID in the component)
G.component_size(0)       # 3
G.component_nodes(0)      # array([0, 1, 2], dtype=uint32)
G.component_ids()         # smallest component id per touched node
```

### Triangles and clustering

Counts and the average are maintained incrementally. Queries are O(1) except
directed local clustering, which builds the union of in/out neighbours.

```python
G = es.StreamGraph()
for u, v in [(0, 1), (1, 2), (0, 2), (2, 3), (1, 3)]:
    G.add_edge(u, v)

G.triangle_count()             # 2   (global, each triangle once)
G.local_triangles(1)           # 2
G.all_local_triangles()        # array([1, 2, 2, 1], dtype=uint32)
G.clustering_coefficient(1)    # 0.666...
G.avg_clustering()             # mean over touched nodes
```

### Degrees, weights and statistics

```python
G.degree(0); G.in_degree(0)    # out-/in-degree (equal when undirected)
G.max_degree()                 # O(1), maintained incrementally
G.avg_degree(); G.density()
G.degree_histogram()           # histogram[d] = #nodes of (out-)degree d
W.edge_weight(0, 1)            # stored weight (1.0 on unweighted graphs)
W.strength(0)                  # sum of (out-)edge weights
W.total_weight()
```

### Centrality on demand

```python
G = es.StreamGraph()
for i in range(4):
    G.add_edge(i, i + 1)          # path 0-1-2-3-4

# k random (s,t) pairs (clamped to exact when k >= all pairs); normalised to [0, 1]
G.betweenness_approx(k=200, n_threads=4, seed=42)
# -> array([0.  , 0.75, 1.  , 0.75, 0.  ])

G.betweenness_approx(k=200, seed=42, normalise=False)   # raw estimates
G.pagerank(damping=0.85)                                # sums to 1

D = es.StreamGraph(directed=True)
for u, v in [(0, 1), (1, 2), (2, 0), (2, 3)]:
    D.add_edge(u, v)
D.strong_component_ids()      # array([0, 0, 0, 3]): cycle {0,1,2} + {3}
D.n_strong_components()       # 2
```

Weighted graphs automatically use Dijkstra shortest paths for betweenness and
weight-proportional rank flow for PageRank. Weighted path lengths use a relative
`1e-9` tolerance, so mathematically tied floating-point routes are split fairly
even when their additions occur in a different order.

### Directed graph semantics

- `add_edge(u, v)` stores the ordered edge `u -> v`; the reverse edge is distinct.
- `degree`, `neighbours`, `degree_histogram`, `max_degree` are over
  **out**-edges; `in_degree` / `in_neighbours` cover the incoming side.
- `n_components` etc. are **weakly** connected components (direction
  ignored); use `strong_component_ids` for direction-aware components.
- Triangles are counted on the **underlying undirected graph**, so the count
  is insertion-order independent.
- `betweenness_approx` follows edge direction and sums over ordered pairs.

### Interop

```python
nxg = es.to_networkx(G)              # networkx Graph/DiGraph (weights preserved)
H, idx = es.from_networkx(nxg)       # relabels arbitrary nodes via NodeIndex
S = es.to_scipy_sparse(G)            # scipy.sparse.coo_matrix
us, vs, ws = G.edge_list(weights=True)
```

```r
ig <- as_igraph(G)                   # R: igraph out (weights preserved)
G2 <- from_igraph(ig)
edge_list(G)                         # data.frame(u, v, w)
```

### Serialisation

```python
G.save("graph.esg")                  # portable, atomic EDGS v3
H = es.StreamGraph.load("graph.esg")
```

### Mapping arbitrary keys to node IDs

```python
idx = es.NodeIndex()
G = es.StreamGraph()
G.add_edge(idx["alice"], idx["bob"])
idx["alice"]              # 0   (assigned on first use, stable afterwards)
idx.id("bob")             # 1
idx.key(0)                # 'alice'
```

## Performance

Streaming scenario (100k edges arriving one at a time, components + triangles
+ degree queried every 1000 edges, answers checksum-verified identical):

| | edgestream | rustworkx* | igraph | NetworkX |
|---|---|---|---|---|
| time | **0.04 s** | 0.30 s | 1.08 s | 8.81 s |
| ratio | 1x | 7x | 26x | 216x |

\* rustworkx lacks a triangle-count API, so it answers strictly less.

The gap is structural: recompute-on-query is O(checkpoints × (n + m)), while
edgestream maintains the answers as edges arrive. Methodology, fairness notes
and current micro-benchmarks: [docs/benchmarks.md](docs/benchmarks.md).

## Documentation

- Python: docstrings (`help(es.StreamGraph)`) and type stubs
  (`edgestream/_edgestream.pyi`); guides in [docs/](docs/index.md)
- R: `?add_edge` etc., plus `vignette("edgestream")`
- [NEWS.md](NEWS.md) — changelog

## Status

v0.3: components, triangles, clustering, degrees, weights, explicit nodes,
edge removal, sliding windows, snapshots, portable serialisation and
betweenness/PageRank/SCC on demand. Simple graphs are intentional: parallel
edges and self-loops remain out of scope, as do GPU/distributed backends and
exact incremental betweenness.

## Licence

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 bagol1000.
