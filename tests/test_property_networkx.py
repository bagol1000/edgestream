"""Property-based tests: every metric must agree with NetworkX on random
graphs. hypothesis generates edge lists (directed/undirected, optionally
weighted); mismatches shrink to a minimal counterexample."""

import math

import pytest

hypothesis = pytest.importorskip("hypothesis")
nx = pytest.importorskip("networkx")

from hypothesis import given, settings, strategies as st

import edgestream as es

# small graphs keep shrinking fast; 0..14 node ids, up to 60 edges
edge_st = st.tuples(st.integers(0, 14), st.integers(0, 14))
edges_st = st.lists(edge_st, min_size=1, max_size=60)
SETTINGS = settings(max_examples=40, deadline=None)


def build(edges, directed, weights=None):
    G = es.StreamGraph(directed=directed, weighted=weights is not None)
    N = nx.DiGraph() if directed else nx.Graph()
    for i, (u, v) in enumerate(edges):
        if u == v:
            continue
        if weights is None:
            G.add_edge(u, v)
            N.add_edge(u, v)
        else:
            w = weights[i % len(weights)]
            if not G.has_edge(u, v):
                G.add_edge(u, v, w)
                if not N.has_edge(u, v):
                    N.add_edge(u, v, weight=w)
    return G, N


@given(edges=edges_st, directed=st.booleans())
@SETTINGS
def test_basic_counts_match(edges, directed):
    G, N = build(edges, directed)
    assert G.n_edges() == N.number_of_edges()
    assert G.n_nodes() == N.number_of_nodes()
    for u in N.nodes():
        assert G.degree(u) == (N.out_degree(u) if directed else N.degree(u))
        if directed:
            assert G.in_degree(u) == N.in_degree(u)


@given(edges=edges_st, directed=st.booleans())
@SETTINGS
def test_triangles_match_underlying_undirected(edges, directed):
    G, N = build(edges, directed)
    U = N.to_undirected() if directed else N
    assert G.triangle_count() == sum(nx.triangles(U).values()) // 3
    for u in U.nodes():
        assert G.local_triangles(u) == nx.triangles(U, u)
        assert G.clustering_coefficient(u) == pytest.approx(nx.clustering(U, u))
    expected_avg = sum(nx.clustering(U).values()) / len(U) if len(U) else 0.0
    assert G.avg_clustering() == pytest.approx(expected_avg)


@given(edges=edges_st, directed=st.booleans())
@SETTINGS
def test_components_match(edges, directed):
    G, N = build(edges, directed)
    U = N.to_undirected() if directed else N
    assert G.n_components() == nx.number_connected_components(U)
    if directed:
        assert G.n_strong_components() == nx.number_strongly_connected_components(N)


@given(edges=edges_st, directed=st.booleans())
@SETTINGS
def test_exact_betweenness_matches(edges, directed):
    G, N = build(edges, directed)
    ours = G.betweenness_approx(k=10**9, normalise=False)  # k >= all pairs => exact
    theirs = nx.betweenness_centrality(N, normalized=False)
    for u, bc in theirs.items():
        assert ours[u] == pytest.approx(bc, abs=1e-9)


@given(edges=edges_st, directed=st.booleans(),
       # Integer-valued weights avoid using NetworkX's exact float equality as
       # the oracle for mathematically equal paths with different sum orders.
       weights=st.lists(st.integers(1, 10), min_size=1, max_size=10))
@SETTINGS
def test_exact_weighted_betweenness_matches(edges, directed, weights):
    G, N = build(edges, directed, weights=weights)
    if G.n_edges() == 0:
        return
    ours = G.betweenness_approx(k=10**9, normalise=False)
    theirs = nx.betweenness_centrality(N, normalized=False, weight="weight")
    for u, bc in theirs.items():
        assert ours[u] == pytest.approx(bc, abs=1e-6)


@given(edges=edges_st, directed=st.booleans())
@SETTINGS
def test_pagerank_matches(edges, directed):
    G, N = build(edges, directed)
    ours = G.pagerank(damping=0.85, tol=1e-12, max_iter=200)
    theirs = nx.pagerank(N, alpha=0.85, tol=1e-12, max_iter=200)
    for u, pr in theirs.items():
        assert ours[u] == pytest.approx(pr, abs=1e-8)


@given(edges=edges_st, directed=st.booleans(),
       removals=st.lists(st.integers(0, 100), max_size=20))
@SETTINGS
def test_removal_matches_rebuild(edges, directed, removals):
    """Removing edges must leave the graph identical to one built from the
    surviving edge set."""
    G, N = build(edges, directed)
    present = list(N.edges())
    for r in removals:
        if not present:
            break
        u, v = present.pop(r % len(present))
        assert G.remove_edge(u, v) is True
        N.remove_edge(u, v)

    H, _ = build(present, directed) if present else (es.StreamGraph(directed=directed), None)
    assert G.n_edges() == len(present)
    assert G.triangle_count() == H.triangle_count()
    assert G.max_degree() == H.max_degree()
    # nodes remain in both graphs after removals (isolated = singleton component)
    U = N.to_undirected() if directed else N
    assert G.n_components() == nx.number_connected_components(U)
