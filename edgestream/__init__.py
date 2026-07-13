"""Streaming (dynamic) graph analytics backed by a C++17 core: incremental
connected components, triangle counting, clustering, degree tracking,
neighbourhood queries, PageRank, SCC and approximate betweenness."""

__version__ = "0.2.0"

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
    us, vs, ws = G.edge_list(weights=True)
    label = (lambda i: index.key(int(i))) if index is not None else int
    for u, v, w in zip(us, vs, ws):
        out.add_edge(label(u), label(v), weight=float(w))
    return out


def from_networkx(nxg):
    """Convert a networkx graph to (StreamGraph, NodeIndex).

    Node labels are mapped to contiguous integer ids through the returned
    NodeIndex; edge ``weight`` attributes are preserved (default 1.0).
    """
    weighted = any("weight" in d for _, _, d in nxg.edges(data=True))
    G = StreamGraph(directed=nxg.is_directed(), weighted=weighted)
    idx = NodeIndex()
    for n in nxg.nodes():
        idx[n]  # register isolated nodes too (they stay untouched in G)
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


__all__ = [
    "StreamGraph",
    "NodeIndex",
    "to_networkx",
    "from_networkx",
    "to_scipy_sparse",
]
