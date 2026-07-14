# Benchmarks

Release 0.3.0 was measured on Linux 6.17 x86-64, Python 3.12.3 and NumPy
2.5.1 with the C++ core built using `-O3 -std=c++17 -fopenmp`. Absolute
times depend on hardware; use the included scripts to reproduce them.

## Streaming comparison

The scenario feeds 100,000 random edge events over 50,000 possible IDs and
queries components, triangles and one degree every 1,000 events. The harness
now verifies full checkpoint checksums for NetworkX and igraph. rustworkx has
no triangle API, so its component/degree checksum is verified separately.

| library | time | ratio | verified work |
|---|---:|---:|---|
| edgestream | **0.04 s** | 1x | components, triangles, degree |
| rustworkx | 0.30 s | 7x | components, degree |
| igraph | 1.08 s | 26x | components, triangles, degree |
| NetworkX | 8.81 s | 216x | components, triangles, degree |

The advantage comes from maintaining query results during mutation. It is not
a claim that every insertion is constant-time: sorted adjacency insertion and
triangle maintenance cost O(deg(u)+deg(v)) on balanced neighbourhoods, with an
adaptive O(min(deg) log(max(deg))) intersection for highly skewed degrees.

Run:

    python bench/compare.py

The script prints OS, Python and NumPy versions and exits with an error if any
comparable checksum differs.

## 0.3.0 micro-benchmarks

| operation | measured time |
|---|---:|
| batch 100k edges, 4 threads | 0.0202 s |
| 100k avg_clustering queries | 0.0103 s |
| 100k strength queries | 0.0169 s |
| 10k hub/leaf insertions after building a 20k-degree hub | 0.0026 s |

Run:

    python bench/micro.py

Both `avg_clustering()` and weighted `strength()` are maintained
incrementally in 0.3.0, so their query cost is O(1). Batch sorting is
OpenMP-parallel on GCC/libstdc++; other toolchains use the portable serial
sort while preserving identical semantics. Applying unique edges remains
serial because DSU and triangle updates depend on prior edges in the batch.

## Query complexity

| operation | complexity |
|---|---|
| counts, degree, density, total weight, strength, triangle counts, average clustering | O(1) |
| same_component, component_id, component_size | amortised O(alpha(n)) |
| first component query after removals | O(n + m) rebuild |
| neighbours / in_neighbours | O(degree) to copy the result |
| edge_weight | O(log degree) |
| component_nodes, component_ids, nodes | O(n_ids) |
| degree_histogram | O(max_degree) to copy |
| all_local_triangles, PageRank output | O(n_ids) output size |
| clustering_coefficient | O(1) undirected; O(in_degree + out_degree) directed |

Benchmarks are indicative results, not a performance guarantee.
