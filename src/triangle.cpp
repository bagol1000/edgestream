#include "streamgraph.h"

namespace streamgraph {

uint32_t count_common_neighbours(const AdjList& a, const AdjList& b, std::vector<uint32_t>& common_out) {
    const std::vector<uint32_t>& A = a.neighbours;
    const std::vector<uint32_t>& B = b.neighbours;
    //sorted merge, O(deg_a + deg_b), sequential and cache-friendly
    size_t i = 0, j = 0;
    uint32_t count = 0;
    while (i < A.size() && j < B.size()) {
        if (A[i] < B[j]) {
            ++i;
        } else if (A[i] > B[j]) {
            ++j;
        } else {
            common_out.push_back(A[i]);
            ++count;
            ++i;
            ++j;
        }
    }
    return count;
}

uint64_t StreamGraph::total_triangles() const { return n_triangles; }

uint32_t StreamGraph::local_triangles(uint32_t u) const {
    if (u >= adj.size()) return 0;
    return adj[u].triangle_count;
}

std::vector<uint32_t> StreamGraph::all_local_triangles() const {
    std::vector<uint32_t> out(n_nodes_actual, 0);
    for (uint32_t i = 0; i < n_nodes_actual; ++i) out[i] = adj[i].triangle_count;
    return out;
}

}
