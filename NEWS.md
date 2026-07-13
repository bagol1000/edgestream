# edgestream 0.2.0 (2026-07-12)

Package renamed from `streamgraph` to `edgestream` (the old name collides with
the streamgraph chart type). No released version was published under the old
name.

## New features

* Edge weights: `stream_graph(weighted = TRUE)` / `StreamGraph(weighted=True)`
  stores a positive weight per edge; `edge_weight()`, `strength()`,
  `total_weight()`. Weighted betweenness uses Dijkstra; PageRank distributes
  rank proportionally to weights.
* Edge removal: `remove_edge()` updates triangles, degrees and weights
  exactly; the next component query after a removal rebuilds Union-Find in
  O(n + m).
* Clustering coefficients: `clustering_coefficient()`, `avg_clustering()`.
* Strongly connected components on demand (iterative Tarjan):
  `strong_component_ids()`, `n_strong_components()`.
* PageRank on demand (power iteration): `pagerank()`.
* Edge-list export (`edge_list()`) and ecosystem conversions: igraph in R
  (`as_igraph()`, `from_igraph()`), NetworkX and scipy.sparse in Python
  (`to_networkx()`, `from_networkx()`, `to_scipy_sparse()`).
* `betweenness_approx(normalise = FALSE)` returns raw betweenness estimates.
* `in_degree()` / `in_neighbours()` for directed graphs; `max_degree()` is
  now O(1).

## Fixes

* Directed graphs: triangle counting is now insertion-order independent
  (counted on the underlying undirected graph) and survives save/load.
* Directed betweenness back-propagates over in-edges and sums over ordered
  pairs (previously returned zeros on DAG paths).
* `load()` rejects files with trailing data.

## File format

* New binary format v2 (magic `EDGS`, extension `.esg`) with optional edge
  weights. Files written by the unreleased v1 format are not readable.

# edgestream 0.1.0

Initial development version (as `streamgraph`): incremental connected
components, triangle counting, degree tracking, neighbourhood queries, batch
ingestion, binary serialisation and approximate betweenness.
