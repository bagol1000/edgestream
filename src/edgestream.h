// Top-level dynamic graph: adjacency lists, Union-Find components, triangles,
// degree tracking, statistics, batch ingestion, serialisation, edge removal,
// optional edge weights, and on-demand centrality (betweenness, PageRank, SCC).
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dsu.h"
#include "flat_hash_set.h"

namespace edgestream {

// In-neighbour lists for directed graphs live in StreamGraph::in_adj, and
// weights in StreamGraph::w_adj: keeping AdjList small preserves cache
// locality for the unweighted/undirected hot path.
struct AdjList {
    std::vector<uint32_t> neighbours;  // out-neighbours when directed; kept sorted
    uint32_t degree = 0;               // out-degree when directed
    uint32_t triangle_count = 0;
};

// Merge-scan two sorted id lists in O(|a| + |b|); returns the common count
// and appends the common ids to common_out (caller clears it).
uint32_t count_common_neighbours(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b,
    std::vector<uint32_t>& common_out
);

// Sorted union with dedup of two sorted id lists into out (cleared first).
void sorted_union(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b,
    std::vector<uint32_t>& out
);

using EdgeKey = uint64_t;
inline EdgeKey make_key(uint32_t u, uint32_t v) {
    return (static_cast<uint64_t>(std::min(u, v)) << 32) | std::max(u, v);
}
inline EdgeKey make_directed_key(uint32_t u, uint32_t v) {
    return (static_cast<uint64_t>(u) << 32) | v;
}

using EdgeSet = FlatHashSet;

// Starting capacity when n_nodes_initial == 0 (auto-growing mode)
constexpr uint32_t EDGESTREAM_AUTO_INITIAL_CAPACITY = 1024;

struct StreamGraph {
    bool directed;
    bool weighted;             // when false all edges have implicit weight 1.0
    uint32_t n_nodes_initial;  // pre-allocated size, 0 means auto-growing
    uint32_t n_nodes_actual;   // current maximum node id + 1

    std::vector<AdjList> adj;  // indexed by node id
    std::vector<std::vector<uint32_t>> in_adj;  // sorted in-neighbours per node; directed mode only (else empty)
    std::vector<std::vector<double>> w_adj;     // weights aligned with adj[u].neighbours; weighted mode only
    std::vector<double> strength_sum;           // maintained out-strength; weighted mode only
    DSU dsu;
    EdgeSet edge_set;

    uint64_t n_edges = 0;
    uint64_t n_triangles = 0;
    double clustering_sum = 0.0;  // sum of local coefficients over touched nodes
    double total_weight_sum = 0.0;  // sum of weights over present edges
    // degree_histogram[d] = #touched nodes of (out-)degree d; sums to n_touched.
    // Nodes whose edges were all removed (and directed sinks) sit in bucket 0.
    std::vector<uint64_t> degree_histogram;

    // Model A component bookkeeping: a node counts only once it is touched by an edge;
    // never-referenced buffer slots are not components. A node stays touched after
    // its edges are removed (it becomes an isolated single-node component).
    std::vector<bool> touched;
    uint32_t n_touched = 0;
    uint32_t n_components_touched = 0;

    StreamGraph(uint32_t n_nodes_initial, bool directed, bool weighted = false);

    // Non-copyable: always held by pointer (pybind11 unique_ptr / Rcpp XPtr)
    StreamGraph(const StreamGraph&) = delete;
    StreamGraph& operator=(const StreamGraph&) = delete;

    // Add an edge; returns true if new, false for a self-loop or duplicate
    // (a duplicate does not update the stored weight). w is ignored unless weighted.
    bool add_edge(uint32_t u, uint32_t v, double w = 1.0);

    bool add_node(uint32_t u);
    uint64_t add_nodes(uint32_t start, uint32_t count);
    std::vector<uint32_t> nodes() const;
    void reserve_nodes(uint32_t n);
    void reserve_edges(uint64_t m);
    void clear();

    // Remove an edge; returns true if it was present. O(deg) for the adjacency
    // update; the next component query after a removal rebuilds the Union-Find
    // in O(n + m). Triangles, degrees and weights update exactly.
    bool remove_edge(uint32_t u, uint32_t v);
    bool update_edge_weight(uint32_t u, uint32_t v, double w);

    // Batch add (parallel dedup, serial apply); returns the number of new edges.
    // ws may be null (all weights 1); with duplicates inside the batch the first
    // occurrence's weight wins.
    uint64_t add_edges(const uint32_t* us, const uint32_t* vs, const double* ws,
                       size_t m, int n_threads);

    // Components ignore edge direction: for directed graphs these are the
    // weakly connected components.
    uint32_t find_component(uint32_t u);
    bool same_component(uint32_t u, uint32_t v);
    uint32_t n_components();
    uint32_t degree(uint32_t u) const;     // out-degree when directed
    uint32_t in_degree(uint32_t u) const;  // == degree(u) when undirected
    std::vector<uint32_t> neighbours(uint32_t u) const;     // sorted; out-neighbours when directed
    std::vector<uint32_t> in_neighbours(uint32_t u) const;  // sorted; == neighbours(u) when undirected
    bool has_edge(uint32_t u, uint32_t v) const;
    double edge_weight(uint32_t u, uint32_t v) const;  // 1.0 for unweighted; throws if absent
    double strength(uint32_t u) const;   // sum of (out-)edge weights; == degree when unweighted
    double total_weight() const;         // sum of weights over all edges
    uint32_t component_size(uint32_t u);
    uint32_t n_nodes() const;  // == n_touched

    // Triangles are counted on the underlying undirected graph (direction and
    // reciprocal edges ignored), which makes the count insertion-order independent.
    uint64_t total_triangles() const;
    uint32_t local_triangles(uint32_t u) const;
    std::vector<uint32_t> all_local_triangles() const;  // per node id over the allocated range

    // Local clustering coefficient 2T(u) / (deg(u) (deg(u)-1)) on the underlying
    // undirected graph (undirected degree used when directed); 0 for deg < 2.
    double clustering_coefficient(uint32_t u) const;
    double avg_clustering() const;  // mean over touched nodes

    double density() const;     // 2m/(n(n-1)) undirected, m/(n(n-1)) directed
    double avg_degree() const;  // 2m/n undirected, m/n directed
    uint32_t max_degree() const;  // O(1); max out-degree when directed

    std::vector<uint32_t> component_nodes(uint32_t u);  // touched nodes in u's component, O(n)
    std::vector<uint32_t> component_ids();  // canonical min id per touched node, ascending id

    // Edge list export: fills us/vs (and ws unless null) with every edge once
    // (undirected: u < v; directed: every stored out-edge).
    void edge_list(std::vector<uint32_t>& us, std::vector<uint32_t>& vs,
                   std::vector<double>* ws) const;

    // Strongly connected components (iterative Tarjan, on demand, O(n + m)).
    // Per touched node in ascending id order, the label is the smallest node id
    // in its SCC. For undirected graphs this equals the connected components.
    std::vector<uint32_t> strong_component_ids() const;
    uint32_t n_strong_components() const;

    // PageRank via power iteration (on demand). Weighted graphs distribute rank
    // proportionally to edge weights; dangling nodes redistribute uniformly.
    // Returns one value per node id (untouched ids get 0); sums to 1.
    std::vector<double> pagerank(double damping = 0.85, double tol = 1e-10,
                                 int max_iter = 100) const;

    void save(const char* path) const;
    static std::unique_ptr<StreamGraph> load(const char* path);

private:
    std::vector<uint32_t> common_buf;  // scratch for common-neighbour collection
    std::vector<uint32_t> union_u_buf, union_v_buf;  // scratch for directed undirected-neighbour unions
    uint32_t max_deg_ = 0;      // maintained incrementally (histogram_move / removal scan-down)
    bool comps_dirty = false;   // set by remove_edge; next component query rebuilds the DSU

    EdgeKey edge_key(uint32_t u, uint32_t v) const;
    bool valid_touched_node(uint32_t u) const;
    void expand_to(uint32_t new_max);
    void touch(uint32_t u);
    void insert_neighbour(uint32_t u, uint32_t v, double w);
    void insert_in_neighbour(uint32_t u, uint32_t v);
    void erase_neighbour(uint32_t u, uint32_t v);
    void erase_in_neighbour(uint32_t u, uint32_t v);
    void histogram_move(uint32_t old_deg, uint32_t new_deg);
    // Count triangles the undirected edge {u, v} participates in right now,
    // collecting the common neighbours into common_buf.
    uint32_t undirected_common_count(uint32_t u, uint32_t v);
    void ensure_components();  // rebuild DSU after removals, O(n + m)
    uint64_t add_edges_unweighted(const uint32_t* us, const uint32_t* vs, size_t m, int n_threads);
};

// Approximate betweenness via random pair sampling; kept separate from StreamGraph
// (expensive, computed on demand). One value per node id; empty if the graph has
// no touched nodes. Directed graphs sum dependencies over ordered (s, t) pairs and
// respect edge direction; undirected graphs use unordered pairs. Weighted graphs
// use Dijkstra shortest paths (weights as distances, must be positive).
// normalise=true divides by the maximum (scores in [0, 1]); normalise=false returns
// the raw betweenness estimate (sampled sums scaled by total_pairs / k).
struct BetweennessApprox {
    static std::vector<double> compute(
        const StreamGraph& G,
        int k = 200,
        int n_threads = 0,
        uint64_t seed = 42,
        bool normalise = true
    );
};

}
