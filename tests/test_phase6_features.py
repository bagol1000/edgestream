"""Phase 6 tests: edge weights, edge removal, clustering coefficients, SCC,
PageRank, edge-list export and ecosystem conversions."""

import math

import numpy as np
import pytest

import edgestream as es


# ---------- weights ----------

def test_weighted_basics():
    G = es.StreamGraph(weighted=True)
    assert G.weighted is True
    G.add_edge(0, 1, 2.5)
    G.add_edge(1, 2, 1.5)
    assert G.edge_weight(0, 1) == 2.5
    assert G.edge_weight(1, 0) == 2.5      # undirected symmetry
    assert G.strength(1) == 4.0
    assert G.total_weight() == 4.0
    # duplicate keeps the original weight
    assert G.add_edge(0, 1, 99.0) is False
    assert G.edge_weight(0, 1) == 2.5


def test_unweighted_weight_queries():
    G = es.StreamGraph()
    G.add_edge(0, 1)
    assert G.edge_weight(0, 1) == 1.0
    assert G.strength(0) == 1.0
    assert G.total_weight() == 1.0
    with pytest.raises(IndexError):
        G.edge_weight(0, 5)


def test_weight_must_be_positive():
    G = es.StreamGraph(weighted=True)
    with pytest.raises(ValueError):
        G.add_edge(0, 1, 0.0)
    with pytest.raises(ValueError):
        G.add_edge(0, 1, -1.0)


def test_weighted_batch_first_wins():
    G = es.StreamGraph(weighted=True)
    us = np.array([0, 0, 1], dtype=np.uint32)
    vs = np.array([1, 1, 2], dtype=np.uint32)
    ws = np.array([5.0, 9.0, 2.0])
    assert G.add_edges(us, vs, ws) == 2
    assert G.edge_weight(0, 1) == 5.0
    assert G.total_weight() == 7.0


def test_weighted_serialisation_roundtrip(tmp_path):
    G = es.StreamGraph(directed=True, weighted=True)
    G.add_edge(0, 1, 2.25)
    G.add_edge(1, 2, 0.5)
    p = str(tmp_path / "w.esg")
    G.save(p)
    H = es.StreamGraph.load(p)
    assert H.weighted and H.directed
    assert H.edge_weight(0, 1) == 2.25
    assert H.total_weight() == G.total_weight()


# ---------- removal ----------

def test_remove_edge_updates_everything():
    G = es.StreamGraph()
    for u, v in [(0, 1), (1, 2), (0, 2), (2, 3)]:
        G.add_edge(u, v)
    assert G.triangle_count() == 1
    assert G.remove_edge(0, 2) is True
    assert G.remove_edge(0, 2) is False    # already gone
    assert G.triangle_count() == 0
    assert G.n_edges() == 3
    assert G.has_edge(0, 2) is False
    assert G.degree(0) == 1
    assert list(G.neighbours(2)) == [1, 3]


def test_remove_edge_splits_component_lazily():
    G = es.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(1, 2)
    assert G.n_components() == 1
    G.remove_edge(1, 2)
    assert G.n_components() == 2           # {0,1} + isolated {2}
    assert G.same_component(0, 1)
    assert not G.same_component(0, 2)
    assert G.component_size(2) == 1
    assert G.n_nodes() == 3                # node 2 stays touched


def test_remove_then_add_again():
    G = es.StreamGraph()
    G.add_edge(0, 1)
    G.remove_edge(0, 1)
    assert G.max_degree() == 0
    assert G.add_edge(0, 1) is True
    assert G.n_edges() == 1
    assert G.n_components() == 1
    assert G.max_degree() == 1


def test_remove_directed_reciprocal_keeps_triangles():
    G = es.StreamGraph(directed=True)
    for u, v in [(0, 1), (1, 2), (0, 2), (2, 0)]:
        G.add_edge(u, v)
    assert G.triangle_count() == 1
    # removing one of the two 0<->2 arcs keeps the undirected edge alive
    G.remove_edge(2, 0)
    assert G.triangle_count() == 1
    G.remove_edge(0, 2)
    assert G.triangle_count() == 0


def test_random_add_remove_consistency():
    import random
    random.seed(11)
    G = es.StreamGraph()
    present = set()
    for _ in range(3000):
        u, v = random.randint(0, 24), random.randint(0, 24)
        if u == v:
            continue
        key = (min(u, v), max(u, v))
        if key in present and random.random() < 0.5:
            assert G.remove_edge(u, v) is True
            present.discard(key)
        else:
            assert G.add_edge(u, v) == (key not in present)
            present.add(key)
    assert G.n_edges() == len(present)
    # rebuild a fresh graph from the surviving edges: metrics must agree
    H = es.StreamGraph()
    for u, v in present:
        H.add_edge(u, v)
    assert G.triangle_count() == H.triangle_count()
    assert G.n_components() == H.n_components()
    assert G.max_degree() == H.max_degree()

    def hist(g):  # bucket 0 differs (removal survivors) and the vector keeps trailing zeros
        h = list(g.degree_histogram())[1:]
        while h and h[-1] == 0:
            h.pop()
        return h

    assert hist(G) == hist(H)


# ---------- clustering ----------

def test_clustering_coefficient():
    G = es.StreamGraph()
    for u, v in [(0, 1), (1, 2), (0, 2), (0, 3)]:
        G.add_edge(u, v)
    assert G.clustering_coefficient(1) == 1.0             # deg 2, 1 triangle
    assert G.clustering_coefficient(0) == pytest.approx(1 / 3)  # deg 3, 1 of 3 pairs
    assert G.clustering_coefficient(3) == 0.0             # deg 1
    expected = (1 / 3 + 1.0 + 1.0 + 0.0) / 4
    assert G.avg_clustering() == pytest.approx(expected)


# ---------- SCC ----------

def test_scc_directed_cycle_and_tail():
    G = es.StreamGraph(directed=True)
    for u, v in [(0, 1), (1, 2), (2, 0), (2, 3)]:
        G.add_edge(u, v)
    assert list(G.strong_component_ids()) == [0, 0, 0, 3]
    assert G.n_strong_components() == 2


def test_scc_undirected_equals_components():
    G = es.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(2, 3)
    assert G.n_strong_components() == G.n_components() == 2


# ---------- PageRank ----------

def test_pagerank_star_and_sum():
    G = es.StreamGraph()
    for i in range(1, 5):
        G.add_edge(0, i)
    pr = G.pagerank()
    assert sum(pr) == pytest.approx(1.0)
    assert pr[0] > pr[1]
    assert pr[1] == pytest.approx(pr[2]) == pytest.approx(pr[3])


def test_pagerank_weighted_prefers_heavy_edge():
    G = es.StreamGraph(directed=True, weighted=True)
    G.add_edge(0, 1, 9.0)
    G.add_edge(0, 2, 1.0)
    pr = G.pagerank()
    assert pr[1] > pr[2]


# ---------- exports / conversions ----------

def test_edge_list_roundtrip():
    G = es.StreamGraph(weighted=True)
    G.add_edge(3, 1, 2.0)
    G.add_edge(1, 2, 4.0)
    us, vs, ws = G.edge_list(weights=True)
    triples = sorted(zip(us.tolist(), vs.tolist(), ws.tolist()))
    assert triples == [(1, 2, 4.0), (1, 3, 2.0)]
    us2, vs2 = G.edge_list()
    assert len(us2) == 2


def test_networkx_roundtrip():
    nx = pytest.importorskip("networkx")
    G = es.StreamGraph(directed=True, weighted=True)
    G.add_edge(0, 1, 2.0)
    G.add_edge(1, 2, 3.0)
    nxg = es.to_networkx(G)
    assert nxg.is_directed()
    assert nxg[0][1]["weight"] == 2.0
    H, idx = es.from_networkx(nxg)
    assert H.n_edges() == 2
    assert H.edge_weight(idx.id(0), idx.id(1)) == 2.0

    K, kidx = es.from_networkx(nx.karate_club_graph())
    assert K.n_edges() == 78
    assert K.triangle_count() == 45


def test_scipy_sparse():
    pytest.importorskip("scipy")
    G = es.StreamGraph()
    G.add_edge(0, 1)
    S = es.to_scipy_sparse(G)
    assert S.shape == (2, 2)
    M = S.toarray()
    assert M[0, 1] == 1.0 and M[1, 0] == 1.0


def test_node_index_relabelling():
    nx = pytest.importorskip("networkx")
    G = es.StreamGraph()
    idx = es.NodeIndex()
    G.add_edge(idx["a"], idx["b"])
    nxg = es.to_networkx(G, index=idx)
    assert set(nxg.nodes()) == {"a", "b"}
