"""Phase 1 tests: Union-Find connected components, degree, neighbours.

Run with:  pytest tests/test_phase1.py -s
"""

import time

import numpy as np
import pytest

import streamgraph as sg


def test_union_find_basic():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(2, 3)
    #Model A: only touched nodes count, {0,1} and {2,3} -> 2 components
    assert G.n_components() == 2
    assert G.same_component(0, 1)
    assert not G.same_component(0, 2)
    G.add_edge(1, 2)
    assert G.same_component(0, 3)
    assert G.n_components() == 1


def test_no_duplicates():
    G = sg.StreamGraph()
    assert G.add_edge(0, 1) is True
    assert G.add_edge(0, 1) is False
    assert G.add_edge(1, 0) is False  #undirected
    assert G.n_edges() == 1


def test_no_self_loops():
    G = sg.StreamGraph()
    assert G.add_edge(5, 5) is False
    assert G.n_edges() == 0


def test_degree():
    G = sg.StreamGraph()
    G.add_edge(0, 1)
    G.add_edge(0, 2)
    G.add_edge(0, 3)
    assert G.degree(0) == 3
    assert G.degree(1) == 1


def test_neighbours_sorted():
    G = sg.StreamGraph()
    G.add_edge(5, 2)
    G.add_edge(5, 8)
    G.add_edge(5, 1)
    nb = G.neighbours(5)
    assert list(nb) == [1, 2, 8]


def test_component_size():
    G = sg.StreamGraph()
    for i in range(9):
        G.add_edge(i, i + 1)
    assert G.component_size(0) == 10


def test_auto_expansion():
    G = sg.StreamGraph(n_nodes=0)
    G.add_edge(0, 999999)
    assert G.same_component(0, 999999)
    assert G.n_components() == 1  #Model A: 2 touched nodes, connected


def test_preallocated():
    G = sg.StreamGraph(n_nodes=1000)
    G.add_edge(0, 999)
    #Model A: only the 2 touched nodes count, and they are connected
    assert G.n_components() == 1
    assert G.n_nodes() == 2


def test_benchmark_add_edge():
    """Report-only benchmark: amortised time per add_edge. Target < 500 ns."""
    n = 1_000_000
    m = 10_000_000
    rng = np.random.default_rng(42)
    us = rng.integers(0, n, size=m, dtype=np.uint32)
    vs = rng.integers(0, n, size=m, dtype=np.uint32)

    G = sg.StreamGraph(n_nodes=n)
    t0 = time.perf_counter()
    for i in range(m):
        G.add_edge(int(us[i]), int(vs[i]))
    elapsed = time.perf_counter() - t0

    ns_per_edge = elapsed * 1e9 / m
    print(f"\n[benchmark] {m} add_edge calls in {elapsed:.2f}s "
          f"-> {ns_per_edge:.1f} ns/edge ({elapsed * 1e3 / m:.6f} ms/edge), "
          f"target < 500 ns/edge")
