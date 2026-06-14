//Binary serialisation (little-endian, no external deps).
//Header: magic uint32 "SGPH", version uint16, directed uint8, n_nodes uint64, n_edges uint64.
//Then n_edges x (u uint32, v uint32). load() replays add_edge on every edge, which rebuilds
//the DSU, triangles and adjacency exactly; edge order does not affect the final state.
#include <fstream>
#include <stdexcept>
#include <string>

#include "streamgraph.h"

namespace streamgraph {

namespace {

constexpr uint32_t SGPH_MAGIC = 0x53475048u;   //"SGPH"
constexpr uint16_t SGPH_VERSION = 1;

template <typename T> void write_pod(std::ostream& os, T value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T> T read_pod(std::istream& is, const char* path) {
    T value;
    if (!is.read(reinterpret_cast<char*>(&value), sizeof(T)))
        throw std::runtime_error(std::string("streamgraph: corrupt or truncated save file: ") + path);
    return value;
}

}

void StreamGraph::save(const char* path) const {
    std::ofstream os(path, std::ios::binary);
    if (!os) throw std::runtime_error(std::string("streamgraph: cannot open file for writing: ") + path);

    write_pod<uint32_t>(os, SGPH_MAGIC);
    write_pod<uint16_t>(os, SGPH_VERSION);
    write_pod<uint8_t>(os, directed ? 1 : 0);
    write_pod<uint64_t>(os, static_cast<uint64_t>(n_touched));
    write_pod<uint64_t>(os, n_edges);

    //emit each edge once: undirected only u < v, directed every out-edge
    uint64_t written = 0;
    for (uint32_t u = 0; u < n_nodes_actual; ++u) {
        for (uint32_t v : adj[u].neighbours) {
            if (directed || u < v) {
                write_pod<uint32_t>(os, u);
                write_pod<uint32_t>(os, v);
                ++written;
            }
        }
    }

    if (!os || written != n_edges)
        throw std::runtime_error(std::string("streamgraph: failed to write all edges to: ") + path);
}

std::unique_ptr<StreamGraph> StreamGraph::load(const char* path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) throw std::runtime_error(std::string("streamgraph: cannot open file for reading: ") + path);

    uint32_t magic = read_pod<uint32_t>(is, path);
    if (magic != SGPH_MAGIC)
        throw std::runtime_error(std::string("streamgraph: bad magic (not an SGPH file): ") + path);
    uint16_t version = read_pod<uint16_t>(is, path);
    if (version != SGPH_VERSION)
        throw std::runtime_error(std::string("streamgraph: unsupported version in: ") + path);

    uint8_t directed = read_pod<uint8_t>(is, path);
    (void)read_pod<uint64_t>(is, path);   //n_nodes, informational
    uint64_t n_edges = read_pod<uint64_t>(is, path);

    auto g = std::make_unique<StreamGraph>(0, directed != 0);
    for (uint64_t i = 0; i < n_edges; ++i) {
        uint32_t u = read_pod<uint32_t>(is, path);
        uint32_t v = read_pod<uint32_t>(is, path);
        g->add_edge(u, v);
    }
    return g;
}

}
