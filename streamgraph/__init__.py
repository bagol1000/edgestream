"""Streaming (dynamic) graph analytics backed by a C++17 core: incremental
connected components, triangle counting, degree tracking, neighbourhood queries
and approximate betweenness."""

__version__ = "0.1.0.9002"

from ._streamgraph import StreamGraph


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


__all__ = ["StreamGraph", "NodeIndex"]
