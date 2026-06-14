# streamgraph

[![PyPI version](https://img.shields.io/pypi/v/streamgraph.svg)](https://pypi.org/project/streamgraph/)
[![CI](https://github.com/bagol1000/streamgraph/actions/workflows/ci.yml/badge.svg)](https://github.com/bagol1000/streamgraph/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Streaming (dynamic) graph analytics with Python and R bindings backed by a
C++17 core — maintain connected components, triangle counts, degrees and basic
centrality incrementally as edges arrive.

Most Python/R graph tools (NetworkX, igraph) assume a static graph: load
everything, run an algorithm, done. `streamgraph` instead keeps its metrics up
to date on every edge, so you can query a graph that never stops growing.

## Features

- Incremental connected components via Union-Find — `O(alpha(n))` per edge
- Streaming triangle counting — global and per-node, `O(1)` to query
- Degree tracking and degree histogram — `O(1)` per edge
- Neighbourhood queries — sorted neighbours, `O(1)` edge existence
- Approximate betweenness centrality — random pair sampling, OpenMP-parallel
- Batch edge ingestion with OpenMP
- Binary save / load
- `NodeIndex` helper to map arbitrary keys (strings, etc.) to node IDs

Node IDs are non-negative integers, **0-indexed in both Python and R**.

## Installation

### Python

```sh
pip install .
```

Requires a C++17 compiler with OpenMP (build flags: `-O3 -std=c++17 -fopenmp`).
Build dependencies (`pybind11`, `numpy`) are declared in `pyproject.toml`.

### R

```r
# from the package root
install.packages(".", repos = NULL, type = "source")
# or: R CMD INSTALL .
```

Requires `Rcpp` (LinkingTo) and a C++17 toolchain with OpenMP.

## Quick start — fraud-detection style

A common streaming use case: watch money flow between accounts and ask, in real
time, which accounts are now connected and how tightly clustered an account is.

```python
import streamgraph as sg

G = sg.StreamGraph()
idx = sg.NodeIndex()        # maps account names -> integer node IDs

transactions = [
    ("acc_001", "acc_017"),
    ("acc_017", "acc_999"),
    ("acc_999", "acc_001"),   # closes a triangle 001-017-999
    ("acc_500", "acc_777"),
]

for sender, receiver in transactions:
    G.add_edge(idx[sender], idx[receiver])

# Are two accounts part of the same money-flow cluster?
G.same_component(idx["acc_001"], idx["acc_999"])   # True
G.same_component(idx["acc_001"], idx["acc_500"])   # False

# How tightly is an account clustered? (triangles it participates in)
G.local_triangles(idx["acc_001"])                  # 1

# Cluster-level stats
G.n_components()                                    # 2
G.component_size(idx["acc_001"])                    # 3
G.triangle_count()                                  # 1
```

The same workflow in R:

```r
library(streamgraph)

G   <- stream_graph()
idx <- node_index()

add_edge(G, node_id(idx, "acc_001"), node_id(idx, "acc_017"))
add_edge(G, node_id(idx, "acc_017"), node_id(idx, "acc_999"))
add_edge(G, node_id(idx, "acc_999"), node_id(idx, "acc_001"))

same_component(G, get_id(idx, "acc_001"), get_id(idx, "acc_999"))  # TRUE
local_triangles(G, get_id(idx, "acc_001"))                        # 1
triangle_count(G)                                                 # 1
```

## Usage

All Python examples assume `import numpy as np, streamgraph as sg`. Node IDs are
non-negative integers, 0-indexed.

### Building a graph

```python
G = sg.StreamGraph()              # auto-growing; or StreamGraph(n_nodes=N) to pre-allocate
G.add_edge(0, 1)                  # True  (new edge)
G.add_edge(0, 1)                  # False (duplicate; self-loops also return False)

# bulk ingestion from uint32 arrays (intra-batch duplicates and self-loops dropped)
us = np.array([0, 1, 2], dtype=np.uint32)
vs = np.array([1, 2, 0], dtype=np.uint32)
G2 = sg.StreamGraph()
G2.add_edges(us, vs, n_threads=4)   # -> 3 new edges added
```

Pass `directed=True` to `StreamGraph` for a directed graph.

### Connected components

```python
G = sg.StreamGraph()
for u, v in [(0, 1), (1, 2), (5, 6)]:
    G.add_edge(u, v)

G.n_components()          # 2   (only touched nodes count)
G.same_component(0, 2)    # True
G.same_component(0, 5)    # False
G.component_id(2)         # 0   (root id of the component)
G.component_size(0)       # 3
G.component_nodes(0)      # array([0, 1, 2], dtype=uint32)
G.component_ids()         # root id per touched node
```

### Triangles

Maintained incrementally on every `add_edge`; queries are O(1).

```python
G = sg.StreamGraph()
for u, v in [(0, 1), (1, 2), (0, 2), (2, 3), (1, 3)]:
    G.add_edge(u, v)

G.triangle_count()        # 2   (global, each triangle once)
G.local_triangles(1)      # 2
G.all_local_triangles()   # array([1, 2, 2, 1], dtype=uint32)
```

### Degrees and statistics

```python
G = sg.StreamGraph()
for i in range(4):
    G.add_edge(0, i + 1)          # a star centred on node 0

G.degree(0)               # 4
G.max_degree()            # 4
G.avg_degree()            # 1.6
G.density()               # 0.4
G.degree_histogram()      # array([0, 4, 0, 0, 1])  histogram[d] = #nodes of degree d
G.n_nodes(), G.n_edges()  # (5, 4)
```

### Neighbours and edge lookup

```python
G = sg.StreamGraph()
for v in (8, 2, 5):
    G.add_edge(1, v)

G.neighbours(1)           # array([2, 5, 8], dtype=uint32)  (sorted)
G.has_edge(1, 8)          # True
```

### Approximate betweenness

```python
G = sg.StreamGraph()
for i in range(4):
    G.add_edge(i, i + 1)          # path 0-1-2-3-4

# k random (s,t) pairs (clamped to exact when k >= all pairs); normalised to [0, 1]
G.betweenness_approx(k=200, n_threads=4, seed=42)
# -> array([0.  , 0.75, 1.  , 0.75, 0.  ])   middle node highest, endpoints zero
```

### Serialisation

```python
G.save("graph.sgph")
H = sg.StreamGraph.load("graph.sgph")
H.n_edges() == G.n_edges()        # True (edges, components and triangles restored)
```

### Mapping arbitrary keys to node IDs

`NodeIndex` (pure Python) turns strings or any hashable key into contiguous IDs:

```python
idx = sg.NodeIndex()
G = sg.StreamGraph()
G.add_edge(idx["alice"], idx["bob"])
G.add_edge(idx["bob"], idx["carol"])

idx["alice"]              # 0   (assigned on first use, stable afterwards)
idx.id("bob")             # 1
idx.key(2)                # 'carol'
"alice" in idx, idx.n_nodes()   # (True, 3)
```

### R

The R API mirrors Python (also 0-indexed); pass the graph object first:

```r
library(streamgraph)
G <- stream_graph()
for (i in 0:4) add_edge(G, i, i + 1L)

n_components(G)                       # 1
triangle_count(G)
betweenness_approx(G, k = 200L, seed = 42L)

us <- c(0L, 1L, 2L); vs <- c(1L, 2L, 0L)
H <- stream_graph(); add_edges(H, us, vs, n_threads = 4L)

p <- tempfile(fileext = ".sgph")
save_graph(G, p); G2 <- load_graph(p)

idx <- node_index()
add_edge(G, node_id(idx, "alice"), node_id(idx, "bob"))
```

## Performance

See [docs/benchmarks.md](docs/benchmarks.md). Highlights (single machine):
`add_edge` ~637 ns amortised (worst-case uniform-random), batch 1M edges in
~0.59 s on 4 threads, approximate betweenness (k=200) over 1000 nodes / 5000
edges in ~0.078 s.

## Documentation

- Python: numpy-style docstrings (`help(sg.StreamGraph.add_edge)`) and type
  stubs in `streamgraph/_streamgraph.pyi`
- R: `?add_edge`, `?betweenness_approx`, etc. (roxygen2-generated man pages)
- [Benchmarks & methodology](docs/benchmarks.md)

## Status

Edge addition only (no deletion in v1). Triangle counting, components, degrees,
statistics, batch ingestion, serialisation and approximate betweenness are
implemented and tested. Out of scope for v1: edge deletion, weighted edges,
community detection, PageRank, exact incremental betweenness, GPU/distributed
backends.

## Licence

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 bagol1000.
