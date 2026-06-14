//Batch edge ingestion. Phase A (parallel): build keys, drop self-loops, sort and dedup.
//Phase B (serial): replay unique edges through add_edge, which also filters edges already
//present and does the DSU + triangle updates that are unsafe to parallelise.
#include <algorithm>
#include <vector>

#include "streamgraph.h"

#ifdef _OPENMP
#include <omp.h>
#endif

//libstdc++ (GCC) ships an OpenMP-backed parallel sort; fall back to std::sort elsewhere
#if defined(_OPENMP) && defined(__GNUC__) && !defined(__clang__)
#include <parallel/algorithm>
#define SG_SORT(first, last) __gnu_parallel::sort((first), (last))
#else
#define SG_SORT(first, last) std::sort((first), (last))
#endif

namespace streamgraph {

uint64_t StreamGraph::add_edges(const uint32_t* us, const uint32_t* vs, size_t m, int n_threads) {
    if (m == 0) return 0;

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
        keys.push_back(directed ? ((static_cast<uint64_t>(u) << 32) | v) : make_key(u, v));
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

}
