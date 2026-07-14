# Python guide

Everything lives on the `StreamGraph` class; `NodeIndex` maps arbitrary
hashable keys to 0-indexed integer node ids.

```python
import numpy as np
import edgestream as es
```

## Building and mutating

```python
G = es.StreamGraph()                       # auto-growing, undirected, unweighted
D = es.StreamGraph(directed=True)
W = es.StreamGraph(weighted=True)          # positive weight per edge
P = es.StreamGraph(n_nodes=1_000_000)      # pre-allocate when the bound is known

G.add_edge(0, 1)          # True (new), False (duplicate / self-loop)
W.add_edge(0, 1, 2.5)     # duplicates keep the original weight
G.remove_edge(0, 1)       # True if it was present
G.add_node(10)            # explicit isolated node
G.nodes()                 # sorted touched IDs
W.update_edge_weight(0, 1, 4.0)
snapshot = G.copy()       # independent graph

us = np.array([0, 1, 2], dtype=np.uint32)  # batch ingestion (OpenMP dedup)
vs = np.array([1, 2, 0], dtype=np.uint32)
G.add_edges(us, vs, n_threads=4)
W.add_edges(us, vs, ws=np.array([1.0, 2.0, 3.0]))
```

`remove_edge` keeps triangles, degrees and weights exact; the first component
query after a removal rebuilds Union-Find in O(n + m). Nodes stay "touched"
after losing all edges (they become singleton components).

## Queries

```python
G.n_nodes(); G.n_edges(); G.density(); G.avg_degree(); G.max_degree()
G.degree(u); G.in_degree(u)               # in_* = out for undirected
G.neighbours(u); G.in_neighbours(u)       # sorted uint32 arrays
G.has_edge(u, v); G.edge_weight(u, v); G.strength(u); G.total_weight()
G.degree_histogram()                      # histogram[d] = #nodes of degree d

G.same_component(u, v); G.component_id(u); G.n_components()
G.component_size(u); G.component_nodes(u); G.component_ids()

G.triangle_count(); G.local_triangles(u); G.all_local_triangles()
G.clustering_coefficient(u); G.avg_clustering()
```

Scalar maintained metrics are O(1), while methods returning arrays necessarily
copy their output. Neighbour queries cost O(degree), component listings cost
O(n_ids), and the first component query after removals rebuilds DSU in O(n+m).
Directed local clustering builds the union of in/out neighbours. See the
[complexity table](benchmarks.md#query-complexity).

## Sliding windows

```python
W = es.SlidingWindowGraph(window=60.0, directed=True, weighted=True)
W.add_edge(0, 1, timestamp=100.0, w=25.0)
W.add_edge(1, 2, timestamp=130.0, w=10.0)
W.advance(161.0)       # expires the edge observed at t=100
snapshot = W.snapshot()
```

Timestamps must be finite and non-decreasing. Re-observing an edge refreshes
its expiry time and replaces its weight in a weighted window.

## On-demand algorithms

```python
G.betweenness_approx(k=200, n_threads=4, seed=42)   # sampled, [0, 1]
G.betweenness_approx(k=10**9, normalise=False)      # exact raw values
G.pagerank(damping=0.85)                            # sums to 1
G.strong_component_ids(); G.n_strong_components()   # Tarjan (directed)
```

Weighted graphs use Dijkstra for betweenness and weight-proportional PageRank
automatically. Dijkstra compares path lengths with a relative `1e-9` tolerance,
avoiding order-dependent treatment of mathematically tied floating-point routes.
All weights must be finite and strictly positive.

## Directed semantics

`degree`/`neighbours`/`degree_histogram`/`max_degree` are over **out**-edges;
components are **weakly** connected (use `strong_component_ids` for
direction-aware); triangles are counted on the underlying undirected graph,
so the count does not depend on insertion order.

## Interop and persistence

```python
nxg = es.to_networkx(G)                    # networkx Graph/DiGraph
H, idx = es.from_networkx(nxg)             # relabels via NodeIndex
S = es.to_scipy_sparse(G)                  # coo_matrix (symmetric if undirected)
us, vs, ws = G.edge_list(weights=True)     # plain numpy arrays

G.save("graph.esg")                        # portable, atomic EDGS v3
H = es.StreamGraph.load("graph.esg")
```

Type stubs (`edgestream/_edgestream.pyi`) document every method for IDEs;
`help(es.StreamGraph)` works too.

Calls operating on one Python graph are serialized by the GIL. OpenMP still
parallelizes work inside supported C++ operations. Use `copy()` when one
thread needs a stable snapshot while another continues ingesting data.
