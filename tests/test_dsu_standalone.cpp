//Standalone DSU test.
//Build: g++ -std=c++17 -O2 -Isrc tests/test_dsu_standalone.cpp src/dsu.cpp -o /tmp/test_dsu
#include "dsu.h"
#include <cassert>
#include <cstdio>

using edgestream::DSU;

int main() {
    DSU d(10);
    for (uint32_t i = 0; i < 10; ++i) {
        assert(d.find(i) == i);
        assert(d.comp_size(i) == 1);
    }

    assert(d.unite(0, 1) == true);
    assert(d.same(0, 1));
    assert(d.comp_size(0) == 2);

    assert(d.unite(2, 3) == true);
    assert(!d.same(0, 2));

    assert(d.unite(0, 1) == false);   //duplicate unite does nothing

    assert(d.unite(1, 2) == true);    //merge {0,1} with {2,3}
    assert(d.same(0, 3));
    assert(d.comp_size(3) == 4);

    //long chain exercises iterative path compression
    DSU chain(1000);
    int merges = 0;
    for (uint32_t i = 0; i < 999; ++i) merges += chain.unite(i, i + 1);
    assert(merges == 999);
    assert(chain.comp_size(0) == 1000);
    assert(chain.same(0, 999));
    uint32_t r = chain.find(999);
    assert(chain.parent[999] == r);

    std::printf("DSU standalone test: ALL PASSED\n");
    return 0;
}
