import math
import struct
from pathlib import Path

import numpy as np
import pytest

import edgestream as es


def test_explicit_nodes_are_first_class():
    g = es.StreamGraph(n_nodes=100)
    assert g.add_node(7)
    assert not g.add_node(7)
    assert g.add_nodes(10, 3) == 3
    assert g.nodes().tolist() == [7, 10, 11, 12]
    assert g.n_nodes() == g.n_components() == 4
    assert g.degree_histogram().tolist() == [4]
    assert g.n_ids() == 100


def test_v3_roundtrip_preserves_isolates_and_id_range(tmp_path):
    g = es.StreamGraph(n_nodes=200, directed=True, weighted=True)
    g.add_node(99)
    g.add_edge(5, 7, 2.5)
    g.remove_edge(5, 7)
    path = tmp_path / "state.esg"
    g.save(str(path))
    assert path.read_bytes()[:4] == b"EDGS"
    h = es.StreamGraph.load(str(path))
    assert h.nodes().tolist() == [5, 7, 99]
    assert h.n_ids() == 200
    assert h.n_components() == 3
    assert not (tmp_path / "state.esg.tmp").exists()


def test_v2_compatibility(tmp_path):
    path = tmp_path / "legacy.esg"
    path.write_bytes(
        struct.pack("<IHBQQII", 0x45444753, 2, 0, 2, 1, 0, 1)
    )
    g = es.StreamGraph.load(str(path))
    assert g.n_edges() == 1
    assert g.nodes().tolist() == [0, 1]


@pytest.mark.parametrize("weight", [math.inf, -math.inf, math.nan, 0.0, -1.0])
def test_weights_must_be_finite_and_positive(weight):
    g = es.StreamGraph(weighted=True)
    with pytest.raises(ValueError):
        g.add_edge(0, 1, weight)
    with pytest.raises(ValueError):
        g.add_edges(
            np.array([0], dtype=np.uint32),
            np.array([1], dtype=np.uint32),
            np.array([weight], dtype=np.float64),
        )


def test_update_weight_is_exact_and_strength_constant_time_state():
    g = es.StreamGraph(weighted=True)
    g.add_edge(0, 1, 2.0)
    assert g.update_edge_weight(1, 0, 5.0)
    assert g.edge_weight(0, 1) == 5.0
    assert g.strength(0) == g.strength(1) == 5.0
    assert g.total_weight() == 5.0
    assert not g.update_edge_weight(1, 2, 3.0)
    with pytest.raises(RuntimeError):
        es.StreamGraph().update_edge_weight(0, 1, 2.0)


def test_component_labels_are_canonical_and_copy_is_independent():
    g = es.StreamGraph()
    g.add_edge(5, 6)
    g.add_edge(1, 5)
    assert g.component_id(6) == 1
    assert g.component_ids().tolist() == [1, 1, 1]
    h = g.copy()
    h.remove_edge(1, 5)
    assert g.n_edges() == 2
    assert h.n_edges() == 1


def test_parameter_and_array_validation():
    g = es.StreamGraph()
    g.add_edge(0, 1)
    for kwargs in ({"damping": -0.1}, {"damping": 1.0}, {"tol": 0}, {"max_iter": 0}):
        with pytest.raises(ValueError):
            g.pagerank(**kwargs)
    with pytest.raises(ValueError):
        g.betweenness_approx(k=0)
    with pytest.raises(TypeError):
        g.add_edges(np.array([-1]), np.array([0]))


def test_weighted_betweenness_splits_equal_paths_independently_of_heap_order():
    g = es.StreamGraph(weighted=True)
    # The 0-1-2-3-0 cycle has two equal-length routes despite different edge
    # weights; the 0-4-5 tail makes an asymmetric counting error observable.
    edges = [(0, 4), (2, 3), (4, 5), (0, 3), (0, 1), (1, 2)]
    weights = [1.496915315443057, 0.1, 1.496915315443057,
               1.496915315443057, 0.1, 1.496915315443057]
    for (u, v), weight in zip(edges, weights):
        g.add_edge(u, v, weight)
    assert g.betweenness_approx(k=10**9, normalise=False, n_threads=1).tolist() == \
        pytest.approx([6.5, 1.5, 0.5, 1.5, 4.0, 0.0])


def test_nodeindex_rejects_negative_reverse_index():
    idx = es.NodeIndex()
    idx["a"]
    with pytest.raises(IndexError):
        idx.key(-1)
    assert idx.key(np.int64(0)) == "a"


def test_python_and_r_versions_match():
    description = Path(__file__).parents[1] / "DESCRIPTION"
    version_line = next(
        line for line in description.read_text().splitlines()
        if line.startswith("Version:")
    )
    assert es.__version__ == version_line.split(":", 1)[1].strip() == "0.3.0"


def test_networkx_roundtrip_keeps_isolates():
    nx = pytest.importorskip("networkx")
    source = nx.Graph()
    source.add_nodes_from(["isolated", "a", "b"])
    source.add_edge("a", "b")
    g, idx = es.from_networkx(source)
    assert g.n_nodes() == 3
    result = es.to_networkx(g, idx)
    assert set(result.nodes()) == set(source.nodes())
    assert nx.is_isomorphic(result, source)


def test_sliding_window_refresh_and_snapshot():
    w = es.SlidingWindowGraph(10, weighted=True)
    assert w.add_edge(0, 1, timestamp=0, w=2)
    assert not w.add_edge(0, 1, timestamp=8, w=5)
    assert w.advance(11) == 0
    assert w.edge_weight(0, 1) == 5
    snap = w.snapshot()
    assert w.advance(19) == 1
    assert w.n_edges() == 0
    assert snap.n_edges() == 1
