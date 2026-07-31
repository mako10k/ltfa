# Third-Party Source

## yyjson 0.12.0

LTFA pins the yyjson amalgamated C source for the future strict observation
JSON Lines decoder.

| Field | Value |
| --- | --- |
| Upstream | `https://github.com/ibireme/yyjson` |
| Release | `0.12.0` |
| Commit | `8b4a38dc994a110abaec8a400615567bd996105f` |
| Release date | 2025-08-18 |
| License | MIT, copyright 2020 YaoYuan |
| Source form | upstream `src/yyjson.c` and `src/yyjson.h` |

The files were retrieved from commit-addressed upstream URLs:

- `https://raw.githubusercontent.com/ibireme/yyjson/8b4a38dc994a110abaec8a400615567bd996105f/src/yyjson.c`
- `https://raw.githubusercontent.com/ibireme/yyjson/8b4a38dc994a110abaec8a400615567bd996105f/src/yyjson.h`
- `https://raw.githubusercontent.com/ibireme/yyjson/8b4a38dc994a110abaec8a400615567bd996105f/LICENSE`

SHA-256:

```text
45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947  LICENSE
ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a  yyjson.c
175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3  yyjson.h
```

The isolated `ltfa_yyjson` target uses C99 and enables these compile-time
options:

```text
YYJSON_DISABLE_WRITER=1
YYJSON_DISABLE_INCR_READER=1
YYJSON_DISABLE_UTILS=1
YYJSON_DISABLE_NON_STANDARD=1
```

UTF-8 validation, the standard reader, and fast numeric conversion remain
enabled. yyjson 0.12.0's file-reading helpers remain present in the amalgamated
source, but the LTFA observation decoder will own bounded local input and use
only the memory reader. The pin task builds and tests this target without
linking it into `ltfa_core` or the CLI.

Project warnings remain errors. The vendored C target does not inherit the
project warning flags, while the C++ consumer test does. Sanitizer flags are
shared with the vendored target so the complete validation matrix instruments
it.

The decision and verification evidence are recorded in
`docs/decisions/0006-pin-yyjson.md` and
`docs/acceptance/m3-yyjson-pin.md`.

Graphviz remains an optional external renderer and is not vendored or linked
into the core.
