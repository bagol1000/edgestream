---
title: 'edgestream: incremental graph analytics over edge streams for Python and R'
tags:
  - Python
  - R
  - C++
  - graphs
  - streaming
  - network analysis
authors:
  - name: bagol1000
    affiliation: 1
affiliations:
  - name: Warsaw University of Technology, Poland
    index: 1
date: 14 July 2026
bibliography: paper.bib
---

# Summary

`edgestream` is a graph-analytics library for workloads in which the graph is
not a fixed dataset but a stream of edges: financial transactions, network
flows, interaction logs. Instead of the load-compute-discard cycle of
conventional tools, `edgestream` maintains its metrics *incrementally*: every
`add_edge` (or `remove_edge`) updates connected components (Union-Find with
path compression and union by rank), exact global and per-node triangle
counts, clustering coefficients, degree statistics and edge-weight totals, so
that scalar counts and maintained aggregates are readable in $O(1)$ and DSU
queries in amortised $O(\alpha(n))$; methods returning collections scale with
their output size. More expensive analyses — betweenness centrality (random
pair sampling with per-pair BFS/Dijkstra [@brandes2001; @geisberger2008]),
PageRank [@page1999] and strongly connected components (Tarjan) — are
computed on demand over the current snapshot without copying the graph.

The engine is a dependency-free C++17 core parallelised with OpenMP, exposed
to Python through pybind11 (NumPy-native batch ingestion) and to R through
Rcpp, with mirrored APIs. Converters to and from NetworkX [@hagberg2008],
igraph [@csardi2006] and `scipy.sparse` let users combine streaming
maintenance with the wider algorithm ecosystems. Explicit isolated nodes,
portable EDGS v3 snapshots and timestamp-based sliding windows cover common
stream lifecycle needs without a separate temporal database.

# Statement of need

General-purpose libraries such as NetworkX and igraph implement rich
algorithm suites over static graphs; recomputing even cheap statistics after
every arriving edge is quadratic in stream length and becomes infeasible at
moderate rates (see the benchmark suite in the repository: maintaining
components, triangles and clustering over a 100k-edge stream with periodic
queries is several orders of magnitude faster in `edgestream` than
recompute-on-query with static tools). Systems built for temporal graphs
(e.g. Raphtory [@steer2020]) target historical queries over persisted
timelines and bring correspondingly heavier machinery. `edgestream` occupies
a deliberately small niche: a light in-memory structure whose invariant is
that the cheap metrics are *always already computed*, with first-class
bindings for the two languages most used in applied data analysis. Exactness
under mutation is enforced by property-based tests that compare every metric
against NetworkX on randomly generated graphs, and the core is fuzzed under
AddressSanitizer/UBSan in continuous integration.

# Acknowledgements

The package started as a course project at the Warsaw University of
Technology.

# References
