#streamgraph R API. The graph returned by stream_graph() is an external pointer;
#pass it as the first argument to every function. Node IDs are 0-indexed.

#' streamgraph: streaming (dynamic) graph analytics
#'
#' Maintains connected components, triangle counts, degrees and basic centrality
#' incrementally as edges arrive. The graph object is an external pointer; node
#' IDs are non-negative integers, 0-indexed.
#'
#' @useDynLib streamgraph, .registration = TRUE
#' @importFrom Rcpp evalCpp
#' @keywords internal
"_PACKAGE"

#' Create a StreamGraph
#'
#' @param n_nodes Integer; 0 to auto-grow (default), or N to pre-allocate N
#'   node slots when an upper bound is known.
#' @param directed Logical; whether the graph is directed. Default FALSE.
#' @return An external pointer to the underlying C++ StreamGraph.
#' @examples
#' G <- stream_graph()
#' add_edge(G, 0L, 1L)
#' n_edges(G)
#' @export
stream_graph <- function(n_nodes = 0L, directed = FALSE) {
    .sg_create(as.integer(n_nodes), as.logical(directed))
}

#' Add a single edge
#'
#' @param G A StreamGraph (from [stream_graph()]).
#' @param u,v Node IDs (0-indexed). Storage auto-expands to fit.
#' @return Logical: TRUE if the edge was new, FALSE for a self-loop or duplicate.
#' @examples
#' G <- stream_graph()
#' add_edge(G, 0L, 1L)
#' add_edge(G, 0L, 1L)  # FALSE: duplicate
#' @export
add_edge <- function(G, u, v) {
    .sg_add_edge(G, as.integer(u), as.integer(v))
}

#' Add many edges in a batch
#'
#' Duplicates within the batch and self-loops are removed in a parallel pre-pass;
#' unique edges are then applied serially.
#'
#' @param G A StreamGraph.
#' @param us,vs Integer vectors of equal length holding 0-indexed endpoints.
#' @param n_threads Integer; OpenMP threads for the dedup sort (0 = default).
#' @return Numeric: the number of new edges added.
#' @examples
#' G <- stream_graph()
#' add_edges(G, c(0L, 1L, 2L), c(1L, 2L, 0L))
#' @export
add_edges <- function(G, us, vs, n_threads = 0L) {
    .sg_add_edges(G, as.integer(us), as.integer(vs), as.integer(n_threads))
}

#' Whether two nodes are in the same component
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

#' Degree histogram
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
#' Random (s, t) pair sampling with per-pair BFS; normalised to [0, 1].
#'
#' @param G A StreamGraph.
#' @param k Integer; number of sampled pairs (clamped to all pairs => exact).
#' @param n_threads Integer; OpenMP threads (0 = default).
#' @param seed Integer; RNG seed for reproducibility.
#' @return Numeric vector of per-node scores in [0, 1].
#' @examples
#' G <- stream_graph(); for (i in 0:4) add_edge(G, i, i + 1L)
#' betweenness_approx(G, k = 100L, seed = 42L)
#' @export
betweenness_approx <- function(G, k = 200L, n_threads = 0L, seed = 42L) {
    .sg_betweenness_approx(G, as.integer(k), as.integer(n_threads),
                           as.double(seed))
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

#NodeIndex: pure R, an environment with two maps

#' Create a node index
#'
#' Maps arbitrary keys to contiguous 0-indexed node IDs (insertion order).
#'
#' @return An environment of class "streamgraph_node_index".
#' @examples
#' idx <- node_index()
#' node_id(idx, "alice")
#' @export
node_index <- function() {
    idx <- new.env(parent = emptyenv())
    idx$key_to_id <- new.env(parent = emptyenv())
    idx$keys <- character(0)
    class(idx) <- "streamgraph_node_index"
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
    new_id <- length(idx$keys)
    idx$key_to_id[[key]] <- new_id
    idx$keys <- c(idx$keys, key)
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
    if (id < 0 || id >= length(idx$keys)) stop("id out of range")
    idx$keys[[id + 1L]]
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
    length(idx$keys)
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
