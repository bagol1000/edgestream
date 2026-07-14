"""Small reproducible micro-benchmarks for release notes and regression checks."""

import platform
import sys
import time

import numpy as np
import edgestream as es


def timed(label, fn):
    start = time.perf_counter()
    result = fn()
    elapsed = time.perf_counter() - start
    print(f"{label:32s} {elapsed:9.4f} s")
    return result


def main():
    print(f"Python {sys.version.split()[0]} | {platform.platform()} | NumPy {np.__version__}")
    rng = np.random.default_rng(7)
    n = 100_000
    us = rng.integers(0, 50_000, n, dtype=np.uint32)
    vs = rng.integers(0, 50_000, n, dtype=np.uint32)

    g = es.StreamGraph(n_nodes=50_000)
    timed("batch 100k edges / 4 threads", lambda: g.add_edges(us, vs, n_threads=4))
    timed("100k avg_clustering queries", lambda: [g.avg_clustering() for _ in range(n)])

    w = es.StreamGraph(n_nodes=50_000, weighted=True)
    w.add_edges(us, vs, np.ones(n), n_threads=4)
    timed("100k strength queries", lambda: [w.strength(int(x)) for x in us])

    hub = es.StreamGraph()
    for v in range(1, 20_001):
        hub.add_edge(0, v)
    timed("10k hub/leaf insertions", lambda: [hub.add_edge(v, v + 10_000) for v in range(1, 10_001)])


if __name__ == "__main__":
    main()
