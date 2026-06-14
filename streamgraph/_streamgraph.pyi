"""Type stubs and documentation for the streamgraph C++ core.

Node IDs are non-negative integers (uint32), 0-indexed. All examples run in
well under two seconds.
"""

from __future__ import annotations

import numpy as np

class StreamGraph:
    """An in-memory dynamic graph maintained incrementally as edges arrive.

    Connected components (Union-Find), degree tracking, triangle counts and
    basic statistics are kept up to date on every ``add_edge``; queries are
    O(1) or O(alpha(n)).

    Parameters
    ----------
    n_nodes : int, optional
        Pre-allocate this many node slots. ``0`` (default) auto-grows.
    directed : bool, optional
        Whether edges are directed. Default ``False``.

    Examples
    --------
    >>> import streamgraph as sg
    >>> G = sg.StreamGraph()
    >>> G.add_edge(0, 1)
    True
    >>> G.add_edge(1, 2)
    True
    >>> G.same_component(0, 2)
    True
    """

    def __init__(self, n_nodes: int = 0, directed: bool = False) -> None: ...

    def add_edge(self, u: int, v: int) -> bool:
        """Add an edge between ``u`` and ``v``.

        Parameters
        ----------
        u, v : int
            Node IDs (0-indexed). Storage auto-expands to fit.

        Returns
        -------
        bool
            ``True`` if the edge was new; ``False`` for a self-loop or
            duplicate (no state change in that case).

        Examples
        --------
        >>> G = StreamGraph()
        >>> G.add_edge(0, 1), G.add_edge(0, 1), G.add_edge(2, 2)
        (True, False, False)
        """

    def add_edges(self, us: np.ndarray, vs: np.ndarray, n_threads: int = 0) -> int:
        """Batch-add edges from two parallel uint32 arrays.

        Duplicates within the batch and self-loops are removed in a parallel
        pre-pass; the unique edges are then applied serially.

        Parameters
        ----------
        us, vs : numpy.ndarray
            1-D ``uint32`` arrays of equal length holding edge endpoints.
        n_threads : int, optional
            OpenMP threads for the dedup sort. ``0`` (default) uses the
            library default.

        Returns
        -------
        int
            Number of new (non-duplicate, non-self-loop) edges added.

        Examples
        --------
        >>> import numpy as np
        >>> G = StreamGraph()
        >>> G.add_edges(np.array([0, 1], np.uint32), np.array([1, 2], np.uint32))
        2
        """

    def same_component(self, u: int, v: int) -> bool:
        """Return whether ``u`` and ``v`` are in the same component (O(alpha(n)))."""

    def component_id(self, u: int) -> int:
        """Return the root ID of ``u``'s component (O(alpha(n)))."""

    def n_components(self) -> int:
        """Return the number of connected components among touched nodes."""

    def component_size(self, u: int) -> int:
        """Return the number of nodes in ``u``'s component."""

    def component_nodes(self, u: int) -> np.ndarray:
        """Return a uint32 array of all touched nodes in ``u``'s component."""

    def component_ids(self) -> np.ndarray:
        """Return a uint32 array of the component root ID for each touched node."""

    def degree(self, u: int) -> int:
        """Return the degree of node ``u`` (0 if never referenced)."""

    def degree_histogram(self) -> np.ndarray:
        """Return a uint64 array where entry ``d`` counts nodes of degree ``d``."""

    def neighbours(self, u: int) -> np.ndarray:
        """Return a sorted uint32 array of ``u``'s neighbours.

        Examples
        --------
        >>> G = StreamGraph()
        >>> _ = [G.add_edge(5, x) for x in (8, 2, 1)]
        >>> list(G.neighbours(5))
        [1, 2, 8]
        """

    def has_edge(self, u: int, v: int) -> bool:
        """Return whether edge ``(u, v)`` exists (O(1))."""

    def triangle_count(self) -> int:
        """Return the global triangle count, each counted once (O(1)).

        Examples
        --------
        >>> G = StreamGraph()
        >>> _ = [G.add_edge(*e) for e in [(0, 1), (1, 2), (0, 2)]]
        >>> G.triangle_count()
        1
        """

    def local_triangles(self, u: int) -> int:
        """Return the number of triangles incident to node ``u`` (O(1))."""

    def all_local_triangles(self) -> np.ndarray:
        """Return a uint32 array of per-node-id local triangle counts."""

    def n_nodes(self) -> int:
        """Return the number of touched (referenced) nodes."""

    def n_edges(self) -> int:
        """Return the number of distinct edges (O(1))."""

    def density(self) -> float:
        """Return edge density: ``2m/(n(n-1))`` undirected, ``m/(n(n-1))`` directed."""

    def avg_degree(self) -> float:
        """Return the average degree: ``2m/n`` undirected, ``m/n`` directed."""

    def max_degree(self) -> int:
        """Return the maximum degree across all nodes (O(n))."""

    def betweenness_approx(
        self, k: int = 200, n_threads: int = 0, seed: int = 42
    ) -> np.ndarray:
        """Approximate betweenness centrality via random (s, t) pair sampling.

        Parameters
        ----------
        k : int, optional
            Number of (s, t) pairs to sample. Clamped to all pairs (exact) when
            it exceeds ``n*(n-1)/2``. Default 200.
        n_threads : int, optional
            OpenMP threads (0 = library default). BFS over each pair is
            independent.
        seed : int, optional
            RNG seed; identical seeds give identical results. Default 42.

        Returns
        -------
        numpy.ndarray
            ``float64`` per-node-id scores normalised to ``[0, 1]``; empty if
            the graph has no touched nodes.

        Examples
        --------
        >>> G = StreamGraph()
        >>> _ = [G.add_edge(i, i + 1) for i in range(4)]
        >>> bc = G.betweenness_approx(k=1000, seed=42)
        >>> bc[2] > bc[0]
        True
        """

    def save(self, path: str) -> None:
        """Write the graph to a binary ``.sgph`` file (Section 10 format)."""

    @staticmethod
    def load(path: str) -> StreamGraph:
        """Load a graph from a ``.sgph`` file, replaying its edges.

        Examples
        --------
        >>> import tempfile, os
        >>> G = StreamGraph()
        >>> _ = [G.add_edge(i, i + 1) for i in range(5)]
        >>> p = os.path.join(tempfile.mkdtemp(), "g.sgph")
        >>> G.save(p)
        >>> StreamGraph.load(p).n_edges()
        5
        """
