"""Phase 2 tests: streaming triangle counting."""

import edgestream as sg


def test_no_triangle_open_path():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(1, 2)
    assert G.triangle_count() == 0


def test_single_triangle():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(1, 2)
    G.add_edge(0, 2)
    assert G.triangle_count() == 1
    assert G.local_triangles(0) == 1
    assert G.local_triangles(1) == 1
    assert G.local_triangles(2) == 1


def test_k4_four_triangles():
    G = sg.StreamGraph()
    for u, v in [(0, 1), (0, 2), (0, 3), (1, 2), (1, 3), (2, 3)]:
        G.add_edge(u, v)
    assert G.triangle_count() == 4
    assert G.local_triangles(0) == 3


def test_consistency_invariant():
    import random
    random.seed(42)
    G = sg.StreamGraph()
    for _ in range(2000):
        u, v = random.randint(0, 49), random.randint(0, 49)
        if u != v:
            G.add_edge(u, v)
    assert sum(G.all_local_triangles()) == 3 * G.triangle_count()


def test_no_duplicate_counting():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(0, 1)  #duplicate ignored
    G.add_edge(1, 2)
    G.add_edge(0, 2)
    assert G.triangle_count() == 1


def test_k10_120_triangles():
    G = sg.StreamGraph()
    for u in range(10):
        for v in range(u + 1, 10):
            G.add_edge(u, v)
    assert G.triangle_count() == 120  #C(10,3) = 120


def test_k10_local_count():
    G = sg.StreamGraph()
    for u in range(10):
        for v in range(u + 1, 10):
            G.add_edge(u, v)
    #each node in K10 belongs to C(9,2) = 36 triangles
    assert all(G.local_triangles(i) == 36 for i in range(10))
