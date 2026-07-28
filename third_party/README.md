# Third-Party Source

No third-party source is vendored in the bootstrap scaffold.

The reviewed basic design selects yyjson for the future observation JSON Lines
decoder. It must not be added until the implementation change records:

- pinned release or commit
- upstream source URL
- license
- source checksum
- enabled compile-time options
- local build and test evidence

Graphviz remains an optional external renderer and is not vendored or linked
into the core.
