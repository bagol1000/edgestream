//Batch edge ingestion. Phase A (parallel): build keys, drop self-loops, sort and dedup.
//Phase B (serial): replay unique edges through add_edge, which also filters edges already
//present and does the DSU + triangle updates that are unsafe to parallelise.
//The weighted path sorts (key, weight) pairs stably so the first occurrence's weight wins.
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "edgestream.h"

#ifdef _OPENMP
#include <omp.h>
#endif

//libstdc++ (GCC) ships an OpenMP-backed parallel sort; fall back to std::sort elsewhere
#if defined(_OPENMP) && defined(__GNUC__) && !defined(__clang__)
#include <parallel/algorithm>
#define SG_SORT(...) __gnu_parallel::sort(__VA_ARGS__)
#else
#define SG_SORT(...) std::sort(__VA_ARGS__)
#endif

namespace edgestream {

namespace {
struct WeightedItem {
    EdgeKey key;
    double weight;
    size_t order;
};
}

uint64_t StreamGraph::add_edges_unweighted(const uint32_t* us, const uint32_t* vs, size_t m, int n_threads) {
    if (n_threads < 0) throw std::invalid_argument("edgestream: n_threads must be non-negative");
#ifdef _OPENMP
    int saved_threads = omp_get_max_threads();
    if (n_threads > 0) omp_set_num_threads(n_threads);
#else
    (void)n_threads;
#endif

    std::vector<EdgeKey> keys;
    keys.reserve(m);
    for (size_t i = 0; i < m; ++i) {
        uint32_t u = us[i], v = vs[i];
        if (u == v) continue;
        //directed distinguishes (u,v) from (v,u); undirected normalises
        keys.push_back(directed ? make_directed_key(u, v) : make_key(u, v));
    }

    SG_SORT(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

#ifdef _OPENMP
    if (n_threads > 0) omp_set_num_threads(saved_threads);
#endif

    uint64_t added = 0;
    for (EdgeKey k : keys) {
        uint32_t u = static_cast<uint32_t>(k >> 32);
        uint32_t v = static_cast<uint32_t>(k & 0xFFFFFFFFu);
        if (add_edge(u, v)) ++added;
    }
    return added;
}

uint64_t StreamGraph::add_edges(const uint32_t* us, const uint32_t* vs, const double* ws,
                                size_t m, int n_threads) {
    if (n_threads < 0) throw std::invalid_argument("edgestream: n_threads must be non-negative");
    if (m == 0) return 0;
    for (size_t i = 0; i < m; ++i) {
        if (us[i] == std::numeric_limits<uint32_t>::max() ||
            vs[i] == std::numeric_limits<uint32_t>::max())
            throw std::out_of_range("edgestream: node id is too large");
        if (weighted && ws != nullptr && (!(ws[i] > 0.0) || !std::isfinite(ws[i])))
            throw std::invalid_argument("edgestream: edge weights must be finite and positive");
    }
    if (ws == nullptr || !weighted) return add_edges_unweighted(us, vs, m, n_threads);

#ifdef _OPENMP
    int saved_threads = omp_get_max_threads();
    if (n_threads > 0) omp_set_num_threads(n_threads);
#endif
    std::vector<WeightedItem> items;
    items.reserve(m);
    for (size_t i = 0; i < m; ++i) {
        if (us[i] == vs[i]) continue;
        items.push_back({
            directed ? make_directed_key(us[i], vs[i]) : make_key(us[i], vs[i]),
            ws[i], i
        });
    }
    SG_SORT(items.begin(), items.end(),
            [](const WeightedItem& a, const WeightedItem& b) {
                return a.key < b.key || (a.key == b.key && a.order < b.order);
            });
    items.erase(std::unique(items.begin(), items.end(),
                            [](const WeightedItem& a, const WeightedItem& b) {
                                return a.key == b.key;
                            }),
                items.end());
#ifdef _OPENMP
    if (n_threads > 0) omp_set_num_threads(saved_threads);
#endif

    uint64_t added = 0;
    for (const auto& it : items) {
        uint32_t u = static_cast<uint32_t>(it.key >> 32);
        uint32_t v = static_cast<uint32_t>(it.key & 0xFFFFFFFFu);
        if (add_edge(u, v, it.weight)) ++added;
    }
    return added;
}

}
