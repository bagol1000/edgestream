#include "streamgraph.h"

#include <stdexcept>

namespace streamgraph {

StreamGraph::StreamGraph(uint32_t n_nodes_initial_, bool directed_)
    : directed(directed_), n_nodes_initial(n_nodes_initial_) {
    uint32_t cap = (n_nodes_initial_ == 0) ? STREAMGRAPH_AUTO_INITIAL_CAPACITY : n_nodes_initial_;
    adj.resize(cap);
    if (directed_) in_adj.resize(cap);
    dsu.init(cap);
    touched.assign(cap, false);

    // Auto mode starts with no referenced node; pre-allocated mode promises ids up to n_nodes_initial-1
    n_nodes_actual = n_nodes_initial_;

    // Touched nodes always have degree >= 1, so degree_histogram[0] stays 0
    degree_histogram.assign(1, 0);
}

EdgeKey StreamGraph::edge_key(uint32_t u, uint32_t v) const {
    return directed ? make_directed_key(u, v) : make_key(u, v);
}

bool StreamGraph::valid_touched_node(uint32_t u) const {
    return u < adj.size() && touched[u];
}

void StreamGraph::expand_to(uint32_t new_max) {
    if (new_max < adj.size()) return;
    size_t old_size = adj.size();
    size_t new_size = std::max(
        static_cast<size_t>(new_max) + 1,
        old_size + old_size / 2
    );
    adj.resize(new_size);
    if (directed) in_adj.resize(new_size);
    dsu.grow(static_cast<uint32_t>(new_size));
    touched.resize(new_size, false);
}

void StreamGraph::touch(uint32_t u) {
    if (!touched[u]) {
        touched[u] = true;
        ++n_touched;
        ++n_components_touched;
    }
}

void StreamGraph::histogram_move(uint32_t old_deg, uint32_t new_deg) {
    if (new_deg >= degree_histogram.size()) degree_histogram.resize(new_deg + 1, 0);
    // old_deg == 0 means the node was isolated and not yet in the histogram
    if (old_deg > 0) --degree_histogram[old_deg];
    ++degree_histogram[new_deg];
    if (new_deg > max_deg_) max_deg_ = new_deg;
}

void StreamGraph::insert_neighbour(uint32_t u, uint32_t v) {
    auto& a = adj[u];
    auto pos = std::lower_bound(a.neighbours.begin(), a.neighbours.end(), v);
    a.neighbours.insert(pos, v);
    histogram_move(a.degree, a.degree + 1);
    ++a.degree;
}

void StreamGraph::insert_in_neighbour(uint32_t u, uint32_t v) {
    auto& in = in_adj[u];
    in.insert(std::lower_bound(in.begin(), in.end(), v), v);
}

bool StreamGraph::add_edge(uint32_t u, uint32_t v) {
    if (u == v) return false;

    uint32_t hi = std::max(u, v);
    if (hi >= adj.size()) expand_to(hi);
    if (hi + 1 > n_nodes_actual) n_nodes_actual = hi + 1;

    EdgeKey key = edge_key(u, v);
    if (!edge_set.insert(key)) return false;

    touch(u);
    touch(v);

    // A directed edge whose reverse is already present adds no undirected
    // structure, so triangles must not be recounted for it.
    bool new_undirected_edge = !directed || !edge_set.contains(make_directed_key(v, u));

    insert_neighbour(u, v);
    if (directed) insert_in_neighbour(v, u);
    else insert_neighbour(v, u);

    if (dsu.unite(u, v)) --n_components_touched;

    if (new_undirected_edge) {
        common_buf.clear();
        uint32_t cnt;
        if (directed) {
            // undirected neighbourhood = out ∪ in; makes the count order-independent
            sorted_union(adj[u].neighbours, in_adj[u], union_u_buf);
            sorted_union(adj[v].neighbours, in_adj[v], union_v_buf);
            cnt = count_common_neighbours(union_u_buf, union_v_buf, common_buf);
        } else {
            cnt = count_common_neighbours(adj[u].neighbours, adj[v].neighbours, common_buf);
        }
        n_triangles += cnt;
        adj[u].triangle_count += cnt;
        adj[v].triangle_count += cnt;
        for (uint32_t w : common_buf) adj[w].triangle_count += 1;
    }

    ++n_edges;
    return true;
}

uint32_t StreamGraph::find_component(uint32_t u) {
    if (!valid_touched_node(u)) {
        throw std::out_of_range("streamgraph: node has not been touched");
    }
    return dsu.find(u);
}

bool StreamGraph::same_component(uint32_t u, uint32_t v) {
    if (!valid_touched_node(u) || !valid_touched_node(v)) return false;
    return dsu.find(u) == dsu.find(v);
}

uint32_t StreamGraph::n_components() const { return n_components_touched; }

uint32_t StreamGraph::degree(uint32_t u) const {
    if (u >= adj.size()) return 0;
    return adj[u].degree;
}

uint32_t StreamGraph::in_degree(uint32_t u) const {
    if (u >= adj.size()) return 0;
    return directed ? static_cast<uint32_t>(in_adj[u].size()) : adj[u].degree;
}

std::vector<uint32_t> StreamGraph::neighbours(uint32_t u) const {
    if (u >= adj.size()) return {};
    return adj[u].neighbours;
}

std::vector<uint32_t> StreamGraph::in_neighbours(uint32_t u) const {
    if (u >= adj.size()) return {};
    return directed ? in_adj[u] : adj[u].neighbours;
}

bool StreamGraph::has_edge(uint32_t u, uint32_t v) const {
    if (u == v) return false;
    return edge_set.contains(edge_key(u, v));
}

uint32_t StreamGraph::component_size(uint32_t u) {
    if (!valid_touched_node(u)) return 0;
    return dsu.comp_size(u);
}

uint32_t StreamGraph::n_nodes() const { return n_touched; }

double StreamGraph::density() const {
    double n = static_cast<double>(n_touched);
    if (n < 2.0) return 0.0;
    double m = static_cast<double>(n_edges);
    double possible = n * (n - 1.0);
    return directed ? (m / possible) : (2.0 * m / possible);
}

double StreamGraph::avg_degree() const {
    if (n_touched == 0) return 0.0;
    double m = static_cast<double>(n_edges);
    double n = static_cast<double>(n_touched);
    return directed ? (m / n) : (2.0 * m / n);
}

uint32_t StreamGraph::max_degree() const { return max_deg_; }

std::vector<uint32_t> StreamGraph::component_nodes(uint32_t u) {
    std::vector<uint32_t> out;
    if (u >= adj.size() || !touched[u]) return out;
    uint32_t root = dsu.find(u);
    for (uint32_t i = 0; i < n_nodes_actual; ++i)
        if (touched[i] && dsu.find(i) == root) out.push_back(i);
    return out;
}

std::vector<uint32_t> StreamGraph::component_ids() {
    std::vector<uint32_t> out;
    out.reserve(n_touched);
    for (uint32_t i = 0; i < n_nodes_actual; ++i)
        if (touched[i]) out.push_back(dsu.find(i));
    return out;
}

}
