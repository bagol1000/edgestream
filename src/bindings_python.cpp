// pybind11 bindings exposing StreamGraph to Python; conversions happen here at the boundary.
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "streamgraph.h"

namespace py = pybind11;
using streamgraph::StreamGraph;

static py::array_t<uint32_t> to_uint32_array(const std::vector<uint32_t>& v) {
    py::array_t<uint32_t> out(static_cast<py::ssize_t>(v.size()));
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

static uint64_t add_edges_np(
    StreamGraph& G,
    py::array_t<uint32_t, py::array::c_style | py::array::forcecast> us,
    py::array_t<uint32_t, py::array::c_style | py::array::forcecast> vs,
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
    //long-running pure C++ section: let other Python threads run
    py::gil_scoped_release release;
    return G.add_edges(pu, pv, m, n_threads);
}

static py::array_t<double> betweenness_approx_np(
    const StreamGraph& G,
    int k,
    int n_threads,
    uint64_t seed,
    bool normalise
) {
    std::vector<double> bc;
    {
        py::gil_scoped_release release;
        bc = streamgraph::BetweennessApprox::compute(G, k, n_threads, seed, normalise);
    }
    py::array_t<double> out(static_cast<py::ssize_t>(bc.size()));
    if (!bc.empty()) std::copy(bc.begin(), bc.end(), out.mutable_data());
    return out;
}

static py::array_t<uint64_t> degree_histogram_np(const StreamGraph& G) {
    const std::vector<uint64_t>& h = G.degree_histogram;
    py::array_t<uint64_t> out(static_cast<py::ssize_t>(h.size()));
    if (!h.empty()) std::copy(h.begin(), h.end(), out.mutable_data());
    return out;
}

PYBIND11_MODULE(_streamgraph, m) {
    m.doc() = "streamgraph C++ core: streaming components, triangles, statistics and betweenness.";

    py::class_<StreamGraph>(m, "StreamGraph")
        .def(
            py::init([](uint32_t n_nodes, bool directed) {
                return std::make_unique<StreamGraph>(n_nodes, directed);
            }),
            py::arg("n_nodes") = 0,
            py::arg("directed") = false,
            "Create a StreamGraph. n_nodes=0 auto-grows; n_nodes=N pre-allocates N slots."
        )
        .def(
            "add_edge",
            &StreamGraph::add_edge,
            py::arg("u"),
            py::arg("v"),
            "Add edge (u, v). True if new, False if a duplicate or self-loop."
        )
        .def(
            "add_edges",
            &add_edges_np,
            py::arg("us"),
            py::arg("vs"),
            py::arg("n_threads") = 0,
            "Batch-add edges from 1-D arrays cast to contiguous uint32 buffers. "
            "Returns the number of new edges added."
        )
        .def("same_component", &StreamGraph::same_component, py::arg("u"), py::arg("v"))
        .def("component_id", &StreamGraph::find_component, py::arg("u"), "Root id of the component containing u.")
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
            "uint32 array of the component root id per touched node."
        )
        .def("degree", &StreamGraph::degree, py::arg("u"), "Degree of u (out-degree in directed graphs).")
        .def("in_degree", &StreamGraph::in_degree, py::arg("u"), "In-degree of u; equals degree(u) in undirected graphs.")
        .def(
            "degree_histogram",
            &degree_histogram_np,
            "uint64 array: histogram[d] = number of nodes with degree d (out-degree in directed graphs)."
        )
        .def("has_edge", &StreamGraph::has_edge, py::arg("u"), py::arg("v"))
        .def("neighbours", &neighbours_np, py::arg("u"), "Sorted uint32 array of u's (out-)neighbours.")
        .def(
            "in_neighbours",
            &in_neighbours_np,
            py::arg("u"),
            "Sorted uint32 array of u's in-neighbours; equals neighbours(u) in undirected graphs."
        )
        .def("n_nodes", &StreamGraph::n_nodes)
        .def("n_edges", [](const StreamGraph& G) { return G.n_edges; })
        .def("triangle_count", &StreamGraph::total_triangles, "Global triangle count, O(1).")
        .def("local_triangles", &StreamGraph::local_triangles, py::arg("u"), "Triangles incident to node u, O(1).")
        .def("all_local_triangles", &all_local_triangles_np, "uint32 array of per-node-id local triangle counts.")
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
            "and sum over ordered pairs."
        )
        .def(
            "save",
            [](const StreamGraph& G, const std::string& path) { G.save(path.c_str()); },
            py::arg("path"),
            py::call_guard<py::gil_scoped_release>(),
            "Save the graph to a binary .sgph file."
        )
        .def_static(
            "load",
            [](const std::string& path) { return StreamGraph::load(path.c_str()); },
            py::arg("path"),
            py::call_guard<py::gil_scoped_release>(),
            "Load a graph from a binary .sgph file."
        );
}
