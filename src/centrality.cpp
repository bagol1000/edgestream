//On-demand whole-graph algorithms: strongly connected components (iterative
//Tarjan, no recursion so deep graphs cannot overflow the stack) and PageRank
//(power iteration; weighted graphs distribute rank proportionally to weights).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "edgestream.h"

namespace edgestream {

namespace {
constexpr uint32_t UNSET = UINT32_MAX;
}

std::vector<uint32_t> StreamGraph::strong_component_ids() const {
    const uint32_t N = n_nodes_actual;
    std::vector<uint32_t> index(N, UNSET), low(N, 0), comp(N, UNSET);
    std::vector<bool> on_stack(N, false);
    std::vector<uint32_t> tarjan_stack;
    std::vector<uint32_t> scc_nodes;                    //scratch for one popped SCC
    std::vector<std::pair<uint32_t, size_t>> dfs;       //(node, next out-edge index)
    uint32_t next_index = 0;

    for (uint32_t s = 0; s < N; ++s) {
        if (!touched[s] || index[s] != UNSET) continue;
        dfs.emplace_back(s, 0);
        while (!dfs.empty()) {
            uint32_t u = dfs.back().first;
            size_t ci = dfs.back().second;
            if (ci == 0) {
                index[u] = low[u] = next_index++;
                tarjan_stack.push_back(u);
                on_stack[u] = true;
            }
            if (ci < adj[u].neighbours.size()) {
                ++dfs.back().second;
                uint32_t v = adj[u].neighbours[ci];
                if (index[v] == UNSET) {
                    dfs.emplace_back(v, 0);
                } else if (on_stack[v]) {
                    low[u] = std::min(low[u], index[v]);
                }
            } else {
                //u finished: root check, then propagate low to the parent
                if (low[u] == index[u]) {
                    scc_nodes.clear();
                    uint32_t min_id = UNSET;
                    while (true) {
                        uint32_t w = tarjan_stack.back();
                        tarjan_stack.pop_back();
                        on_stack[w] = false;
                        scc_nodes.push_back(w);
                        min_id = std::min(min_id, w);
                        if (w == u) break;
                    }
                    //label every member with the smallest node id in the SCC
                    for (uint32_t w : scc_nodes) comp[w] = min_id;
                }
                dfs.pop_back();
                if (!dfs.empty()) {
                    uint32_t parent = dfs.back().first;
                    low[parent] = std::min(low[parent], low[u]);
                }
            }
        }
    }

    std::vector<uint32_t> out;
    out.reserve(n_touched);
    for (uint32_t i = 0; i < N; ++i)
        if (touched[i]) out.push_back(comp[i]);
    return out;
}

uint32_t StreamGraph::n_strong_components() const {
    std::vector<uint32_t> ids = strong_component_ids();
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return static_cast<uint32_t>(ids.size());
}

std::vector<double> StreamGraph::pagerank(double damping, double tol, int max_iter) const {
    const uint32_t N = n_nodes_actual;
    std::vector<double> pr(N, 0.0);
    if (n_touched == 0) return pr;

    const double n = static_cast<double>(n_touched);
    const double init = 1.0 / n;
    for (uint32_t i = 0; i < N; ++i)
        if (touched[i]) pr[i] = init;

    //out-strength per node: rank flows along out-edges, split by weight
    std::vector<double> out_strength(N, 0.0);
    for (uint32_t u = 0; u < N; ++u) {
        if (!touched[u]) continue;
        out_strength[u] = weighted ? strength(u) : static_cast<double>(adj[u].degree);
    }

    std::vector<double> nxt(N, 0.0);
    for (int iter = 0; iter < max_iter; ++iter) {
        double dangling = 0.0;
        for (uint32_t u = 0; u < N; ++u)
            if (touched[u] && out_strength[u] == 0.0) dangling += pr[u];

        const double base = (1.0 - damping) / n + damping * dangling / n;
        for (uint32_t i = 0; i < N; ++i) nxt[i] = touched[i] ? base : 0.0;

        for (uint32_t u = 0; u < N; ++u) {
            if (!touched[u] || out_strength[u] == 0.0) continue;
            const double factor = damping * pr[u] / out_strength[u];
            const auto& nb = adj[u].neighbours;
            for (size_t i = 0; i < nb.size(); ++i)
                nxt[nb[i]] += factor * (weighted ? w_adj[u][i] : 1.0);
        }

        double diff = 0.0;
        for (uint32_t i = 0; i < N; ++i)
            if (touched[i]) diff += std::abs(nxt[i] - pr[i]);
        pr.swap(nxt);
        if (diff < tol) break;
    }
    return pr;
}

}
