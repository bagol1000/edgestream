"""Streaming benchmark: edgestream vs NetworkX, igraph and rustworkx.

Scenario: 100k random edges arrive one at a time over 50k nodes; after every
1000 edges the pipeline must answer three questions about the *current* graph:
number of connected components, global triangle count, and the degree of a
probe node. That is 100 query checkpoints over the stream.

edgestream answers from its incrementally maintained state. The static
libraries keep their native graph structure up to date (cheap appends) and
run their component/triangle algorithms at each checkpoint, which is the
standard way to use them on a growing graph.

Run:  python bench/compare.py
"""

import platform
import sys
import time

import numpy as np

N_NODES = 50_000
N_EDGES = 100_000
CHECK_EVERY = 1_000
PROBE = 7

rng = np.random.default_rng(42)
US = rng.integers(0, N_NODES, N_EDGES, dtype=np.uint32)
VS = rng.integers(0, N_NODES, N_EDGES, dtype=np.uint32)
EDGES = [(int(u), int(v)) for u, v in zip(US, VS)]


def bench_edgestream():
    import edgestream as es
    G = es.StreamGraph(n_nodes=N_NODES)
    t0 = time.perf_counter()
    full = reduced = 0
    for i, (u, v) in enumerate(EDGES, 1):
        G.add_edge(u, v)
        if i % CHECK_EVERY == 0:
            components = G.n_components()
            degree = G.degree(PROBE)
            full ^= components ^ int(G.triangle_count()) ^ degree
            reduced ^= components ^ degree
    return time.perf_counter() - t0, full, reduced


def bench_networkx():
    import networkx as nx
    G = nx.Graph()
    t0 = time.perf_counter()
    full = reduced = 0
    for i, (u, v) in enumerate(EDGES, 1):
        if u != v:
            G.add_edge(u, v)
        if i % CHECK_EVERY == 0:
            tri = sum(nx.triangles(G).values()) // 3
            components = nx.number_connected_components(G)
            degree = G.degree(PROBE) if PROBE in G else 0
            full ^= components ^ tri ^ degree
            reduced ^= components ^ degree
    return time.perf_counter() - t0, full, reduced


def bench_igraph():
    import igraph as ig
    G = ig.Graph(n=N_NODES)
    seen = set()
    touched = set()
    t0 = time.perf_counter()
    full = reduced = 0
    buf = []
    for i, (u, v) in enumerate(EDGES, 1):
        key = (u, v) if u < v else (v, u)
        if u != v and key not in seen:
            seen.add(key)
            buf.append(key)
            touched.add(u)
            touched.add(v)
        if i % CHECK_EVERY == 0:
            if buf:
                G.add_edges(buf)   # igraph strongly prefers batched appends
                buf.clear()
            # igraph counts the pre-allocated isolated vertices as singleton
            # components; subtract them so the answer matches edgestream's
            comps = len(G.connected_components()) - (N_NODES - len(touched))
            tri = len(G.list_triangles())
            degree = G.degree(PROBE)
            full ^= comps ^ tri ^ degree
            reduced ^= comps ^ degree
    return time.perf_counter() - t0, full, reduced


def bench_rustworkx():
    # NOTE: rustworkx has no triangle-count API, so its checkpoints answer only
    # components + degree — strictly less work than the other candidates.
    import rustworkx as rx
    G = rx.PyGraph()
    G.add_nodes_from(range(N_NODES))
    seen = set()
    touched = set()
    t0 = time.perf_counter()
    reduced = 0
    for i, (u, v) in enumerate(EDGES, 1):
        key = (u, v) if u < v else (v, u)
        if u != v and key not in seen:
            seen.add(key)
            G.add_edge(u, v, None)
            touched.add(u)
            touched.add(v)
        if i % CHECK_EVERY == 0:
            comps = rx.number_connected_components(G) - (N_NODES - len(touched))
            reduced ^= comps ^ G.degree(PROBE)
    return time.perf_counter() - t0, None, reduced


def main():
    print(f"Python {sys.version.split()[0]} | {platform.platform()} | NumPy {np.__version__}")
    results = {}
    checks = {}
    for name, fn in [
        ("edgestream", bench_edgestream),
        ("rustworkx", bench_rustworkx),
        ("igraph", bench_igraph),
        ("networkx", bench_networkx),
    ]:
        try:
            dt, full, reduced = fn()
            results[name] = dt
            checks[name] = (full, reduced)
            shown = full if full is not None else reduced
            print(f"{name:12s} {dt:10.2f} s   (checksum {shown})")
        except Exception as exc:  # a competitor missing shouldn't kill the run
            print(f"{name:12s} skipped: {exc}")
    if "edgestream" in results:
        expected_full, expected_reduced = checks["edgestream"]
        for name, (full, reduced) in checks.items():
            if name == "edgestream":
                continue
            if full is not None and full != expected_full:
                raise RuntimeError(f"{name} returned a different full checksum")
            if reduced != expected_reduced:
                raise RuntimeError(f"{name} returned a different component/degree checksum")
        print("checksums: verified")
        base = results["edgestream"]
        for name, dt in results.items():
            if name != "edgestream":
                print(f"  {name} / edgestream: {dt / base:,.0f}x")


if __name__ == "__main__":
    main()
