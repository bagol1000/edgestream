// Top-level dynamic graph: adjacency lists, Union-Find components, triangles,
// degree tracking, statistics, batch ingestion, serialisation and approximate betweenness.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "dsu.h"
#include "flat_hash_set.h"

namespace streamgraph {

struct AdjList {
    std::vector<uint32_t> neighbours;  // kept sorted for binary search and merge
    uint32_t degree = 0;
    uint32_t triangle_count = 0;
};

// Merge-scan two sorted neighbour lists in O(deg_a + deg_b); returns the common count
// and appends the common ids to common_out (caller clears it).
uint32_t count_common_neighbours(
    const AdjList& a,
    const AdjList& b,
    std::vector<uint32_t>& common_out
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
constexpr uint32_t STREAMGRAPH_AUTO_INITIAL_CAPACITY = 1024;

struct StreamGraph {
    bool directed;
    uint32_t n_nodes_initial;  // pre-allocated size, 0 means auto-growing
    uint32_t n_nodes_actual;   // current maximum node id + 1

    std::vector<AdjList> adj;  // indexed by node id
    DSU dsu;
    EdgeSet edge_set;

    uint64_t n_edges = 0;
    uint64_t n_triangles = 0;
    std::vector<uint64_t> degree_histogram;  // degree_histogram[d] = #nodes of degree d

    // Model A component bookkeeping: a node counts only once it is touched by an edge;
    // never-referenced buffer slots are not components.
    std::vector<bool> touched;
    uint32_t n_touched = 0;
    uint32_t n_components_touched = 0;

    StreamGraph(uint32_t n_nodes_initial, bool directed);

    // Non-copyable: always held by pointer (pybind11 unique_ptr / Rcpp XPtr)
    StreamGraph(const StreamGraph&) = delete;
    StreamGraph& operator=(const StreamGraph&) = delete;

    // Add an edge; returns true if new, false for a self-loop or duplicate
    bool add_edge(uint32_t u, uint32_t v);

    // Batch add (parallel dedup, serial apply); returns the number of new edges
    uint64_t add_edges(const uint32_t* us, const uint32_t* vs, size_t m, int n_threads);

    uint32_t find_component(uint32_t u);
    bool same_component(uint32_t u, uint32_t v);
    uint32_t n_components() const;
    uint32_t degree(uint32_t u) const;
    std::vector<uint32_t> neighbours(uint32_t u) const;  // sorted
    bool has_edge(uint32_t u, uint32_t v) const;
    uint32_t component_size(uint32_t u);
    uint32_t n_nodes() const;  // == n_touched

    uint64_t total_triangles() const;
    uint32_t local_triangles(uint32_t u) const;
    std::vector<uint32_t> all_local_triangles() const;  // per node id over the allocated range

    double density() const;     // 2m/(n(n-1)) undirected, m/(n(n-1)) directed
    double avg_degree() const;  // 2m/n undirected, m/n directed
    uint32_t max_degree() const;  // O(n) scan

    std::vector<uint32_t> component_nodes(uint32_t u);  // touched nodes in u's component, O(n)
    std::vector<uint32_t> component_ids();  // root id per touched node, ascending id

    void save(const char* path) const;
    static std::unique_ptr<StreamGraph> load(const char* path);

private:
    std::vector<uint32_t> common_buf;  // scratch for common-neighbour collection

    EdgeKey edge_key(uint32_t u, uint32_t v) const;
    bool valid_touched_node(uint32_t u) const;
    void expand_to(uint32_t new_max);
    void touch(uint32_t u);
    void insert_neighbour(uint32_t u, uint32_t v);
    void histogram_move(uint32_t old_deg, uint32_t new_deg);
};

// Approximate betweenness via random pair sampling; kept separate from StreamGraph
// (expensive, computed on demand). One value per node id, normalised to [0, 1];
// empty if the graph has no touched nodes.
struct BetweennessApprox {
    static std::vector<double> compute(
        const StreamGraph& G,
        int k = 200,
        int n_threads = 0,
        uint64_t seed = 42
    );
};

}
