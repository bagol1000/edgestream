//Approximate betweenness via random (s,t) pair sampling.
//Per pair: BFS from s gives dist and sigma (shortest-path counts from s); back-propagation
//from t gives g[v] (shortest paths v->t); a node's share of s-t paths is sigma[v]*g[v]/sigma[t].
//Summed dependency is scaled by total_pairs/k, then normalised by the max into [0,1].
//Pairs are independent, so OpenMP parallel with per-thread scratch and a private accumulator.
#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "streamgraph.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace streamgraph {

std::vector<double> BetweennessApprox::compute(const StreamGraph& G, int k, int n_threads, uint64_t seed) {
    const uint32_t N = G.n_nodes_actual;

    //touched nodes are the only valid pair endpoints
    std::vector<uint32_t> nodes;
    nodes.reserve(G.n_touched);
    for (uint32_t i = 0; i < N; ++i)
        if (G.touched[i]) nodes.push_back(i);

    const size_t nt = nodes.size();
    if (nt == 0) return {};
    std::vector<double> bc(N, 0.0);
    if (nt < 2 || k <= 0) return bc;

    const uint64_t total_pairs = static_cast<uint64_t>(nt) * (nt - 1) / 2;
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    bool exact = static_cast<uint64_t>(k) >= total_pairs;

    if (exact) {
        //k clamped to all pairs => exact over every unordered pair
        pairs.reserve(total_pairs);
        for (size_t i = 0; i < nt; ++i)
            for (size_t j = i + 1; j < nt; ++j)
                pairs.emplace_back(nodes[i], nodes[j]);
    } else {
        pairs.reserve(k);
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<size_t> pick(0, nt - 1);
        for (int s = 0; s < k; ++s) {
            size_t a = pick(rng), b = pick(rng);
            while (a == b) b = pick(rng);
            pairs.emplace_back(nodes[a], nodes[b]);
        }
    }
    const size_t n_pairs = pairs.size();
    const double scale = static_cast<double>(total_pairs) / static_cast<double>(n_pairs);

    int nthreads = 1;
#ifdef _OPENMP
    int saved = omp_get_max_threads();
    if (n_threads > 0) omp_set_num_threads(n_threads);
    nthreads = (n_threads > 0) ? n_threads : omp_get_max_threads();
#else
    (void)n_threads;
#endif
    std::vector<std::vector<double>> tcounts(nthreads, std::vector<double>(N, 0.0));

#ifdef _OPENMP
#pragma omp parallel
#endif
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
        std::vector<double>& acc = tcounts[tid];

        std::vector<int32_t> dist(N, -1);
        std::vector<double> sigma(N, 0.0);   //shortest paths from s
        std::vector<double> g(N, 0.0);       //shortest paths to t
        std::vector<uint32_t> queue(N);
        std::vector<uint32_t> order;
        order.reserve(N);

#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
        for (long long p = 0; p < static_cast<long long>(n_pairs); ++p) {
            uint32_t s = pairs[p].first;
            uint32_t t = pairs[p].second;

            //BFS from s
            size_t head = 0, tail = 0;
            dist[s] = 0;
            sigma[s] = 1.0;
            queue[tail++] = s;
            order.clear();
            while (head < tail) {
                uint32_t u = queue[head++];
                order.push_back(u);
                int32_t du = dist[u];
                for (uint32_t w : G.adj[u].neighbours) {
                    if (dist[w] < 0) {
                        dist[w] = du + 1;
                        queue[tail++] = w;
                    }
                    if (dist[w] == du + 1) sigma[w] += sigma[u];
                }
            }

            //back-propagate from t along the shortest-path DAG (only if reachable)
            if (dist[t] >= 0) {
                g[t] = 1.0;
                //order is non-decreasing distance; walk it backwards
                for (size_t idx = order.size(); idx-- > 0;) {
                    uint32_t w = order[idx];
                    double gw = g[w];
                    if (gw == 0.0) continue;
                    int32_t dw = dist[w];
                    for (uint32_t v : G.adj[w].neighbours)
                        if (dist[v] == dw - 1) g[v] += gw;   //v is closer to s
                }
                double sigma_t = sigma[t];
                for (uint32_t v : order) {
                    if (v == s || v == t) continue;
                    if (g[v] > 0.0) acc[v] += (sigma[v] * g[v]) / sigma_t;
                }
            }

            //reset only the nodes this BFS touched
            for (uint32_t u : order) {
                dist[u] = -1;
                sigma[u] = 0.0;
                g[u] = 0.0;
            }
        }
    }

#ifdef _OPENMP
    if (n_threads > 0) omp_set_num_threads(saved);
#endif

    //reduce per-thread accumulators in tid order (deterministic)
    for (int th = 0; th < nthreads; ++th) {
        const std::vector<double>& acc = tcounts[th];
        for (uint32_t v = 0; v < N; ++v) bc[v] += acc[v];
    }

    //scale to a full-betweenness estimate, then normalise into [0,1]
    double maxv = 0.0;
    for (uint32_t v = 0; v < N; ++v) {
        bc[v] *= scale;
        if (bc[v] > maxv) maxv = bc[v];
    }
    if (maxv > 0.0)
        for (uint32_t v = 0; v < N; ++v) bc[v] /= maxv;

    return bc;
}

}
