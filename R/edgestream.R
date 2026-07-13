#edgestream R API. The graph returned by stream_graph() is an external pointer;
#pass it as the first argument to every function. Node IDs are 0-indexed.

#' edgestream: streaming (dynamic) graph analytics
#'
#' Maintains connected components, triangle counts, degrees and basic centrality
#' incrementally as edges arrive. The graph object is an external pointer; node
#' IDs are non-negative integers, 0-indexed.
#'
#' @useDynLib edgestream, .registration = TRUE
#' @importFrom Rcpp evalCpp
#' @keywords internal
"_PACKAGE"

#' Create a StreamGraph
#'
#' @param n_nodes Integer; 0 to auto-grow (default), or N to pre-allocate N
#'   node slots when an upper bound is known.
#' @param directed Logical; whether the graph is directed. Default FALSE.
#' @param weighted Logical; whether edges carry a positive weight. Default FALSE.
#' @return An external pointer to the underlying C++ StreamGraph.
#' @examples
#' G <- stream_graph()
#' add_edge(G, 0L, 1L)
#' n_edges(G)
#' W <- stream_graph(weighted = TRUE)
#' add_edge(W, 0L, 1L, w = 2.5)
#' @export
stream_graph <- function(n_nodes = 0L, directed = FALSE, weighted = FALSE) {
    .sg_create(as.integer(n_nodes), as.logical(directed), as.logical(weighted))
}

#' Add a single edge
#'
#' @param G A StreamGraph (from [stream_graph()]).
#' @param u,v Node IDs (0-indexed). Storage auto-expands to fit.
#' @param w Numeric edge weight (> 0); used only by weighted graphs. A
#'   duplicate edge keeps its originally stored weight.
#' @return Logical: TRUE if the edge was new, FALSE for a self-loop or duplicate.
#' @examples
#' G <- stream_graph()
#' add_edge(G, 0L, 1L)
#' add_edge(G, 0L, 1L)  # FALSE: duplicate
#' @export
add_edge <- function(G, u, v, w = 1.0) {
    .sg_add_edge(G, as.integer(u), as.integer(v), as.double(w))
}

#' Remove a single edge
#'
#' Triangles, degrees and weights update exactly; the Union-Find structure
#' cannot split, so the next component query after a removal rebuilds it in
#' O(n + m). A node stays "touched" after losing all edges (it becomes an
#' isolated single-node component).
#'
#' @param G A StreamGraph.
#' @param u,v Node IDs (0-indexed).
#' @return Logical: TRUE if the edge was present and removed.
#' @examples
#' G <- stream_graph()
#' add_edge(G, 0L, 1L); add_edge(G, 1L, 2L)
#' remove_edge(G, 1L, 2L)
#' n_components(G)  # 2: {0, 1} and isolated {2}
#' @export
remove_edge <- function(G, u, v) {
    .sg_remove_edge(G, as.integer(u), as.integer(v))
}

#' Add many edges in a batch
#'
#' Duplicates within the batch and self-loops are removed in a parallel pre-pass;
#' unique edges are then applied serially. With weights, the first occurrence of
#' a duplicated edge wins.
#'
#' @param G A StreamGraph.
#' @param us,vs Integer vectors of equal length holding 0-indexed endpoints.
#' @param ws Optional numeric vector of edge weights (> 0), same length as us.
#' @param n_threads Integer; OpenMP threads for the dedup sort (0 = default).
#' @return Numeric: the number of new edges added.
#' @examples
#' G <- stream_graph()
#' add_edges(G, c(0L, 1L, 2L), c(1L, 2L, 0L))
#' @export
add_edges <- function(G, us, vs, ws = NULL, n_threads = 0L) {
    if (!is.null(ws)) ws <- as.double(ws)
    .sg_add_edges(G, as.integer(us), as.integer(vs), ws, as.integer(n_threads))
}

#' Whether two nodes are in the same component
#'
#' Components ignore edge direction: for directed graphs these are the
#' weakly connected components.
#'
#' @param G A StreamGraph.
#' @param u,v Node IDs (0-indexed).
#' @return Logical.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' same_component(G, 0L, 1L)
#' @export
same_component <- function(G, u, v) {
    .sg_same_component(G, as.integer(u), as.integer(v))
}

#' Component root ID of a node
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer root ID.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' component_id(G, 1L)
#' @export
component_id <- function(G, u) {
    .sg_component_id(G, as.integer(u))
}

#' Number of connected components (touched nodes only)
#'
#' Edge direction is ignored: for directed graphs this counts weakly
#' connected components.
#'
#' @param G A StreamGraph.
#' @return Integer.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L); add_edge(G, 2L, 3L)
#' n_components(G)
#' @export
n_components <- function(G) {
    .sg_n_components(G)
}

#' Number of nodes in a node's component
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer.
#' @examples
#' G <- stream_graph(); for (i in 0:4) add_edge(G, i, i + 1L)
#' component_size(G, 0L)
#' @export
component_size <- function(G, u) {
    .sg_component_size(G, as.integer(u))
}

#' All touched nodes in a node's component
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer vector of node IDs (0-indexed).
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L); add_edge(G, 1L, 2L)
#' component_nodes(G, 0L)
#' @export
component_nodes <- function(G, u) {
    .sg_component_nodes(G, as.integer(u))
}

#' Component root ID for every touched node
#'
#' @param G A StreamGraph.
#' @return Integer vector, one entry per touched node in ascending ID order.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' component_ids(G)
#' @export
component_ids <- function(G) {
    .sg_component_ids(G)
}

#' Degree of a node
#'
#' In directed graphs this is the out-degree; see [in_degree()].
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L); add_edge(G, 0L, 2L)
#' degree(G, 0L)
#' @export
degree <- function(G, u) {
    .sg_degree(G, as.integer(u))
}

#' In-degree of a node
#'
#' Equals [degree()] in undirected graphs.
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer.
#' @examples
#' G <- stream_graph(directed = TRUE); add_edge(G, 0L, 2L); add_edge(G, 1L, 2L)
#' in_degree(G, 2L)  # 2
#' degree(G, 2L)     # 0 (out-degree)
#' @export
in_degree <- function(G, u) {
    .sg_in_degree(G, as.integer(u))
}

#' Degree histogram
#'
#' In directed graphs the histogram is over out-degrees.
#'
#' @param G A StreamGraph.
#' @return Named numeric vector; entry "d" is the number of nodes of degree d.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L); add_edge(G, 0L, 2L)
#' degree_histogram(G)
#' @export
degree_histogram <- function(G) {
    .sg_degree_histogram(G)
}

#' Sorted neighbours of a node
#'
#' In directed graphs these are the out-neighbours; see [in_neighbours()].
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Sorted integer vector of neighbour IDs (0-indexed).
#' @examples
#' G <- stream_graph(); add_edge(G, 5L, 2L); add_edge(G, 5L, 1L)
#' neighbours(G, 5L)
#' @export
neighbours <- function(G, u) {
    .sg_neighbours(G, as.integer(u))
}

#' Sorted in-neighbours of a node
#'
#' Equals [neighbours()] in undirected graphs.
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Sorted integer vector of in-neighbour IDs (0-indexed).
#' @examples
#' G <- stream_graph(directed = TRUE); add_edge(G, 0L, 2L); add_edge(G, 1L, 2L)
#' in_neighbours(G, 2L)  # c(0L, 1L)
#' @export
in_neighbours <- function(G, u) {
    .sg_in_neighbours(G, as.integer(u))
}

#' Whether an edge exists
#'
#' @param G A StreamGraph.
#' @param u,v Node IDs (0-indexed).
#' @return Logical.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' has_edge(G, 0L, 1L)
#' @export
has_edge <- function(G, u, v) {
    .sg_has_edge(G, as.integer(u), as.integer(v))
}

#' Number of touched (referenced) nodes
#'
#' @param G A StreamGraph.
#' @return Integer.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' n_nodes(G)
#' @export
n_nodes <- function(G) {
    .sg_n_nodes(G)
}

#' Number of edges
#'
#' @param G A StreamGraph.
#' @return Numeric.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' n_edges(G)
#' @export
n_edges <- function(G) {
    .sg_n_edges(G)
}

#' Global triangle count
#'
#' Triangles are counted on the underlying undirected graph: edge direction
#' and reciprocal edges are ignored, so the count does not depend on the
#' order in which edges were added.
#'
#' @param G A StreamGraph.
#' @return Numeric; each triangle counted once.
#' @examples
#' G <- stream_graph()
#' for (e in list(c(0,1), c(1,2), c(0,2))) add_edge(G, e[1], e[2])
#' triangle_count(G)
#' @export
triangle_count <- function(G) {
    .sg_triangle_count(G)
}

#' Triangles incident to a node
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Integer.
#' @examples
#' G <- stream_graph()
#' for (e in list(c(0,1), c(1,2), c(0,2))) add_edge(G, e[1], e[2])
#' local_triangles(G, 0L)
#' @export
local_triangles <- function(G, u) {
    .sg_local_triangles(G, as.integer(u))
}

#' Per-node local triangle counts
#'
#' @param G A StreamGraph.
#' @return Integer vector indexed by node ID over the allocated range.
#' @examples
#' G <- stream_graph()
#' for (e in list(c(0,1), c(1,2), c(0,2))) add_edge(G, e[1], e[2])
#' all_local_triangles(G)
#' @export
all_local_triangles <- function(G) {
    .sg_all_local_triangles(G)
}

#' Edge density
#'
#' @param G A StreamGraph.
#' @return Numeric: 2m/(n(n-1)) undirected, m/(n(n-1)) directed.
#' @examples
#' G <- stream_graph(); for (i in 0:3) add_edge(G, i, i + 1L)
#' density(G)
#' @export
density <- function(G) {
    .sg_density(G)
}

#' Average degree
#'
#' @param G A StreamGraph.
#' @return Numeric: 2m/n undirected, m/n directed.
#' @examples
#' G <- stream_graph(); for (i in 0:3) add_edge(G, i, i + 1L)
#' avg_degree(G)
#' @export
avg_degree <- function(G) {
    .sg_avg_degree(G)
}

#' Maximum degree
#'
#' Maintained incrementally, O(1) to query. In directed graphs this is the
#' maximum out-degree.
#'
#' @param G A StreamGraph.
#' @return Integer.
#' @examples
#' G <- stream_graph(); for (i in 0:3) add_edge(G, i, i + 1L)
#' max_degree(G)
#' @export
max_degree <- function(G) {
    .sg_max_degree(G)
}

#' Approximate betweenness centrality
#'
#' Random (s, t) pair sampling with per-pair BFS (Dijkstra for weighted
#' graphs). Directed graphs respect edge direction and sum dependencies over
#' ordered pairs; undirected graphs use unordered pairs.
#'
#' @param G A StreamGraph.
#' @param k Integer; number of sampled pairs (clamped to all pairs => exact).
#' @param n_threads Integer; OpenMP threads (0 = default).
#' @param seed Integer; RNG seed for reproducibility.
#' @param normalise Logical; TRUE (default) divides by the maximum so scores
#'   lie in [0, 1], FALSE returns the raw betweenness estimate (sampled sums
#'   scaled by total_pairs / k).
#' @return Numeric vector of per-node scores.
#' @examples
#' G <- stream_graph(); for (i in 0:4) add_edge(G, i, i + 1L)
#' betweenness_approx(G, k = 100L, seed = 42L)
#' betweenness_approx(G, k = 100L, seed = 42L, normalise = FALSE)
#' @export
betweenness_approx <- function(G, k = 200L, n_threads = 0L, seed = 42L,
                               normalise = TRUE) {
    .sg_betweenness_approx(G, as.integer(k), as.integer(n_threads),
                           as.double(seed), as.logical(normalise))
}

#' Save a graph to a binary file
#'
#' @param G A StreamGraph.
#' @param path Output file path.
#' @return Invisibly NULL.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' p <- tempfile(fileext = ".sgph")
#' save_graph(G, p)
#' @export
save_graph <- function(G, path) {
    .sg_save(G, path)
    invisible(NULL)
}

#' Load a graph from a binary file
#'
#' @param path Input file path (written by [save_graph()]).
#' @return A StreamGraph external pointer.
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L)
#' p <- tempfile(fileext = ".sgph"); save_graph(G, p)
#' H <- load_graph(p); n_edges(H)
#' @export
load_graph <- function(path) {
    .sg_load(path)
}

#' Weight of an edge
#'
#' @param G A StreamGraph.
#' @param u,v Node IDs (0-indexed).
#' @return Numeric; 1.0 for unweighted graphs. Errors if the edge is absent.
#' @examples
#' W <- stream_graph(weighted = TRUE); add_edge(W, 0L, 1L, w = 2.5)
#' edge_weight(W, 0L, 1L)
#' @export
edge_weight <- function(G, u, v) {
    .sg_edge_weight(G, as.integer(u), as.integer(v))
}

#' Strength (weighted degree) of a node
#'
#' Sum of (out-)edge weights at u; equals [degree()] when unweighted.
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Numeric.
#' @examples
#' W <- stream_graph(weighted = TRUE)
#' add_edge(W, 0L, 1L, w = 2); add_edge(W, 0L, 2L, w = 3)
#' strength(W, 0L)  # 5
#' @export
strength <- function(G, u) {
    .sg_strength(G, as.integer(u))
}

#' Total weight of all edges
#'
#' @param G A StreamGraph.
#' @return Numeric; equals [n_edges()] when unweighted.
#' @examples
#' W <- stream_graph(weighted = TRUE); add_edge(W, 0L, 1L, w = 2.5)
#' total_weight(W)
#' @export
total_weight <- function(G) {
    .sg_total_weight(G)
}

#' Local clustering coefficient
#'
#' 2T(u) / (d(u) (d(u) - 1)) on the underlying undirected graph, where T(u)
#' is the number of triangles through u; 0 for degree < 2.
#'
#' @param G A StreamGraph.
#' @param u Node ID (0-indexed).
#' @return Numeric in [0, 1].
#' @examples
#' G <- stream_graph()
#' for (e in list(c(0, 1), c(1, 2), c(0, 2))) add_edge(G, e[1], e[2])
#' clustering_coefficient(G, 0L)  # 1
#' @export
clustering_coefficient <- function(G, u) {
    .sg_clustering_coefficient(G, as.integer(u))
}

#' Average local clustering coefficient
#'
#' Mean of [clustering_coefficient()] over all touched nodes.
#'
#' @param G A StreamGraph.
#' @return Numeric in [0, 1].
#' @examples
#' G <- stream_graph()
#' for (e in list(c(0, 1), c(1, 2), c(0, 2), c(2, 3))) add_edge(G, e[1], e[2])
#' avg_clustering(G)
#' @export
avg_clustering <- function(G) {
    .sg_avg_clustering(G)
}

#' Strongly connected component labels
#'
#' Iterative Tarjan on demand, O(n + m). For each touched node (ascending ID)
#' the label is the smallest node ID in its SCC. For undirected graphs this
#' equals the connected components.
#'
#' @param G A StreamGraph.
#' @return Integer vector, one entry per touched node.
#' @examples
#' D <- stream_graph(directed = TRUE)
#' for (e in list(c(0, 1), c(1, 2), c(2, 0), c(2, 3))) add_edge(D, e[1], e[2])
#' strong_component_ids(D)  # c(0, 0, 0, 3): cycle {0,1,2} plus {3}
#' @export
strong_component_ids <- function(G) {
    .sg_strong_component_ids(G)
}

#' Number of strongly connected components
#'
#' @param G A StreamGraph.
#' @return Integer.
#' @examples
#' D <- stream_graph(directed = TRUE)
#' add_edge(D, 0L, 1L); add_edge(D, 1L, 0L); add_edge(D, 1L, 2L)
#' n_strong_components(D)  # 2
#' @export
n_strong_components <- function(G) {
    .sg_n_strong_components(G)
}

#' PageRank
#'
#' Power iteration on demand. Weighted graphs distribute rank proportionally
#' to edge weights; dangling nodes redistribute uniformly over touched nodes.
#'
#' @param G A StreamGraph.
#' @param damping Numeric damping factor. Default 0.85.
#' @param tol Numeric L1 convergence tolerance. Default 1e-10.
#' @param max_iter Integer iteration cap. Default 100L.
#' @return Numeric vector indexed by node ID over the allocated range
#'   (untouched IDs get 0); sums to 1.
#' @examples
#' G <- stream_graph(); for (i in 0:3) add_edge(G, i, i + 1L)
#' pagerank(G)
#' @export
pagerank <- function(G, damping = 0.85, tol = 1e-10, max_iter = 100L) {
    .sg_pagerank(G, as.double(damping), as.double(tol), as.integer(max_iter))
}

#' Edge list as a data.frame
#'
#' Each edge appears once (undirected: u < v; directed: every out-edge).
#'
#' @param G A StreamGraph.
#' @return data.frame with integer columns u, v and numeric w (1 when
#'   unweighted).
#' @examples
#' G <- stream_graph(); add_edge(G, 0L, 1L); add_edge(G, 1L, 2L)
#' edge_list(G)
#' @export
edge_list <- function(G) {
    as.data.frame(.sg_edge_list(G))
}

#' Convert to an igraph graph
#'
#' Requires the igraph package. Node IDs are shifted to 1-indexed igraph
#' vertices (vertex i+1 is edgestream node i); weights become the igraph
#' `weight` edge attribute.
#'
#' @param G A StreamGraph.
#' @return An igraph graph object.
#' @examples
#' if (requireNamespace("igraph", quietly = TRUE)) {
#'   G <- stream_graph(); add_edge(G, 0L, 1L)
#'   ig <- as_igraph(G)
#' }
#' @export
as_igraph <- function(G) {
    if (!requireNamespace("igraph", quietly = TRUE)) {
        stop("as_igraph() requires the igraph package: install.packages(\"igraph\")")
    }
    el <- .sg_edge_list(G)
    n <- if (length(el$u) == 0) 0L else max(el$u, el$v) + 1L
    ig <- igraph::make_empty_graph(n = n, directed = .sg_is_directed(G))
    if (length(el$u) > 0) {
        edges <- rbind(el$u + 1L, el$v + 1L)
        ig <- igraph::add_edges(ig, as.vector(edges))
        if (.sg_is_weighted(G)) igraph::E(ig)$weight <- el$w
    }
    ig
}

#' Convert from an igraph graph
#'
#' igraph vertex i becomes edgestream node i-1; a `weight` edge attribute, if
#' present, is carried over into a weighted StreamGraph.
#'
#' @param ig An igraph graph object.
#' @return A StreamGraph.
#' @examples
#' if (requireNamespace("igraph", quietly = TRUE)) {
#'   ig <- igraph::make_ring(5)
#'   G <- from_igraph(ig)
#'   n_edges(G)  # 5
#' }
#' @export
from_igraph <- function(ig) {
    if (!requireNamespace("igraph", quietly = TRUE)) {
        stop("from_igraph() requires the igraph package: install.packages(\"igraph\")")
    }
    weighted <- "weight" %in% igraph::edge_attr_names(ig)
    G <- stream_graph(n_nodes = igraph::vcount(ig),
                      directed = igraph::is_directed(ig),
                      weighted = weighted)
    el <- igraph::as_edgelist(ig, names = FALSE)
    if (nrow(el) > 0) {
        ws <- if (weighted) igraph::E(ig)$weight else NULL
        add_edges(G, as.integer(el[, 1]) - 1L, as.integer(el[, 2]) - 1L, ws = ws)
    }
    G
}

#NodeIndex: pure R, an environment with two hash maps (key -> id, id -> key)

#' Create a node index
#'
#' Maps arbitrary keys to contiguous 0-indexed node IDs (insertion order).
#' Both directions are environment-backed hash maps, so lookups and inserts
#' are O(1).
#'
#' @return An environment of class "edgestream_node_index".
#' @examples
#' idx <- node_index()
#' node_id(idx, "alice")
#' @export
node_index <- function() {
    idx <- new.env(parent = emptyenv())
    idx$key_to_id <- new.env(parent = emptyenv())
    idx$id_to_key <- new.env(parent = emptyenv())
    idx$n <- 0L
    class(idx) <- "edgestream_node_index"
    idx
}

#' Look up or assign a node ID for a key
#'
#' @param idx A node index from [node_index()].
#' @param key A key (coerced to character).
#' @return Integer ID (0-indexed); assigned on first use.
#' @examples
#' idx <- node_index()
#' node_id(idx, "alice")  # 0
#' node_id(idx, "bob")    # 1
#' @export
node_id <- function(idx, key) {
    key <- as.character(key)
    existing <- idx$key_to_id[[key]]
    if (!is.null(existing)) return(existing)
    new_id <- idx$n
    idx$key_to_id[[key]] <- new_id
    idx$id_to_key[[as.character(new_id)]] <- key
    idx$n <- new_id + 1L
    new_id
}

#' Get the ID of an existing key
#'
#' @param idx A node index.
#' @param key A key (coerced to character).
#' @return Integer ID; errors if the key is absent.
#' @examples
#' idx <- node_index(); node_id(idx, "alice")
#' get_id(idx, "alice")
#' @export
get_id <- function(idx, key) {
    key <- as.character(key)
    existing <- idx$key_to_id[[key]]
    if (is.null(existing)) stop(sprintf("key not found: %s", key))
    existing
}

#' Get the key for an existing ID
#'
#' @param idx A node index.
#' @param id Integer node ID (0-indexed).
#' @return The key (character); errors if out of range.
#' @examples
#' idx <- node_index(); node_id(idx, "alice")
#' get_key(idx, 0L)
#' @export
get_key <- function(idx, id) {
    id <- as.integer(id)
    key <- idx$id_to_key[[as.character(id)]]
    if (is.null(key)) stop("id out of range")
    key
}

#' Number of indexed keys
#'
#' @param idx A node index.
#' @return Integer.
#' @examples
#' idx <- node_index(); node_id(idx, "alice")
#' n_indexed(idx)
#' @export
n_indexed <- function(idx) {
    idx$n
}

#' Whether a key is present in the index
#'
#' @param idx A node index.
#' @param key A key (coerced to character).
#' @return Logical.
#' @examples
#' idx <- node_index(); node_id(idx, "alice")
#' has_key(idx, "alice")
#' @export
has_key <- function(idx, key) {
    !is.null(idx$key_to_id[[as.character(key)]])
}
