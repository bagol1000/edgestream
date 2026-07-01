"""Phase 5 tests: directed-graph semantics — triangles, betweenness,
in-degree/in-neighbours, serialisation and the normalise flag."""

import numpy as np
import pytest

import streamgraph as sg


def test_directed_triangles_are_insertion_order_independent():
    orders = [
        [(0, 1), (0, 2), (1, 2)],
        [(0, 2), (1, 2), (0, 1)],
        [(1, 2), (0, 1), (0, 2)],
    ]
    counts = []
    for edges in orders:
        G = sg.StreamGraph(directed=True)
        for u, v in edges:
            G.add_edge(u, v)
        counts.append(G.triangle_count())
    assert counts == [1, 1, 1]


def test_directed_triangles_count_underlying_undirected_graph():
    # 3-cycle: one undirected triangle regardless of orientation
    G = sg.StreamGraph(directed=True)
    for u, v in [(0, 1), (1, 2), (2, 0)]:
        G.add_edge(u, v)
    assert G.triangle_count() == 1
    assert G.local_triangles(0) == 1


def test_directed_reciprocal_edges_do_not_double_count_triangles():
    G = sg.StreamGraph(directed=True)
    for u, v in [(0, 1), (1, 2), (2, 0), (1, 0), (2, 1), (0, 2)]:
        G.add_edge(u, v)
    assert G.n_edges() == 6
    assert G.triangle_count() == 1


def test_directed_serialisation_roundtrip_preserves_triangles(tmp_path):
    G = sg.StreamGraph(directed=True)
    for u, v in [(0, 2), (1, 2), (0, 1), (3, 0), (3, 1)]:
        G.add_edge(u, v)
    p = str(tmp_path / "d.sgph")
    G.save(p)
    H = sg.StreamGraph.load(p)
    assert H.n_edges() == G.n_edges()
    assert H.triangle_count() == G.triangle_count()
    assert list(H.all_local_triangles()) == list(G.all_local_triangles())


def test_load_rejects_trailing_data(tmp_path):
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    p = str(tmp_path / "g.sgph")
    G.save(p)
    with open(p, "ab") as f:
        f.write(b"junk")
    with pytest.raises(RuntimeError, match="trailing data"):
        sg.StreamGraph.load(p)


def test_directed_betweenness_respects_direction():
    # path 0 -> 1 -> 2: only node 1 is an intermediary, and only for (0, 2)
    G = sg.StreamGraph(directed=True)
    G.add_edge(0, 1)
    G.add_edge(1, 2)
    bc = G.betweenness_approx(k=1000, seed=42)
    assert bc[1] == 1.0
    assert bc[0] == 0.0 and bc[2] == 0.0

    # raw estimate: exactly one s-t pair passes through node 1
    raw = G.betweenness_approx(k=1000, seed=42, normalise=False)
    assert raw[1] == 1.0


def test_directed_betweenness_reverse_path_differs():
    # 0 -> 1 <- 2 has no path through 1 at all
    G = sg.StreamGraph(directed=True)
    G.add_edge(0, 1)
    G.add_edge(2, 1)
    bc = G.betweenness_approx(k=1000, seed=42)
    assert list(bc) == [0.0, 0.0, 0.0]


def test_undirected_betweenness_unnormalised_path():
    G = sg.StreamGraph()
    for i in range(4):
        G.add_edge(i, i + 1)  # path 0-1-2-3-4
    raw = G.betweenness_approx(k=10000, seed=42, normalise=False)
    assert list(raw) == [0.0, 3.0, 4.0, 3.0, 0.0]


def test_in_degree_and_in_neighbours_directed():
    G = sg.StreamGraph(directed=True)
    G.add_edge(0, 2)
    G.add_edge(1, 2)
    G.add_edge(2, 3)
    assert G.degree(2) == 1        # out-degree
    assert G.in_degree(2) == 2
    assert list(G.in_neighbours(2)) == [0, 1]
    assert list(G.neighbours(2)) == [3]
    assert G.in_degree(0) == 0
    assert G.in_degree(99) == 0    # never referenced


def test_in_degree_and_in_neighbours_undirected_alias():
    G = sg.StreamGraph()
    G.add_edge(0, 2)
    G.add_edge(1, 2)
    assert G.in_degree(2) == G.degree(2) == 2
    assert list(G.in_neighbours(2)) == list(G.neighbours(2)) == [0, 1]


def test_max_degree_incremental():
    G = sg.StreamGraph()
    assert G.max_degree() == 0
    for i in range(1, 6):
        G.add_edge(0, i)
        assert G.max_degree() == i


def test_directed_batch_matches_sequential_triangles():
    import random
    random.seed(3)
    pairs = [(random.randint(0, 49), random.randint(0, 49)) for _ in range(400)]

    G1 = sg.StreamGraph(directed=True)
    for u, v in pairs:
        G1.add_edge(u, v)

    G2 = sg.StreamGraph(directed=True)
    us = np.array([p[0] for p in pairs], dtype=np.uint32)
    vs = np.array([p[1] for p in pairs], dtype=np.uint32)
    G2.add_edges(us, vs)

    assert G1.n_edges() == G2.n_edges()
    assert G1.triangle_count() == G2.triangle_count()
    assert list(G1.all_local_triangles()) == list(G2.all_local_triangles())
