// Portable EDGS v3 serialisation. All integers and IEEE-754 doubles are stored
// little-endian. v3 preserves the allocated id range and explicitly touched
// nodes, including isolates. The loader remains compatible with v2 files.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "edgestream.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace edgestream {
namespace {

constexpr uint32_t EDGS_MAGIC = 0x53474445u;        // bytes: "EDGS"
constexpr uint32_t EDGS_V2_MAGIC = 0x45444753u;     // historical bytes: "SGDE"
constexpr uint16_t EDGS_VERSION = 3;
constexpr uint16_t EDGS_LEGACY_VERSION = 2;
constexpr uint8_t FLAG_DIRECTED = 1u << 0;
constexpr uint8_t FLAG_WEIGHTED = 1u << 1;
constexpr uint8_t KNOWN_FLAGS = FLAG_DIRECTED | FLAG_WEIGHTED;

template <typename T>
void write_le(std::ostream& os, T value) {
    static_assert(std::is_unsigned<T>::value, "write_le requires an unsigned integer");
    for (size_t i = 0; i < sizeof(T); ++i)
        os.put(static_cast<char>((value >> (8 * i)) & static_cast<T>(0xff)));
}

template <typename T>
T read_le(std::istream& is, const char* path) {
    static_assert(std::is_unsigned<T>::value, "read_le requires an unsigned integer");
    T value = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        int c = is.get();
        if (c == std::char_traits<char>::eof())
            throw std::runtime_error(std::string("edgestream: corrupt or truncated save file: ") + path);
        value |= static_cast<T>(static_cast<unsigned char>(c)) << (8 * i);
    }
    return value;
}

void write_double_le(std::ostream& os, double value) {
    static_assert(sizeof(double) == sizeof(uint64_t), "EDGS requires 64-bit doubles");
    static_assert(std::numeric_limits<double>::is_iec559, "EDGS requires IEEE-754 doubles");
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    write_le<uint64_t>(os, bits);
}

double read_double_le(std::istream& is, const char* path) {
    uint64_t bits = read_le<uint64_t>(is, path);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void validate_header(uint8_t flags, uint64_t n_ids, uint64_t n_touched,
                     uint64_t n_edges, const char* path) {
    if (flags & ~KNOWN_FLAGS)
        throw std::runtime_error(std::string("edgestream: unknown flags in: ") + path);
    if (n_ids > std::numeric_limits<uint32_t>::max() || n_touched > n_ids)
        throw std::runtime_error(std::string("edgestream: invalid node counts in: ") + path);
    const bool directed = (flags & FLAG_DIRECTED) != 0;
    const uint64_t max_edges = directed
        ? n_touched * (n_touched > 0 ? n_touched - 1 : 0)
        : n_touched * (n_touched > 0 ? n_touched - 1 : 0) / 2;
    if (n_edges > max_edges)
        throw std::runtime_error(std::string("edgestream: invalid edge count in: ") + path);
}

}

void StreamGraph::save(const char* path) const {
    const std::string target(path);
    const std::string temporary = target + ".tmp";
    std::ofstream os(temporary, std::ios::binary | std::ios::trunc);
    if (!os)
        throw std::runtime_error("edgestream: cannot open file for writing: " + target);

    uint8_t flags = (directed ? FLAG_DIRECTED : 0) | (weighted ? FLAG_WEIGHTED : 0);
    write_le<uint32_t>(os, EDGS_MAGIC);
    write_le<uint16_t>(os, EDGS_VERSION);
    write_le<uint8_t>(os, flags);
    write_le<uint64_t>(os, n_nodes_actual);
    write_le<uint64_t>(os, n_touched);
    write_le<uint64_t>(os, n_edges);

    for (uint32_t u = 0; u < n_nodes_actual; ++u)
        if (touched[u]) write_le<uint32_t>(os, u);

    uint64_t written = 0;
    for (uint32_t u = 0; u < n_nodes_actual; ++u) {
        const auto& nb = adj[u].neighbours;
        for (size_t i = 0; i < nb.size(); ++i) {
            uint32_t v = nb[i];
            if (!directed && v < u) continue;
            write_le<uint32_t>(os, u);
            write_le<uint32_t>(os, v);
            if (weighted) write_double_le(os, w_adj[u][i]);
            ++written;
        }
    }
    os.flush();
    if (!os || written != n_edges) {
        os.close();
        std::remove(temporary.c_str());
        throw std::runtime_error("edgestream: failed to write all edges to: " + target);
    }
    os.close();
#ifdef _WIN32
    const bool replaced = MoveFileExA(
        temporary.c_str(), target.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    const bool replaced = std::rename(temporary.c_str(), target.c_str()) == 0;
#endif
    if (!replaced) {
        std::remove(temporary.c_str());
        throw std::runtime_error("edgestream: cannot replace output file: " + target);
    }
}

std::unique_ptr<StreamGraph> StreamGraph::load(const char* path) {
    std::ifstream is(path, std::ios::binary);
    if (!is)
        throw std::runtime_error(std::string("edgestream: cannot open file for reading: ") + path);

    uint32_t magic = read_le<uint32_t>(is, path);
    if (magic != EDGS_MAGIC && magic != EDGS_V2_MAGIC)
        throw std::runtime_error(std::string("edgestream: bad magic (not an EDGS file): ") + path);
    uint16_t version = read_le<uint16_t>(is, path);
    if (version != EDGS_VERSION && version != EDGS_LEGACY_VERSION)
        throw std::runtime_error(std::string("edgestream: unsupported version in: ") + path);
    uint8_t flags = read_le<uint8_t>(is, path);

    uint64_t n_ids = 0, n_touched = 0, n_edges = 0;
    if (version == EDGS_VERSION) {
        n_ids = read_le<uint64_t>(is, path);
        n_touched = read_le<uint64_t>(is, path);
        n_edges = read_le<uint64_t>(is, path);
    } else {
        n_touched = read_le<uint64_t>(is, path);
        n_edges = read_le<uint64_t>(is, path);
        n_ids = 0;  // v2 did not persist the maximum ID; infer it from edges
    }
    if (version == EDGS_VERSION) {
        validate_header(flags, n_ids, n_touched, n_edges, path);
    } else {
        if (flags & ~KNOWN_FLAGS || n_touched > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error(std::string("edgestream: invalid v2 header in: ") + path);
        uint64_t possible = n_touched * (n_touched > 0 ? n_touched - 1 : 0);
        if (!(flags & FLAG_DIRECTED)) possible /= 2;
        if (n_edges > possible)
            throw std::runtime_error(std::string("edgestream: invalid edge count in: ") + path);
    }

    bool weighted = (flags & FLAG_WEIGHTED) != 0;
    auto g = std::make_unique<StreamGraph>(
                                           version == EDGS_VERSION ? static_cast<uint32_t>(n_ids) : 0,
                                           (flags & FLAG_DIRECTED) != 0, weighted);
    g->reserve_edges(n_edges);
    if (version == EDGS_VERSION) {
        uint32_t previous = 0;
        for (uint64_t i = 0; i < n_touched; ++i) {
            uint32_t u = read_le<uint32_t>(is, path);
            if (u >= n_ids || (i > 0 && u <= previous))
                throw std::runtime_error(std::string("edgestream: invalid touched-node list in: ") + path);
            g->add_node(u);
            previous = u;
        }
    }

    for (uint64_t i = 0; i < n_edges; ++i) {
        uint32_t u = read_le<uint32_t>(is, path);
        uint32_t v = read_le<uint32_t>(is, path);
        double weight = weighted ? read_double_le(is, path) : 1.0;
        if ((version == EDGS_VERSION && (u >= n_ids || v >= n_ids)) ||
            u == v || !std::isfinite(weight) || weight <= 0.0 ||
            !g->add_edge(u, v, weight))
            throw std::runtime_error(std::string("edgestream: invalid or duplicate edge in: ") + path);
    }
    if (version == EDGS_VERSION && g->n_nodes() != n_touched)
        throw std::runtime_error(std::string("edgestream: edge references an untouched node in: ") + path);
    if (is.peek() != std::char_traits<char>::eof())
        throw std::runtime_error(std::string("edgestream: trailing data after edge list in: ") + path);
    return g;
}

}
