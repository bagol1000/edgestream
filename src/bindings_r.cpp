//Rcpp bindings exposing StreamGraph to R via an external pointer (XPtr). Node IDs are 0-indexed.
#include <Rcpp.h>

#include <memory>

#include "streamgraph.h"

using streamgraph::StreamGraph;
typedef Rcpp::XPtr<StreamGraph> StreamGraphPtr;

namespace {

inline uint32_t as_node(int x) {
    if (x < 0) Rcpp::stop("node IDs must be non-negative (0-indexed)");
    return static_cast<uint32_t>(x);
}

inline StreamGraph* deref(SEXP ptr) {
    StreamGraphPtr p(ptr);
    if (!p) Rcpp::stop("invalid (null) StreamGraph pointer");
    return p.get();
}

}

// [[Rcpp::export(name = ".sg_create")]]
SEXP sg_create(int n_nodes, bool directed) {
    if (n_nodes < 0) Rcpp::stop("n_nodes must be non-negative");
    StreamGraph* g = new StreamGraph(static_cast<uint32_t>(n_nodes), directed);
    StreamGraphPtr ptr(g, true);   //XPtr finaliser deletes on GC
    return ptr;
}

// [[Rcpp::export(name = ".sg_add_edge")]]
bool sg_add_edge(SEXP ptr, int u, int v) { return deref(ptr)->add_edge(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_same_component")]]
bool sg_same_component(SEXP ptr, int u, int v) { return deref(ptr)->same_component(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_component_id")]]
int sg_component_id(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->find_component(as_node(u))); }

// [[Rcpp::export(name = ".sg_n_components")]]
int sg_n_components(SEXP ptr) { return static_cast<int>(deref(ptr)->n_components()); }

// [[Rcpp::export(name = ".sg_component_size")]]
int sg_component_size(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->component_size(as_node(u))); }

// [[Rcpp::export(name = ".sg_degree")]]
int sg_degree(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->degree(as_node(u))); }

// [[Rcpp::export(name = ".sg_has_edge")]]
bool sg_has_edge(SEXP ptr, int u, int v) { return deref(ptr)->has_edge(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_neighbours")]]
Rcpp::IntegerVector sg_neighbours(SEXP ptr, int u) {
    std::vector<uint32_t> nb = deref(ptr)->neighbours(as_node(u));
    Rcpp::IntegerVector out(nb.size());
    for (size_t i = 0; i < nb.size(); ++i) out[i] = static_cast<int>(nb[i]);
    return out;
}

// [[Rcpp::export(name = ".sg_n_nodes")]]
int sg_n_nodes(SEXP ptr) { return static_cast<int>(deref(ptr)->n_nodes()); }

// [[Rcpp::export(name = ".sg_n_edges")]]
double sg_n_edges(SEXP ptr) { return static_cast<double>(deref(ptr)->n_edges); }

// [[Rcpp::export(name = ".sg_triangle_count")]]
double sg_triangle_count(SEXP ptr) {
    //global count can exceed the R integer range, so return numeric
    return static_cast<double>(deref(ptr)->total_triangles());
}

// [[Rcpp::export(name = ".sg_local_triangles")]]
int sg_local_triangles(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->local_triangles(as_node(u))); }

// [[Rcpp::export(name = ".sg_all_local_triangles")]]
Rcpp::IntegerVector sg_all_local_triangles(SEXP ptr) {
    std::vector<uint32_t> t = deref(ptr)->all_local_triangles();
    Rcpp::IntegerVector out(t.size());
    for (size_t i = 0; i < t.size(); ++i) out[i] = static_cast<int>(t[i]);
    return out;
}

// [[Rcpp::export(name = ".sg_add_edges")]]
double sg_add_edges(SEXP ptr, Rcpp::IntegerVector us, Rcpp::IntegerVector vs, int n_threads) {
    if (us.size() != vs.size()) Rcpp::stop("us and vs must have the same length");
    size_t m = static_cast<size_t>(us.size());
    std::vector<uint32_t> cu(m), cv(m);
    for (size_t i = 0; i < m; ++i) {
        if (us[i] < 0 || vs[i] < 0) Rcpp::stop("node IDs must be non-negative (0-indexed)");
        cu[i] = static_cast<uint32_t>(us[i]);
        cv[i] = static_cast<uint32_t>(vs[i]);
    }
    uint64_t added = deref(ptr)->add_edges(cu.data(), cv.data(), m, n_threads);
    return static_cast<double>(added);
}

// [[Rcpp::export(name = ".sg_density")]]
double sg_density(SEXP ptr) { return deref(ptr)->density(); }

// [[Rcpp::export(name = ".sg_avg_degree")]]
double sg_avg_degree(SEXP ptr) { return deref(ptr)->avg_degree(); }

// [[Rcpp::export(name = ".sg_max_degree")]]
int sg_max_degree(SEXP ptr) { return static_cast<int>(deref(ptr)->max_degree()); }

// [[Rcpp::export(name = ".sg_degree_histogram")]]
Rcpp::NumericVector sg_degree_histogram(SEXP ptr) {
    const std::vector<uint64_t>& h = deref(ptr)->degree_histogram;
    Rcpp::NumericVector out(h.size());
    Rcpp::CharacterVector names(h.size());
    for (size_t d = 0; d < h.size(); ++d) {
        out[d] = static_cast<double>(h[d]);
        names[d] = std::to_string(d);
    }
    out.names() = names;
    return out;
}

// [[Rcpp::export(name = ".sg_component_nodes")]]
Rcpp::IntegerVector sg_component_nodes(SEXP ptr, int u) {
    std::vector<uint32_t> c = deref(ptr)->component_nodes(as_node(u));
    Rcpp::IntegerVector out(c.size());
    for (size_t i = 0; i < c.size(); ++i) out[i] = static_cast<int>(c[i]);
    return out;
}

// [[Rcpp::export(name = ".sg_component_ids")]]
Rcpp::IntegerVector sg_component_ids(SEXP ptr) {
    std::vector<uint32_t> c = deref(ptr)->component_ids();
    Rcpp::IntegerVector out(c.size());
    for (size_t i = 0; i < c.size(); ++i) out[i] = static_cast<int>(c[i]);
    return out;
}

// [[Rcpp::export(name = ".sg_betweenness_approx")]]
Rcpp::NumericVector sg_betweenness_approx(SEXP ptr, int k, int n_threads, double seed) {
    std::vector<double> bc = streamgraph::BetweennessApprox::compute(*deref(ptr), k, n_threads, static_cast<uint64_t>(seed));
    return Rcpp::NumericVector(bc.begin(), bc.end());
}

// [[Rcpp::export(name = ".sg_save")]]
void sg_save(SEXP ptr, std::string path) { deref(ptr)->save(path.c_str()); }

// [[Rcpp::export(name = ".sg_load")]]
SEXP sg_load(std::string path) {
    StreamGraph* g = StreamGraph::load(path.c_str()).release();
    StreamGraphPtr ptr(g, true);
    return ptr;
}
