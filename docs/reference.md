# API reference

Python methods live on `StreamGraph`; R functions take the graph as their
first argument. The APIs mirror each other unless noted.

| area | Python | R |
|---|---|---|
| construct | `StreamGraph` | `stream_graph` |
| nodes | `add_node`, `add_nodes`, `nodes` | same |
| storage | `reserve_nodes`, `reserve_edges`, `clear`, `copy` | `reserve_nodes`, `reserve_edges`, `clear_graph`, `copy_graph` |
| mutate edges | `add_edge`, `add_edges`, `remove_edge`, `update_edge_weight` | same |
| components | `same_component`, `component_id`, `component_size`, `component_nodes`, `component_ids`, `n_components` | same |
| directed components | `strong_component_ids`, `n_strong_components` | same |
| degree | `degree`, `in_degree`, `degree_histogram`, `max_degree`, `avg_degree` | same |
| adjacency | `has_edge`, `neighbours`, `in_neighbours`, `edge_list` | same |
| weights | `edge_weight`, `strength`, `total_weight` | same |
| triangles | `triangle_count`, `local_triangles`, `all_local_triangles`, `clustering_coefficient`, `avg_clustering` | same |
| graph size | `n_nodes`, `n_ids`, `n_edges`, `density` | same |
| centrality | `betweenness_approx`, `pagerank` | same |
| persistence | `save`, `StreamGraph.load` | `save_graph`, `load_graph` |
| window | `SlidingWindowGraph` | `sliding_window_graph`, `window_add_edge`, `window_advance`, `window_snapshot` |

Python has NumPy-style docstrings and PEP 561 type information. In R, use
`help(package = "edgestream")` and the generated Rd reference.
