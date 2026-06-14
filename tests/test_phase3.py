"""Phase 3 tests: batch add_edges, statistics, component_nodes, serialisation."""

import numpy as np

import streamgraph as sg


def test_batch_dedup():
    G = sg.StreamGraph()
    us = np.array([0, 0, 1, 2], dtype=np.uint32)
    vs = np.array([1, 1, 2, 0], dtype=np.uint32)
    added = G.add_edges(us, vs)
    #(0,1), (0,1)=dup, (1,2), (2,0)=(0,2)=new -> 3 distinct new edges
    assert added == 3
    assert G.n_edges() == 3


def test_batch_result_matches_sequential():
    import random
    random.seed(0)
    pairs = [(random.randint(0, 99), random.randint(0, 99)) for _ in range(500)]

    G1 = sg.StreamGraph()
    for u, v in pairs:
        G1.add_edge(u, v)

    G2 = sg.StreamGraph()
    us = np.array([p[0] for p in pairs], dtype=np.uint32)
    vs = np.array([p[1] for p in pairs], dtype=np.uint32)
    G2.add_edges(us, vs)

    assert G1.n_edges() == G2.n_edges()
    assert G1.triangle_count() == G2.triangle_count()
    assert G1.n_components() == G2.n_components()


def test_stats():
    G = sg.StreamGraph()
    for i in range(4):
        G.add_edge(i, i + 1)
    assert G.n_edges() == 4
    assert G.n_nodes() == 5
    assert abs(G.avg_degree() - 8 / 5) < 1e-10
    assert G.max_degree() == 2


def test_component_nodes():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(1, 2)
    G.add_edge(5, 6)
    nodes = sorted(G.component_nodes(0))
    assert nodes == [0, 1, 2]
    assert sorted(G.component_nodes(5)) == [5, 6]


def test_serialisation(tmp_path):
    G = sg.StreamGraph()
    for i in range(50):
        G.add_edge(i % 10, (i * 7 + 3) % 10)
    G.save(str(tmp_path / "G.sgph"))
    G2 = sg.StreamGraph.load(str(tmp_path / "G.sgph"))
    assert G.n_edges() == G2.n_edges()
    assert G.triangle_count() == G2.triangle_count()
    assert G.n_components() == G2.n_components()


def test_serialisation_large(tmp_path):
    G = sg.StreamGraph(n_nodes=1000)
    import random
    random.seed(1)
    for _ in range(5000):
        G.add_edge(random.randint(0, 999), random.randint(0, 999))
    G.save(str(tmp_path / "large.sgph"))
    G2 = sg.StreamGraph.load(str(tmp_path / "large.sgph"))
    assert G.n_edges() == G2.n_edges()
    assert G.triangle_count() == G2.triangle_count()
    for i in range(1000):
        assert G.same_component(0, i) == G2.same_component(0, i)
