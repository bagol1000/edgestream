"""Phase 4 tests: approximate betweenness and NodeIndex."""

import edgestream as sg


def test_betweenness_path_graph():
    #linear path 0-1-2-3-4: middle nodes have higher betweenness
    G = sg.StreamGraph()
    for i in range(4):
        G.add_edge(i, i + 1)
    bc = G.betweenness_approx(k=1000, seed=42)
    assert bc[2] > bc[0]
    assert bc[2] > bc[4]


def test_betweenness_star():
    #star graph: the centre has the highest betweenness
    G = sg.StreamGraph()
    for i in range(1, 10):
        G.add_edge(0, i)
    bc = G.betweenness_approx(k=500, seed=42)
    assert bc[0] == max(bc)


def test_betweenness_normalised():
    G = sg.StreamGraph()
    for i in range(20):
        G.add_edge(i, i + 1)
    bc = G.betweenness_approx(k=500, seed=42)
    assert all(0.0 <= v <= 1.0 for v in bc)


def test_betweenness_reproducible():
    G = sg.StreamGraph()
    for i in range(50):
        G.add_edge(i % 10, (i * 7 + 3) % 10)
    bc1 = G.betweenness_approx(k=100, seed=42)
    bc2 = G.betweenness_approx(k=100, seed=42)
    assert list(bc1) == list(bc2)


def test_node_index_basic():
    idx = sg.NodeIndex()
    assert idx["A"] == 0
    assert idx["B"] == 1
    assert idx["A"] == 0  #same ID on second call
    assert idx.id("A") == 0
    assert idx.key(0) == "A"
    assert idx.n_nodes() == 2


def test_node_index_with_graph():
    G = sg.StreamGraph()
    idx = sg.NodeIndex()
    G.add_edge(idx["alice"], idx["bob"])
    G.add_edge(idx["bob"], idx["charlie"])
    assert G.n_nodes() == 3
    assert G.same_component(idx["alice"], idx["charlie"])
