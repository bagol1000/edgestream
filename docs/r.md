# R guide

The R API mirrors Python: the graph is an external pointer created by
`stream_graph()` and passed as the first argument to every function. Node IDs
are **0-indexed** in R as well. Full reference: `?edgestream` or the pkgdown
site; a narrative walkthrough is in the package vignette
(`vignette("edgestream")`).

## Building and mutating

```r
library(edgestream)

G <- stream_graph()                          # undirected, unweighted
D <- stream_graph(directed = TRUE)
W <- stream_graph(weighted = TRUE)

add_edge(G, 0L, 1L)
add_edge(W, 0L, 1L, w = 2.5)
remove_edge(G, 0L, 1L)
add_edges(G, c(0L, 1L, 2L), c(1L, 2L, 0L), n_threads = 4L)
add_edges(W, c(0L, 1L), c(1L, 2L), ws = c(1.5, 3))
```

## Queries

```r
n_nodes(G); n_edges(G); density(G); avg_degree(G); max_degree(G)
degree(G, u); in_degree(G, u); neighbours(G, u); in_neighbours(G, u)
has_edge(G, u, v); edge_weight(W, u, v); strength(W, u); total_weight(W)
degree_histogram(G)

same_component(G, u, v); component_id(G, u); n_components(G)
component_size(G, u); component_nodes(G, u); component_ids(G)

triangle_count(G); local_triangles(G, u); all_local_triangles(G)
clustering_coefficient(G, u); avg_clustering(G)
```

## On-demand algorithms

```r
betweenness_approx(G, k = 200L, n_threads = 4L, seed = 42L)
betweenness_approx(G, k = 200L, normalise = FALSE)
pagerank(G, damping = 0.85)
strong_component_ids(D); n_strong_components(D)
```

## Interop, keys and persistence

```r
ig <- as_igraph(G)          # requires the igraph package
G2 <- from_igraph(ig)
edge_list(G)                # data.frame(u, v, w)

idx <- node_index()         # arbitrary keys -> 0-indexed IDs
add_edge(G, node_id(idx, "alice"), node_id(idx, "bob"))
get_key(idx, 0L)

p <- tempfile(fileext = ".esg")
save_graph(G, p)
H <- load_graph(p)
```
