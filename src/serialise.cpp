//Binary serialisation (little-endian, no external deps). Format v2:
//Header: magic uint32 "EDGS", version uint16 (=2), flags uint8 (bit0 directed,
//bit1 weighted), n_nodes uint64, n_edges uint64.
//Then n_edges x (u uint32, v uint32[, w float64 when weighted]).
//load() replays add_edge on every edge, which rebuilds the DSU, triangles and
//adjacency exactly; edge order does not affect the final state.
#include <fstream>
#include <stdexcept>
#include <string>

#include "edgestream.h"

namespace edgestream {

namespace {

constexpr uint32_t EDGS_MAGIC = 0x45444753u;   //"EDGS"
constexpr uint16_t EDGS_VERSION = 2;
constexpr uint8_t FLAG_DIRECTED = 1u << 0;
constexpr uint8_t FLAG_WEIGHTED = 1u << 1;

template <typename T> void write_pod(std::ostream& os, T value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T> T read_pod(std::istream& is, const char* path) {
    T value;
    if (!is.read(reinterpret_cast<char*>(&value), sizeof(T)))
        throw std::runtime_error(std::string("edgestream: corrupt or truncated save file: ") + path);
    return value;
}

}

void StreamGraph::save(const char* path) const {
    std::ofstream os(path, std::ios::binary);
    if (!os) throw std::runtime_error(std::string("edgestream: cannot open file for writing: ") + path);

    uint8_t flags = 0;
    if (directed) flags |= FLAG_DIRECTED;
    if (weighted) flags |= FLAG_WEIGHTED;

    write_pod<uint32_t>(os, EDGS_MAGIC);
    write_pod<uint16_t>(os, EDGS_VERSION);
    write_pod<uint8_t>(os, flags);
    write_pod<uint64_t>(os, static_cast<uint64_t>(n_touched));
    write_pod<uint64_t>(os, n_edges);

    //emit each edge once: undirected only u < v, directed every out-edge
    uint64_t written = 0;
    for (uint32_t u = 0; u < n_nodes_actual; ++u) {
        const auto& nb = adj[u].neighbours;
        for (size_t i = 0; i < nb.size(); ++i) {
            uint32_t v = nb[i];
            if (directed || u < v) {
                write_pod<uint32_t>(os, u);
                write_pod<uint32_t>(os, v);
                if (weighted) write_pod<double>(os, w_adj[u][i]);
                ++written;
            }
        }
    }

    if (!os || written != n_edges)
        throw std::runtime_error(std::string("edgestream: failed to write all edges to: ") + path);
}

std::unique_ptr<StreamGraph> StreamGraph::load(const char* path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) throw std::runtime_error(std::string("edgestream: cannot open file for reading: ") + path);

    uint32_t magic = read_pod<uint32_t>(is, path);
    if (magic != EDGS_MAGIC)
        throw std::runtime_error(std::string("edgestream: bad magic (not an EDGS file): ") + path);
    uint16_t version = read_pod<uint16_t>(is, path);
    if (version != EDGS_VERSION)
        throw std::runtime_error(std::string("edgestream: unsupported version in: ") + path);

    uint8_t flags = read_pod<uint8_t>(is, path);
    (void)read_pod<uint64_t>(is, path);   //n_nodes, informational
    uint64_t n_edges = read_pod<uint64_t>(is, path);

    bool w = (flags & FLAG_WEIGHTED) != 0;
    auto g = std::make_unique<StreamGraph>(0, (flags & FLAG_DIRECTED) != 0, w);
    for (uint64_t i = 0; i < n_edges; ++i) {
        uint32_t u = read_pod<uint32_t>(is, path);
        uint32_t v = read_pod<uint32_t>(is, path);
        double weight = w ? read_pod<double>(is, path) : 1.0;
        g->add_edge(u, v, weight);
    }
    if (is.peek() != std::char_traits<char>::eof())
        throw std::runtime_error(std::string("edgestream: trailing data after edge list in: ") + path);
    return g;
}

}
