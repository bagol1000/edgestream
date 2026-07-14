//Rcpp bindings exposing StreamGraph to R via an external pointer (XPtr). Node IDs are 0-indexed.
#include <Rcpp.h>

#include <memory>
#include <cmath>
#include <vector>

#include "edgestream.h"

using edgestream::StreamGraph;
typedef Rcpp::XPtr<StreamGraph> StreamGraphPtr;

namespace {

inline uint32_t as_node(int x) {
    if (x < 0) Rcpp::stop("node IDs must be non-negative (0-indexed)");
    return static_cast<uint32_t>(x);
}

inline StreamGraph* deref(SEXP ptr) {
    if (TYPEOF(ptr) != EXTPTRSXP ||
        R_ExternalPtrTag(ptr) != Rf_install("edgestream_StreamGraph"))
        Rcpp::stop("invalid StreamGraph object");
    StreamGraphPtr p(ptr);
    if (!p) Rcpp::stop("invalid (null) StreamGraph pointer");
    return p.get();
}

inline Rcpp::IntegerVector to_int_vector(const std::vector<uint32_t>& v) {
    Rcpp::IntegerVector out(v.size());
    for (size_t i = 0; i < v.size(); ++i) out[i] = static_cast<int>(v[i]);
    return out;
}

}

// [[Rcpp::export(name = ".sg_create")]]
SEXP sg_create(int n_nodes, bool directed, bool weighted) {
    if (n_nodes < 0) Rcpp::stop("n_nodes must be non-negative");
    StreamGraph* g = new StreamGraph(static_cast<uint32_t>(n_nodes), directed, weighted);
    StreamGraphPtr ptr(g, true);   //XPtr finaliser deletes on GC
    R_SetExternalPtrTag(ptr, Rf_install("edgestream_StreamGraph"));
    return ptr;
}

// [[Rcpp::export(name = ".sg_add_node")]]
bool sg_add_node(SEXP ptr, int u) { return deref(ptr)->add_node(as_node(u)); }

// [[Rcpp::export(name = ".sg_add_nodes")]]
double sg_add_nodes(SEXP ptr, int start, int count) {
    if (count < 0) Rcpp::stop("count must be non-negative");
    return static_cast<double>(deref(ptr)->add_nodes(as_node(start), static_cast<uint32_t>(count)));
}

// [[Rcpp::export(name = ".sg_nodes")]]
Rcpp::IntegerVector sg_nodes(SEXP ptr) { return to_int_vector(deref(ptr)->nodes()); }

// [[Rcpp::export(name = ".sg_reserve_nodes")]]
void sg_reserve_nodes(SEXP ptr, int n) {
    if (n < 0) Rcpp::stop("n must be non-negative");
    deref(ptr)->reserve_nodes(static_cast<uint32_t>(n));
}

// [[Rcpp::export(name = ".sg_reserve_edges")]]
void sg_reserve_edges(SEXP ptr, double m) {
    if (!std::isfinite(m) || m < 0 || std::floor(m) != m)
        Rcpp::stop("m must be a finite non-negative integer");
    deref(ptr)->reserve_edges(static_cast<uint64_t>(m));
}

// [[Rcpp::export(name = ".sg_clear")]]
void sg_clear(SEXP ptr) { deref(ptr)->clear(); }

// [[Rcpp::export(name = ".sg_add_edge")]]
bool sg_add_edge(SEXP ptr, int u, int v, double w) { return deref(ptr)->add_edge(as_node(u), as_node(v), w); }

// [[Rcpp::export(name = ".sg_remove_edge")]]
bool sg_remove_edge(SEXP ptr, int u, int v) { return deref(ptr)->remove_edge(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_update_edge_weight")]]
bool sg_update_edge_weight(SEXP ptr, int u, int v, double w) {
    return deref(ptr)->update_edge_weight(as_node(u), as_node(v), w);
}

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

// [[Rcpp::export(name = ".sg_in_degree")]]
int sg_in_degree(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->in_degree(as_node(u))); }

// [[Rcpp::export(name = ".sg_strength")]]
double sg_strength(SEXP ptr, int u) { return deref(ptr)->strength(as_node(u)); }

// [[Rcpp::export(name = ".sg_has_edge")]]
bool sg_has_edge(SEXP ptr, int u, int v) { return deref(ptr)->has_edge(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_edge_weight")]]
double sg_edge_weight(SEXP ptr, int u, int v) { return deref(ptr)->edge_weight(as_node(u), as_node(v)); }

// [[Rcpp::export(name = ".sg_total_weight")]]
double sg_total_weight(SEXP ptr) { return deref(ptr)->total_weight(); }

// [[Rcpp::export(name = ".sg_neighbours")]]
Rcpp::IntegerVector sg_neighbours(SEXP ptr, int u) { return to_int_vector(deref(ptr)->neighbours(as_node(u))); }

// [[Rcpp::export(name = ".sg_in_neighbours")]]
Rcpp::IntegerVector sg_in_neighbours(SEXP ptr, int u) { return to_int_vector(deref(ptr)->in_neighbours(as_node(u))); }

// [[Rcpp::export(name = ".sg_n_nodes")]]
int sg_n_nodes(SEXP ptr) { return static_cast<int>(deref(ptr)->n_nodes()); }

// [[Rcpp::export(name = ".sg_n_ids")]]
double sg_n_ids(SEXP ptr) { return static_cast<double>(deref(ptr)->n_nodes_actual); }

// [[Rcpp::export(name = ".sg_n_edges")]]
double sg_n_edges(SEXP ptr) { return static_cast<double>(deref(ptr)->n_edges); }

// [[Rcpp::export(name = ".sg_is_directed")]]
bool sg_is_directed(SEXP ptr) { return deref(ptr)->directed; }

// [[Rcpp::export(name = ".sg_is_weighted")]]
bool sg_is_weighted(SEXP ptr) { return deref(ptr)->weighted; }

// [[Rcpp::export(name = ".sg_triangle_count")]]
double sg_triangle_count(SEXP ptr) {
    //global count can exceed the R integer range, so return numeric
    return static_cast<double>(deref(ptr)->total_triangles());
}

// [[Rcpp::export(name = ".sg_local_triangles")]]
int sg_local_triangles(SEXP ptr, int u) { return static_cast<int>(deref(ptr)->local_triangles(as_node(u))); }

// [[Rcpp::export(name = ".sg_all_local_triangles")]]
Rcpp::IntegerVector sg_all_local_triangles(SEXP ptr) { return to_int_vector(deref(ptr)->all_local_triangles()); }

// [[Rcpp::export(name = ".sg_clustering_coefficient")]]
double sg_clustering_coefficient(SEXP ptr, int u) { return deref(ptr)->clustering_coefficient(as_node(u)); }

// [[Rcpp::export(name = ".sg_avg_clustering")]]
double sg_avg_clustering(SEXP ptr) { return deref(ptr)->avg_clustering(); }

// [[Rcpp::export(name = ".sg_add_edges")]]
double sg_add_edges(SEXP ptr, Rcpp::IntegerVector us, Rcpp::IntegerVector vs,
                    Rcpp::Nullable<Rcpp::NumericVector> ws, int n_threads) {
    if (us.size() != vs.size()) Rcpp::stop("us and vs must have the same length");
    size_t m = static_cast<size_t>(us.size());
    std::vector<uint32_t> cu(m), cv(m);
    for (size_t i = 0; i < m; ++i) {
        if (us[i] < 0 || vs[i] < 0) Rcpp::stop("node IDs must be non-negative (0-indexed)");
        cu[i] = static_cast<uint32_t>(us[i]);
        cv[i] = static_cast<uint32_t>(vs[i]);
    }
    const double* pw = nullptr;
    Rcpp::NumericVector wv;
    if (ws.isNotNull()) {
        wv = ws.get();
        if (static_cast<size_t>(wv.size()) != m) Rcpp::stop("ws must have the same length as us/vs");
        pw = wv.begin();
    }
    uint64_t added = deref(ptr)->add_edges(cu.data(), cv.data(), pw, m, n_threads);
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
Rcpp::IntegerVector sg_component_nodes(SEXP ptr, int u) { return to_int_vector(deref(ptr)->component_nodes(as_node(u))); }

// [[Rcpp::export(name = ".sg_component_ids")]]
Rcpp::IntegerVector sg_component_ids(SEXP ptr) { return to_int_vector(deref(ptr)->component_ids()); }

// [[Rcpp::export(name = ".sg_strong_component_ids")]]
Rcpp::IntegerVector sg_strong_component_ids(SEXP ptr) { return to_int_vector(deref(ptr)->strong_component_ids()); }

// [[Rcpp::export(name = ".sg_n_strong_components")]]
int sg_n_strong_components(SEXP ptr) { return static_cast<int>(deref(ptr)->n_strong_components()); }

// [[Rcpp::export(name = ".sg_edge_list")]]
Rcpp::List sg_edge_list(SEXP ptr) {
    std::vector<uint32_t> us, vs;
    std::vector<double> ws;
    deref(ptr)->edge_list(us, vs, &ws);
    return Rcpp::List::create(
        Rcpp::Named("u") = to_int_vector(us),
        Rcpp::Named("v") = to_int_vector(vs),
        Rcpp::Named("w") = Rcpp::NumericVector(ws.begin(), ws.end())
    );
}

// [[Rcpp::export(name = ".sg_pagerank")]]
Rcpp::NumericVector sg_pagerank(SEXP ptr, double damping, double tol, int max_iter) {
    std::vector<double> pr = deref(ptr)->pagerank(damping, tol, max_iter);
    return Rcpp::NumericVector(pr.begin(), pr.end());
}

// [[Rcpp::export(name = ".sg_betweenness_approx")]]
Rcpp::NumericVector sg_betweenness_approx(SEXP ptr, int k, int n_threads, double seed, bool normalise) {
    std::vector<double> bc = edgestream::BetweennessApprox::compute(
        *deref(ptr), k, n_threads, static_cast<uint64_t>(seed), normalise);
    return Rcpp::NumericVector(bc.begin(), bc.end());
}

// [[Rcpp::export(name = ".sg_save")]]
void sg_save(SEXP ptr, std::string path) { deref(ptr)->save(path.c_str()); }

// [[Rcpp::export(name = ".sg_load")]]
SEXP sg_load(std::string path) {
    StreamGraph* g = StreamGraph::load(path.c_str()).release();
    StreamGraphPtr ptr(g, true);
    R_SetExternalPtrTag(ptr, Rf_install("edgestream_StreamGraph"));
    return ptr;
}

// [[Rcpp::export(name = ".sg_copy")]]
SEXP sg_copy(SEXP source) {
    StreamGraph* g = deref(source);
    StreamGraph* out = new StreamGraph(g->n_nodes_actual, g->directed, g->weighted);
    for (uint32_t u : g->nodes()) out->add_node(u);
    std::vector<uint32_t> us, vs;
    std::vector<double> ws;
    g->edge_list(us, vs, g->weighted ? &ws : nullptr);
    for (size_t i = 0; i < us.size(); ++i)
        out->add_edge(us[i], vs[i], g->weighted ? ws[i] : 1.0);
    StreamGraphPtr ptr(out, true);
    R_SetExternalPtrTag(ptr, Rf_install("edgestream_StreamGraph"));
    return ptr;
}
