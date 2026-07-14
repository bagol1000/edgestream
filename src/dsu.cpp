#include "dsu.h"

namespace edgestream {

void DSU::init(uint32_t n) {
    parent.resize(n);
    rank.assign(n, 0);
    size.assign(n, 1);
    min_id.resize(n);
    for (uint32_t i = 0; i < n; ++i) parent[i] = min_id[i] = i;
}

void DSU::grow(uint32_t new_size) {
    uint32_t old_size = static_cast<uint32_t>(parent.size());
    if (new_size <= old_size) return;
    parent.resize(new_size);
    rank.resize(new_size, 0);
    size.resize(new_size, 1);
    min_id.resize(new_size);
    for (uint32_t i = old_size; i < new_size; ++i) parent[i] = min_id[i] = i;
}

uint32_t DSU::find(uint32_t x) {
    uint32_t root = x;
    while (parent[root] != root) root = parent[root];
    while (parent[x] != root) {
        uint32_t next = parent[x];
        parent[x] = root;
        x = next;
    }
    return root;
}

bool DSU::unite(uint32_t x, uint32_t y) {
    uint32_t rx = find(x);
    uint32_t ry = find(y);
    if (rx == ry) return false;

    //attach the smaller-rank tree under the larger-rank root
    if (rank[rx] < rank[ry]) { uint32_t tmp = rx; rx = ry; ry = tmp; }
    parent[ry] = rx;
    size[rx] += size[ry];
    min_id[rx] = std::min(min_id[rx], min_id[ry]);
    if (rank[rx] == rank[ry]) ++rank[rx];
    return true;
}

bool DSU::same(uint32_t x, uint32_t y) { return find(x) == find(y); }

uint32_t DSU::comp_size(uint32_t x) { return size[find(x)]; }

uint32_t DSU::component_id(uint32_t x) { return min_id[find(x)]; }

}
