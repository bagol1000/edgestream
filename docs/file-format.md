# EDGS binary format

EDGS v3 is the portable snapshot format written by edgestream 0.3.0. Files
use little-endian unsigned integers and IEEE-754 binary64 weights.

## Header

| offset | type | meaning |
|---:|---|---|
| 0 | 4 bytes | ASCII `EDGS` |
| 4 | uint16 | version, currently 3 |
| 6 | uint8 | flags: bit 0 directed, bit 1 weighted |
| 7 | uint64 | allocated ID range, `n_ids` |
| 15 | uint64 | number of touched nodes |
| 23 | uint64 | number of edges |

Unknown flag bits are invalid.

## Body

The header is followed by:

1. `n_touched` strictly increasing uint32 node IDs;
2. `n_edges` edge records, each containing uint32 `u`, uint32 `v` and,
   for weighted files, one binary64 weight.

Undirected edges occur once. Self-loops, duplicate edges, endpoints outside
the ID range, non-finite/non-positive weights and trailing bytes are invalid.

Saving writes `path.tmp`, flushes and closes it, then renames it over the
target. A failed write therefore does not expose a partially written target.

## Compatibility

The loader accepts v2 snapshots. v2 used the historical numeric magic whose
on-disk bytes read `SGDE`, stored only touched-node count plus edge count,
and could not preserve isolated nodes or a sparse maximum ID. Loading v2
therefore reconstructs every state representable by its edge list, while all
new snapshots are written as v3.
