#include "edgestream.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace edgestream {

StreamGraph::StreamGraph(uint32_t n_nodes_initial_, bool directed_, bool weighted_)
    : directed(directed_), weighted(weighted_), n_nodes_initial(n_nodes_initial_) {
    uint32_t cap = (n_nodes_initial_ == 0) ? EDGESTREAM_AUTO_INITIAL_CAPACITY : n_nodes_initial_;
    adj.resize(cap);
    if (directed_) in_adj.resize(cap);
    if (weighted_) w_adj.resize(cap);
    if (weighted_) strength_sum.assign(cap, 0.0);
    dsu.init(cap);
    touched.assign(cap, false);

    // Auto mode starts with no referenced node; pre-allocated mode promises ids up to n_nodes_initial-1
    n_nodes_actual = n_nodes_initial_;

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
    if (weighted) w_adj.resize(new_size);
    if (weighted) strength_sum.resize(new_size, 0.0);
    dsu.grow(static_cast<uint32_t>(new_size));
    touched.resize(new_size, false);
}

void StreamGraph::touch(uint32_t u) {
    if (!touched[u]) {
        touched[u] = true;
        ++n_touched;
        ++n_components_touched;
        ++degree_histogram[0];  // enters the histogram as (out-)degree 0
    }
}

void StreamGraph::histogram_move(uint32_t old_deg, uint32_t new_deg) {
    if (new_deg >= degree_histogram.size()) degree_histogram.resize(new_deg + 1, 0);
    --degree_histogram[old_deg];
    ++degree_histogram[new_deg];
    if (new_deg > max_deg_) {
        max_deg_ = new_deg;
    } else if (old_deg == max_deg_ && degree_histogram[max_deg_] == 0) {
        while (max_deg_ > 0 && degree_histogram[max_deg_] == 0) --max_deg_;
    }
}

void StreamGraph::insert_neighbour(uint32_t u, uint32_t v, double w) {
    auto& a = adj[u];
    auto pos = std::lower_bound(a.neighbours.begin(), a.neighbours.end(), v);
    size_t idx = static_cast<size_t>(pos - a.neighbours.begin());
    a.neighbours.insert(pos, v);
    if (weighted) {
        w_adj[u].insert(w_adj[u].begin() + idx, w);
        strength_sum[u] += w;
    }
    histogram_move(a.degree, a.degree + 1);
    ++a.degree;
}

void StreamGraph::insert_in_neighbour(uint32_t u, uint32_t v) {
    auto& in = in_adj[u];
    in.insert(std::lower_bound(in.begin(), in.end(), v), v);
}

void StreamGraph::erase_neighbour(uint32_t u, uint32_t v) {
    auto& a = adj[u];
    auto pos = std::lower_bound(a.neighbours.begin(), a.neighbours.end(), v);
    size_t idx = static_cast<size_t>(pos - a.neighbours.begin());
    a.neighbours.erase(pos);
    if (weighted) {
        strength_sum[u] -= w_adj[u][idx];
        w_adj[u].erase(w_adj[u].begin() + idx);
    }
    histogram_move(a.degree, a.degree - 1);
    --a.degree;
}

void StreamGraph::erase_in_neighbour(uint32_t u, uint32_t v) {
    auto& in = in_adj[u];
    in.erase(std::lower_bound(in.begin(), in.end(), v));
}

uint32_t StreamGraph::undirected_common_count(uint32_t u, uint32_t v) {
    common_buf.clear();
    if (directed) {
        // undirected neighbourhood = out ∪ in; makes the count order-independent
        sorted_union(adj[u].neighbours, in_adj[u], union_u_buf);
        sorted_union(adj[v].neighbours, in_adj[v], union_v_buf);
        return count_common_neighbours(union_u_buf, union_v_buf, common_buf);
    }
    return count_common_neighbours(adj[u].neighbours, adj[v].neighbours, common_buf);
}

bool StreamGraph::add_edge(uint32_t u, uint32_t v, double w) {
    if (u == v) return false;
    if (u == std::numeric_limits<uint32_t>::max() ||
        v == std::numeric_limits<uint32_t>::max())
        throw std::out_of_range("edgestream: node id is too large");
    if (weighted && (!(w > 0.0) || !std::isfinite(w))) {
        throw std::invalid_argument("edgestream: edge weights must be finite and positive");
    }

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

    uint32_t cnt = 0;
    std::vector<std::pair<uint32_t, double>> old_clustering;
    if (new_undirected_edge) {
        cnt = undirected_common_count(u, v);
        old_clustering.reserve(common_buf.size() + 2);
        old_clustering.emplace_back(u, clustering_coefficient(u));
        old_clustering.emplace_back(v, clustering_coefficient(v));
        for (uint32_t x : common_buf)
            old_clustering.emplace_back(x, clustering_coefficient(x));
    }

    insert_neighbour(u, v, w);
    if (directed) insert_in_neighbour(v, u);
    else insert_neighbour(v, u, w);

    // After a removal the DSU is stale; skip incremental union, the next
    // component query rebuilds from scratch anyway.
    if (!comps_dirty && dsu.unite(u, v)) --n_components_touched;

    if (new_undirected_edge) {
        n_triangles += cnt;
        adj[u].triangle_count += cnt;
        adj[v].triangle_count += cnt;
        for (uint32_t w2 : common_buf) adj[w2].triangle_count += 1;
        for (const auto& item : old_clustering)
            clustering_sum += clustering_coefficient(item.first) - item.second;
    }

    total_weight_sum += weighted ? w : 1.0;
    ++n_edges;
    return true;
}

bool StreamGraph::add_node(uint32_t u) {
    if (u == std::numeric_limits<uint32_t>::max())
        throw std::out_of_range("edgestream: node id is too large");
    if (u >= adj.size()) expand_to(u);
    if (u + 1 > n_nodes_actual) n_nodes_actual = u + 1;
    bool fresh = !touched[u];
    touch(u);
    return fresh;
}

uint64_t StreamGraph::add_nodes(uint32_t start, uint32_t count) {
    if (count == 0) return 0;
    uint64_t end = static_cast<uint64_t>(start) + count;
    if (end > std::numeric_limits<uint32_t>::max())
        throw std::out_of_range("edgestream: node id range is too large");
    uint64_t added = 0;
    reserve_nodes(static_cast<uint32_t>(end));
    for (uint32_t u = start; u < static_cast<uint32_t>(end); ++u)
        if (add_node(u)) ++added;
    return added;
}

std::vector<uint32_t> StreamGraph::nodes() const {
    std::vector<uint32_t> out;
    out.reserve(n_touched);
    for (uint32_t u = 0; u < n_nodes_actual; ++u)
        if (touched[u]) out.push_back(u);
    return out;
}

void StreamGraph::reserve_nodes(uint32_t n) {
    if (n == 0) return;
    expand_to(n - 1);
    if (n > n_nodes_actual) n_nodes_actual = n;
}

void StreamGraph::reserve_edges(uint64_t m) {
    if (m > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        throw std::length_error("edgestream: edge reservation is too large");
    edge_set.reserve(static_cast<size_t>(m));
}

void StreamGraph::clear() {
    for (auto& a : adj) a = AdjList{};
    for (auto& a : in_adj) a.clear();
    for (auto& a : w_adj) a.clear();
    std::fill(strength_sum.begin(), strength_sum.end(), 0.0);
    dsu.init(static_cast<uint32_t>(adj.size()));
    edge_set.clear();
    n_edges = 0;
    n_triangles = 0;
    clustering_sum = 0.0;
    total_weight_sum = 0.0;
    degree_histogram.assign(1, 0);
    std::fill(touched.begin(), touched.end(), false);
    n_touched = 0;
    n_components_touched = 0;
    n_nodes_actual = n_nodes_initial;
    max_deg_ = 0;
    comps_dirty = false;
}

bool StreamGraph::remove_edge(uint32_t u, uint32_t v) {
    if (u == v) return false;
    if (u >= adj.size() || v >= adj.size()) return false;
    if (!edge_set.erase(edge_key(u, v))) return false;

    // Undirected structure disappears unless the reverse directed edge remains
    bool undirected_loss = !directed || !edge_set.contains(make_directed_key(v, u));

    // Triangles must be recounted while both adjacency entries still exist
    std::vector<std::pair<uint32_t, double>> old_clustering;
    if (undirected_loss) {
        uint32_t cnt = undirected_common_count(u, v);
        old_clustering.reserve(common_buf.size() + 2);
        old_clustering.emplace_back(u, clustering_coefficient(u));
        old_clustering.emplace_back(v, clustering_coefficient(v));
        for (uint32_t x : common_buf)
            old_clustering.emplace_back(x, clustering_coefficient(x));
        n_triangles -= cnt;
        adj[u].triangle_count -= cnt;
        adj[v].triangle_count -= cnt;
        for (uint32_t w2 : common_buf) adj[w2].triangle_count -= 1;
    }

    if (weighted) {
        const auto& nb = adj[u].neighbours;
        auto pos = std::lower_bound(nb.begin(), nb.end(), v);
        total_weight_sum -= w_adj[u][static_cast<size_t>(pos - nb.begin())];
    } else {
        total_weight_sum -= 1.0;
    }

    erase_neighbour(u, v);
    if (directed) erase_in_neighbour(v, u);
    else erase_neighbour(v, u);

    if (undirected_loss)
        for (const auto& item : old_clustering)
            clustering_sum += clustering_coefficient(item.first) - item.second;

    --n_edges;
    // Union-Find cannot split; rebuild lazily on the next component query
    comps_dirty = true;
    return true;
}

bool StreamGraph::update_edge_weight(uint32_t u, uint32_t v, double w) {
    if (!weighted)
        throw std::logic_error("edgestream: cannot update weights on an unweighted graph");
    if (!(w > 0.0) || !std::isfinite(w))
        throw std::invalid_argument("edgestream: edge weights must be finite and positive");
    if (!has_edge(u, v)) return false;
    auto& nb = adj[u].neighbours;
    size_t i = static_cast<size_t>(std::lower_bound(nb.begin(), nb.end(), v) - nb.begin());
    double old = w_adj[u][i];
    double delta = w - old;
    w_adj[u][i] = w;
    strength_sum[u] += delta;
    if (!directed) {
        auto& rnb = adj[v].neighbours;
        size_t j = static_cast<size_t>(std::lower_bound(rnb.begin(), rnb.end(), u) - rnb.begin());
        w_adj[v][j] = w;
        strength_sum[v] += delta;
    }
    total_weight_sum += delta;
    return true;
}

void StreamGraph::ensure_components() {
    if (!comps_dirty) return;
    dsu.init(static_cast<uint32_t>(adj.size()));
    n_components_touched = n_touched;
    for (uint32_t u = 0; u < n_nodes_actual; ++u) {
        for (uint32_t v : adj[u].neighbours) {
            if (!directed && v < u) continue;  // each undirected edge once
            if (dsu.unite(u, v)) --n_components_touched;
        }
    }
    comps_dirty = false;
}

uint32_t StreamGraph::find_component(uint32_t u) {
    if (!valid_touched_node(u)) {
        throw std::out_of_range("edgestream: node has not been touched");
    }
    ensure_components();
    return dsu.component_id(u);
}

bool StreamGraph::same_component(uint32_t u, uint32_t v) {
    if (!valid_touched_node(u) || !valid_touched_node(v)) return false;
    ensure_components();
    return dsu.find(u) == dsu.find(v);
}

uint32_t StreamGraph::n_components() {
    ensure_components();
    return n_components_touched;
}

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

double StreamGraph::edge_weight(uint32_t u, uint32_t v) const {
    if (!has_edge(u, v)) throw std::out_of_range("edgestream: edge not found");
    if (!weighted) return 1.0;
    // has_edge guarantees v is in adj[u].neighbours (undirected stores both sides)
    const auto& nb = adj[u].neighbours;
    auto pos = std::lower_bound(nb.begin(), nb.end(), v);
    return w_adj[u][static_cast<size_t>(pos - nb.begin())];
}

double StreamGraph::strength(uint32_t u) const {
    if (u >= adj.size()) return 0.0;
    if (!weighted) return static_cast<double>(adj[u].degree);
    return strength_sum[u];
}

double StreamGraph::total_weight() const { return total_weight_sum; }

uint32_t StreamGraph::component_size(uint32_t u) {
    if (!valid_touched_node(u)) return 0;
    ensure_components();
    return dsu.comp_size(u);
}

uint32_t StreamGraph::n_nodes() const { return n_touched; }

double StreamGraph::clustering_coefficient(uint32_t u) const {
    if (u >= adj.size()) return 0.0;
    // degree on the underlying undirected graph
    double d;
    if (directed) {
        std::vector<uint32_t> un;
        sorted_union(adj[u].neighbours, in_adj[u], un);
        d = static_cast<double>(un.size());
    } else {
        d = static_cast<double>(adj[u].degree);
    }
    if (d < 2.0) return 0.0;
    return 2.0 * static_cast<double>(adj[u].triangle_count) / (d * (d - 1.0));
}

double StreamGraph::avg_clustering() const {
    if (n_touched == 0) return 0.0;
    return clustering_sum / static_cast<double>(n_touched);
}

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
    ensure_components();
    uint32_t root = dsu.find(u);
    for (uint32_t i = 0; i < n_nodes_actual; ++i)
        if (touched[i] && dsu.find(i) == root) out.push_back(i);
    return out;
}

std::vector<uint32_t> StreamGraph::component_ids() {
    ensure_components();
    std::vector<uint32_t> out;
    out.reserve(n_touched);
    for (uint32_t i = 0; i < n_nodes_actual; ++i)
        if (touched[i]) out.push_back(dsu.component_id(i));
    return out;
}

void StreamGraph::edge_list(std::vector<uint32_t>& us, std::vector<uint32_t>& vs,
                            std::vector<double>* ws) const {
    us.clear(); vs.clear();
    if (ws) ws->clear();
    us.reserve(n_edges); vs.reserve(n_edges);
    if (ws) ws->reserve(n_edges);
    for (uint32_t u = 0; u < n_nodes_actual; ++u) {
        const auto& nb = adj[u].neighbours;
        for (size_t i = 0; i < nb.size(); ++i) {
            uint32_t v = nb[i];
            if (!directed && v < u) continue;  // each undirected edge once (u < v)
            us.push_back(u);
            vs.push_back(v);
            if (ws) ws->push_back(weighted ? w_adj[u][i] : 1.0);
        }
    }
}

}
