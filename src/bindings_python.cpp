// pybind11 bindings exposing StreamGraph to Python; conversions happen here at the boundary.
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "edgestream.h"

namespace py = pybind11;
using edgestream::StreamGraph;

static py::array_t<uint32_t> to_uint32_array(const std::vector<uint32_t>& v) {
    py::array_t<uint32_t> out(static_cast<py::ssize_t>(v.size()));
    if (!v.empty()) std::copy(v.begin(), v.end(), out.mutable_data());
    return out;
}

static py::array_t<double> to_double_array(const std::vector<double>& v) {
    py::array_t<double> out(static_cast<py::ssize_t>(v.size()));
    if (!v.empty()) std::copy(v.begin(), v.end(), out.mutable_data());
    return out;
}

static py::array_t<uint32_t> neighbours_np(const StreamGraph& G, uint32_t u) {
    return to_uint32_array(G.neighbours(u));
}

static py::array_t<uint32_t> in_neighbours_np(const StreamGraph& G, uint32_t u) {
    return to_uint32_array(G.in_neighbours(u));
}

static py::array_t<uint32_t> all_local_triangles_np(const StreamGraph& G) {
    return to_uint32_array(G.all_local_triangles());
}

using U32Array = py::array_t<uint32_t, py::array::c_style>;
using F64Array = py::array_t<double, py::array::c_style | py::array::forcecast>;

static uint64_t add_edges_np(
    StreamGraph& G,
    U32Array us,
    U32Array vs,
    py::object ws,
    int n_threads
) {
    if (us.ndim() != 1 || vs.ndim() != 1) {
        throw std::invalid_argument("us and vs must be 1-D uint32 arrays");
    }
    if (us.shape(0) != vs.shape(0)) {
        throw std::invalid_argument("us and vs must have the same length");
    }
    auto bu = us.request();
    auto bv = vs.request();
    const uint32_t* pu = static_cast<const uint32_t*>(bu.ptr);
    const uint32_t* pv = static_cast<const uint32_t*>(bv.ptr);
    size_t m = static_cast<size_t>(us.shape(0));

    const double* pw = nullptr;
    F64Array warr;
    if (!ws.is_none()) {
        warr = F64Array::ensure(ws);
        if (warr.ndim() != 1 || warr.shape(0) != us.shape(0)) {
            throw std::invalid_argument("ws must be a 1-D array of the same length as us/vs");
        }
        pw = static_cast<const double*>(warr.request().ptr);
    }

    return G.add_edges(pu, pv, pw, m, n_threads);
}

static py::array_t<double> betweenness_approx_np(
    const StreamGraph& G,
    int k,
    int n_threads,
    uint64_t seed,
    bool normalise
) {
    std::vector<double> bc;
    bc = edgestream::BetweennessApprox::compute(G, k, n_threads, seed, normalise);
    return to_double_array(bc);
}

static py::array_t<double> pagerank_np(const StreamGraph& G, double damping, double tol, int max_iter) {
    std::vector<double> pr;
    pr = G.pagerank(damping, tol, max_iter);
    return to_double_array(pr);
}

static py::tuple edge_list_np(const StreamGraph& G, bool weights) {
    std::vector<uint32_t> us, vs;
    std::vector<double> ws;
    G.edge_list(us, vs, weights ? &ws : nullptr);
    if (weights) return py::make_tuple(to_uint32_array(us), to_uint32_array(vs), to_double_array(ws));
    return py::make_tuple(to_uint32_array(us), to_uint32_array(vs));
}

static py::array_t<uint64_t> degree_histogram_np(const StreamGraph& G) {
    const std::vector<uint64_t>& h = G.degree_histogram;
    py::array_t<uint64_t> out(static_cast<py::ssize_t>(h.size()));
    if (!h.empty()) std::copy(h.begin(), h.end(), out.mutable_data());
    return out;
}

PYBIND11_MODULE(_edgestream, m) {
    m.doc() = "edgestream C++ core: streaming components, triangles, statistics and centrality.";

    py::class_<StreamGraph>(m, "StreamGraph")
        .def(
            py::init([](uint32_t n_nodes, bool directed, bool weighted) {
                return std::make_unique<StreamGraph>(n_nodes, directed, weighted);
            }),
            py::arg("n_nodes") = 0,
            py::arg("directed") = false,
            py::arg("weighted") = false,
            "Create a StreamGraph. n_nodes=0 auto-grows; n_nodes=N pre-allocates N slots. "
            "weighted=True stores a positive weight per edge."
        )
        .def(
            "add_edge",
            &StreamGraph::add_edge,
            py::arg("u"),
            py::arg("v"),
            py::arg("w") = 1.0,
            "Add edge (u, v) with optional weight w (weighted graphs only). "
            "True if new, False if a duplicate or self-loop (duplicates keep the stored weight)."
        )
        .def("add_node", &StreamGraph::add_node, py::arg("u"),
             "Add an isolated node explicitly; True when it was new.")
        .def("add_nodes", &StreamGraph::add_nodes, py::arg("start"), py::arg("count"),
             "Add count consecutive node ids starting at start.")
        .def("nodes", [](const StreamGraph& G) { return to_uint32_array(G.nodes()); },
             "Sorted uint32 array of touched node ids.")
        .def("reserve_nodes", &StreamGraph::reserve_nodes, py::arg("n"),
             "Reserve the id range [0, n) without touching nodes.")
        .def("reserve_edges", &StreamGraph::reserve_edges, py::arg("m"),
             "Reserve storage for approximately m distinct edges.")
        .def("clear", &StreamGraph::clear, "Remove all nodes and edges while retaining allocated storage.")
        .def(
            "remove_edge",
            &StreamGraph::remove_edge,
            py::arg("u"),
            py::arg("v"),
            "Remove edge (u, v); True if it was present. Triangles, degrees and weights "
            "update exactly; the next component query rebuilds Union-Find in O(n + m)."
        )
        .def("update_edge_weight", &StreamGraph::update_edge_weight,
             py::arg("u"), py::arg("v"), py::arg("w"),
             "Replace the weight of an existing weighted edge; False when absent.")
        .def(
            "add_edges",
            &add_edges_np,
            py::arg("us"),
            py::arg("vs"),
            py::arg("ws") = py::none(),
            py::arg("n_threads") = 0,
            "Batch-add edges from 1-D arrays cast to contiguous uint32 buffers, with an "
            "optional float64 weight array. Returns the number of new edges added."
        )
        .def("same_component", &StreamGraph::same_component, py::arg("u"), py::arg("v"))
        .def("component_id", &StreamGraph::find_component, py::arg("u"),
             "Smallest node id in the component containing u.")
        .def("n_components", &StreamGraph::n_components)
        .def("component_size", &StreamGraph::component_size, py::arg("u"))
        .def(
            "component_nodes",
            [](StreamGraph& G, uint32_t u) { return to_uint32_array(G.component_nodes(u)); },
            py::arg("u"),
            "uint32 array of all touched nodes in u's component."
        )
        .def(
            "component_ids",
            [](StreamGraph& G) { return to_uint32_array(G.component_ids()); },
            "uint32 array of the canonical minimum component id per touched node."
        )
        .def(
            "strong_component_ids",
            [](const StreamGraph& G) { return to_uint32_array(G.strong_component_ids()); },
            "uint32 array per touched node: smallest node id in its strongly connected "
            "component (Tarjan, on demand, O(n + m))."
        )
        .def("n_strong_components", &StreamGraph::n_strong_components)
        .def("degree", &StreamGraph::degree, py::arg("u"), "Degree of u (out-degree in directed graphs).")
        .def("in_degree", &StreamGraph::in_degree, py::arg("u"), "In-degree of u; equals degree(u) in undirected graphs.")
        .def("strength", &StreamGraph::strength, py::arg("u"),
             "Sum of (out-)edge weights at u; equals degree(u) when unweighted.")
        .def(
            "degree_histogram",
            &degree_histogram_np,
            "uint64 array: histogram[d] = number of touched nodes with (out-)degree d."
        )
        .def("has_edge", &StreamGraph::has_edge, py::arg("u"), py::arg("v"))
        .def("edge_weight", &StreamGraph::edge_weight, py::arg("u"), py::arg("v"),
             "Weight of edge (u, v); 1.0 for unweighted graphs. Raises if the edge is absent.")
        .def("total_weight", &StreamGraph::total_weight, "Sum of weights over all edges.")
        .def("neighbours", &neighbours_np, py::arg("u"), "Sorted uint32 array of u's (out-)neighbours.")
        .def(
            "in_neighbours",
            &in_neighbours_np,
            py::arg("u"),
            "Sorted uint32 array of u's in-neighbours; equals neighbours(u) in undirected graphs."
        )
        .def(
            "edge_list",
            &edge_list_np,
            py::arg("weights") = false,
            "Tuple (us, vs) or (us, vs, ws) of arrays listing each edge once "
            "(undirected: u < v; directed: every out-edge)."
        )
        .def("n_nodes", &StreamGraph::n_nodes)
        .def("n_edges", [](const StreamGraph& G) { return G.n_edges; })
        .def("n_ids", [](const StreamGraph& G) { return G.n_nodes_actual; },
             "Allocated id range: max node id + 1 (length of per-node-id result arrays).")
        .def_property_readonly("directed", [](const StreamGraph& G) { return G.directed; })
        .def_property_readonly("weighted", [](const StreamGraph& G) { return G.weighted; })
        .def("triangle_count", &StreamGraph::total_triangles, "Global triangle count, O(1).")
        .def("local_triangles", &StreamGraph::local_triangles, py::arg("u"), "Triangles incident to node u, O(1).")
        .def("all_local_triangles", &all_local_triangles_np, "uint32 array of per-node-id local triangle counts.")
        .def("clustering_coefficient", &StreamGraph::clustering_coefficient, py::arg("u"),
             "Local clustering coefficient 2T/(d(d-1)) on the underlying undirected graph.")
        .def("avg_clustering", &StreamGraph::avg_clustering, "Mean local clustering over touched nodes.")
        .def("density", &StreamGraph::density)
        .def("avg_degree", &StreamGraph::avg_degree)
        .def("max_degree", &StreamGraph::max_degree, "Maximum (out-)degree, O(1).")
        .def(
            "betweenness_approx",
            &betweenness_approx_np,
            py::arg("k") = 200,
            py::arg("n_threads") = 0,
            py::arg("seed") = 42,
            py::arg("normalise") = true,
            "Approximate betweenness via random (s,t) pair sampling; float64 per node id. "
            "normalise=True divides by the maximum (scores in [0, 1]); normalise=False "
            "returns the raw betweenness estimate. Directed graphs respect edge direction "
            "and sum over ordered pairs; weighted graphs use Dijkstra shortest paths."
        )
        .def(
            "pagerank",
            &pagerank_np,
            py::arg("damping") = 0.85,
            py::arg("tol") = 1e-10,
            py::arg("max_iter") = 100,
            "PageRank via power iteration; float64 per node id summing to 1. Weighted "
            "graphs distribute rank proportionally to edge weights."
        )
        .def(
            "save",
            [](const StreamGraph& G, const std::string& path) { G.save(path.c_str()); },
            py::arg("path"),
            "Save the graph atomically to a portable binary .esg file (EDGS format v3)."
        )
        .def_static(
            "load",
            [](const std::string& path) { return StreamGraph::load(path.c_str()); },
            py::arg("path"),
            "Load an EDGS v2/v3 graph from a binary .esg file."
        )
        .def(
            "copy",
            [](const StreamGraph& G) {
                auto out = std::make_unique<StreamGraph>(
                    G.n_nodes_actual, G.directed, G.weighted);
                for (uint32_t u : G.nodes()) out->add_node(u);
                std::vector<uint32_t> us, vs;
                std::vector<double> ws;
                G.edge_list(us, vs, G.weighted ? &ws : nullptr);
                for (size_t i = 0; i < us.size(); ++i)
                    out->add_edge(us[i], vs[i], G.weighted ? ws[i] : 1.0);
                return out;
            },
            "Return an independent in-memory copy suitable as a read-only snapshot."
        );
}
