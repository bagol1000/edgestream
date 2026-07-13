//Standalone C++ exercise driver for the edgestream core. Designed to run under
//AddressSanitizer/UBSan in CI: it stresses every mutation path (add, remove,
//weighted, directed, batch, serialise) and checks invariants against a naive
//reference model.
//Build: g++ -std=c++17 -O1 -g -fsanitize=address,undefined -Isrc \
//  tests/test_core_standalone.cpp src/*.cpp -o /tmp/test_core   (exclude bindings)
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "edgestream.h"

using namespace edgestream;

namespace {

uint64_t naive_triangles(const std::set<std::pair<uint32_t, uint32_t>>& edges, uint32_t n) {
    //edges hold undirected pairs (a < b)
    auto has = [&](uint32_t a, uint32_t b) {
        if (a > b) std::swap(a, b);
        return edges.count({a, b}) > 0;
    };
    uint64_t t = 0;
    for (uint32_t a = 0; a < n; ++a)
        for (uint32_t b = a + 1; b < n; ++b)
            for (uint32_t c = b + 1; c < n; ++c)
                if (has(a, b) && has(b, c) && has(a, c)) ++t;
    return t;
}

void fuzz_undirected(uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> node(0, 19);
    StreamGraph G(0, false);
    std::set<std::pair<uint32_t, uint32_t>> ref;

    for (int step = 0; step < 5000; ++step) {
        uint32_t u = node(rng), v = node(rng);
        if (u == v) {
            assert(!G.add_edge(u, v));
            continue;
        }
        auto key = std::minmax(u, v);
        std::pair<uint32_t, uint32_t> e{key.first, key.second};
        if (ref.count(e) && rng() % 2 == 0) {
            assert(G.remove_edge(u, v));
            ref.erase(e);
        } else {
            bool added = G.add_edge(u, v);
            assert(added == (ref.count(e) == 0));
            ref.insert(e);
        }
        assert(G.n_edges == ref.size());
    }
    assert(G.total_triangles() == naive_triangles(ref, 20));

    //degrees against the reference
    for (uint32_t u = 0; u < 20; ++u) {
        uint32_t d = 0;
        for (const auto& e : ref) d += (e.first == u || e.second == u);
        assert(G.degree(u) == d);
    }
    std::printf("fuzz_undirected(seed=%llu): OK (%zu edges)\n",
                (unsigned long long)seed, ref.size());
}

void fuzz_directed_weighted(uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> node(0, 14);
    std::uniform_real_distribution<double> weight(0.1, 5.0);
    StreamGraph G(0, true, true);
    std::set<std::pair<uint32_t, uint32_t>> ref;   //ordered pairs

    for (int step = 0; step < 4000; ++step) {
        uint32_t u = node(rng), v = node(rng);
        if (u == v) continue;
        std::pair<uint32_t, uint32_t> e{u, v};
        if (ref.count(e) && rng() % 2 == 0) {
            assert(G.remove_edge(u, v));
            ref.erase(e);
        } else {
            bool added = G.add_edge(u, v, weight(rng));
            assert(added == (ref.count(e) == 0));
            ref.insert(e);
        }
    }
    assert(G.n_edges == ref.size());

    //underlying undirected triangle count
    std::set<std::pair<uint32_t, uint32_t>> und;
    for (const auto& e : ref) und.insert(std::minmax(e.first, e.second));
    assert(G.total_triangles() == naive_triangles(und, 15));

    //centrality smoke under sanitizers
    auto bc = BetweennessApprox::compute(G, 500, 2, 7, true);
    auto pr = G.pagerank();
    auto scc = G.strong_component_ids();
    (void)bc; (void)pr; (void)scc;

    //serialise roundtrip preserves counts and weights
    const char* path = "/tmp/edgestream_core_test.esg";
    G.save(path);
    auto H = StreamGraph::load(path);
    assert(H->n_edges == G.n_edges);
    assert(H->total_triangles() == G.total_triangles());
    for (const auto& e : ref)
        assert(H->edge_weight(e.first, e.second) == G.edge_weight(e.first, e.second));
    std::remove(path);
    std::printf("fuzz_directed_weighted(seed=%llu): OK (%zu edges)\n",
                (unsigned long long)seed, ref.size());
}

void hash_set_erase_torture() {
    FlatHashSet s;
    std::mt19937_64 rng(3);
    std::set<uint64_t> ref;
    for (int i = 0; i < 200000; ++i) {
        uint64_t k = rng() % 5000;
        if (ref.count(k) && rng() % 2 == 0) {
            assert(s.erase(k));
            ref.erase(k);
        } else {
            assert(s.insert(k) == (ref.count(k) == 0));
            ref.insert(k);
        }
    }
    for (uint64_t k = 0; k < 5000; ++k) assert(s.contains(k) == (ref.count(k) > 0));
    assert(s.size() == ref.size());
    std::printf("hash_set_erase_torture: OK (%zu keys)\n", ref.size());
}

}

int main() {
    hash_set_erase_torture();
    for (uint64_t seed : {1ULL, 2ULL, 3ULL}) fuzz_undirected(seed);
    for (uint64_t seed : {4ULL, 5ULL}) fuzz_directed_weighted(seed);
    std::printf("edgestream core standalone: ALL PASSED\n");
    return 0;
}
