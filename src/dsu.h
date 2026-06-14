//Disjoint Set Union (Union-Find) with iterative path compression and union by rank.
//Component counting lives in StreamGraph (Model A), not here.
#pragma once

#include <cstdint>
#include <vector>

namespace streamgraph {

struct DSU {
    std::vector<uint32_t> parent;
    std::vector<uint32_t> rank;
    std::vector<uint32_t> size;   //component size, valid at the root only

    DSU() = default;
    explicit DSU(uint32_t n) { init(n); }

    void init(uint32_t n);
    void grow(uint32_t new_size);

    //Iterative two-pass path compression; avoids stack overflow on deep uncompressed trees.
    uint32_t find(uint32_t x);

    //Union by rank; returns true iff two separate components were merged.
    bool unite(uint32_t x, uint32_t y);

    bool same(uint32_t x, uint32_t y);
    uint32_t comp_size(uint32_t x);
};

}
