"""Streaming (dynamic) graph analytics backed by a C++17 core: incremental
connected components, triangle counting, clustering, degree tracking,
neighbourhood queries, PageRank, SCC and approximate betweenness."""

__version__ = "0.3.0"

from collections import deque
import math
import operator
from ._edgestream import StreamGraph


class NodeIndex:
    """Map arbitrary hashable keys to contiguous uint32 node IDs.

    IDs are assigned in insertion order starting from 0. A dict plus a list give
    O(1) lookups in both directions; kept separate from the C++ core.

    >>> idx = NodeIndex()
    >>> idx["account_A"]
    0
    >>> idx["account_B"]
    1
    >>> idx["account_A"]
    0
    """

    __slots__ = ("_key_to_id", "_keys")

    def __init__(self):
        self._key_to_id = {}
        self._keys = []

    def __getitem__(self, key):
        """Return the ID for key, assigning a new one on first use."""
        i = self._key_to_id.get(key)
        if i is None:
            i = len(self._keys)
            self._key_to_id[key] = i
            self._keys.append(key)
        return i

    def id(self, key):
        """Return the ID for an existing key; raise KeyError if absent."""
        return self._key_to_id[key]

    def key(self, node_id):
        """Return the key for an existing node ID; raise IndexError if absent."""
        if isinstance(node_id, bool):
            raise IndexError("node id must be a non-negative integer")
        try:
            node_id = operator.index(node_id)
        except TypeError as exc:
            raise IndexError("node id must be a non-negative integer") from exc
        if node_id < 0:
            raise IndexError("node id must be a non-negative integer")
        return self._keys[node_id]

    def n_nodes(self):
        return len(self._keys)

    def __len__(self):
        return len(self._keys)

    def __contains__(self, key):
        return key in self._key_to_id

    def keys(self):
        """All keys in insertion (ID) order."""
        return list(self._keys)

    def ids(self):
        """All IDs in insertion order: [0, 1, ..., n_nodes()-1]."""
        return list(range(len(self._keys)))


def to_networkx(G, index=None):
    """Convert a StreamGraph to a networkx Graph/DiGraph.

    Parameters
    ----------
    G : StreamGraph
    index : NodeIndex, optional
        When given, node ids are relabelled back to their original keys.

    Requires networkx (``pip install networkx``).
    """
    import networkx as nx

    out = nx.DiGraph() if G.directed else nx.Graph()
    label = (lambda i: index.key(int(i))) if index is not None else int
    out.add_nodes_from(label(u) for u in G.nodes())
    us, vs, ws = G.edge_list(weights=True)
    for u, v, w in zip(us, vs, ws):
        out.add_edge(label(u), label(v), weight=float(w))
    return out


def from_networkx(nxg):
    """Convert a networkx graph to (StreamGraph, NodeIndex).

    Node labels are mapped to contiguous integer ids through the returned
    NodeIndex; edge ``weight`` attributes are preserved (default 1.0).
    """
    weighted = any("weight" in d for _, _, d in nxg.edges(data=True))
    G = StreamGraph(n_nodes=nxg.number_of_nodes(), directed=nxg.is_directed(), weighted=weighted)
    idx = NodeIndex()
    for n in nxg.nodes():
        G.add_node(idx[n])
    for u, v, d in nxg.edges(data=True):
        if weighted:
            G.add_edge(idx[u], idx[v], float(d.get("weight", 1.0)))
        else:
            G.add_edge(idx[u], idx[v])
    return G, idx


def to_scipy_sparse(G):
    """Return the adjacency matrix as a scipy.sparse.coo_matrix (float64).

    Undirected edges appear symmetrically. Requires scipy.
    """
    import numpy as np
    from scipy.sparse import coo_matrix

    us, vs, ws = G.edge_list(weights=True)
    n = G.n_ids()
    if not G.directed:
        us, vs = np.concatenate([us, vs]), np.concatenate([vs, us])
        ws = np.concatenate([ws, ws])
    return coo_matrix((ws, (us, vs)), shape=(n, n))


class SlidingWindowGraph:
    """Timestamp-aware wrapper that expires edges outside a fixed window.

    Timestamps must be finite numbers supplied in non-decreasing order.
    Re-observing an edge refreshes its expiry time; on weighted graphs it also
    replaces the stored weight.
    """

    __slots__ = ("graph", "window", "_events", "_active", "_last_time")

    def __init__(self, window, *, n_nodes=0, directed=False, weighted=False):
        window = float(window)
        if not math.isfinite(window) or window <= 0:
            raise ValueError("window must be finite and positive")
        self.graph = StreamGraph(n_nodes=n_nodes, directed=directed, weighted=weighted)
        self.window = window
        self._events = deque()
        self._active = {}
        self._last_time = -math.inf

    def _key(self, u, v):
        return (u, v) if self.graph.directed or u <= v else (v, u)

    def advance(self, timestamp):
        """Expire edges older than timestamp minus window; return their count."""
        timestamp = float(timestamp)
        if not math.isfinite(timestamp) or timestamp < self._last_time:
            raise ValueError("timestamps must be finite and non-decreasing")
        self._last_time = timestamp
        cutoff = timestamp - self.window
        removed = 0
        while self._events and self._events[0][0] < cutoff:
            ts, key = self._events.popleft()
            if self._active.get(key) == ts:
                del self._active[key]
                removed += bool(self.graph.remove_edge(*key))
        return removed

    def add_edge(self, u, v, timestamp, w=1.0):
        """Expire old edges, then add or refresh one timestamped edge."""
        self.advance(timestamp)
        key = self._key(u, v)
        is_new = self.graph.add_edge(u, v, w)
        if not is_new and self.graph.weighted and u != v:
            self.graph.update_edge_weight(u, v, w)
        if u != v:
            ts = float(timestamp)
            self._active[key] = ts
            self._events.append((ts, key))
        return is_new

    def snapshot(self):
        """Return an independent StreamGraph copy of the current window."""
        return self.graph.copy()

    def __getattr__(self, name):
        return getattr(self.graph, name)


__all__ = [
    "StreamGraph",
    "NodeIndex",
    "to_networkx",
    "from_networkx",
    "to_scipy_sparse",
    "SlidingWindowGraph",
]
