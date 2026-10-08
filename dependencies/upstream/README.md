# Immutable OpenCEA authoring dependencies

These immutable wheels come from committed, reviewed OpenCEA/GrowBIM source, not
a sibling editable checkout. The upstream repository has no configured publication
remote; its private source commits and built artifacts are retained locally.
`uv.lock` records the wheels' relative paths and SHA-256 hashes.

| Artifact | SHA-256 |
|---|---|
| `opencea-0.1.0-py3-none-any.whl` | `6bc833fcf21644c260aeeb21ce6912443a36a8926ccf48a6fdd173550dcfbbb9` |
| `growbim-0.2.0-py3-none-any.whl` | `299d153be788261bcb13d7ba508784338e9d91aee3965c25205dc7444a7779d2` |

## Source and build evidence

OpenCEA 0.1.0 retains the original wheel from source commit
`91b3b6b855cea542667fcd9794fd3aaccb208e1f`. Its original two-package source archive
SHA-256 is `8b55043e3bcd0fe60702cb22b6302be21a4327c395a4fb4978769ae5f050d3aa`.
The original wheel bytes and published program/design schemas are unchanged.

GrowBIM 0.2.0 comes from upstream B01 commit
`093e09656f04ecbf48677c43ceb92ce549d09ff5`. Its `packages/growbim` source archive
SHA-256 is `d57fc4e03ca798f96417c3c98e48cc040188315a1afcab92ff5cf2a2f8b024d4`.
The source commit includes the actual facility profile, native validation and
reviewed MEP reconcile route, durable acceptance, regression tests and qualification
record. No private engine or user facility source is included in these wheels.

The wheels were built using uv 0.12.3 and Python 3.12.13:

```sh
# Original OpenCEA wheel; already retained unchanged:
git archive --format=tar --output=upstream-source.tar \
  91b3b6b855cea542667fcd9794fd3aaccb208e1f packages/opencea packages/growbim
mkdir upstream-source
tar -xf upstream-source.tar -C upstream-source
uv build --wheel --no-sources --python 3.12.13 \
  upstream-source/packages/opencea --out-dir wheels

# GrowBIM B01 replacement, from the upstream repository:
git archive --format=tar --output=facility-source.tar \
  093e09656f04ecbf48677c43ceb92ce549d09ff5 packages/growbim
mkdir facility-source
tar -xf facility-source.tar -C facility-source
uv build --wheel --no-sources --python 3.12.13 \
  facility-source/packages/growbim --out-dir wheels
```

These commands explain artifact origin; byte-for-byte reproducibility of the
wheel-generation environment has not been independently qualified. Frozen
consumer builds use the recorded wheel bytes, not a fresh source build.

## Rights and scope

Each wheel retains its MIT license in its `.dist-info/licenses/LICENSES/`
directory, copyright 2026 OpenCEA contributors. GrowBIM also retains its
`CC0-1.0.txt` license. Source/data-specific notices remain authoritative; this
artifact inclusion grants no manufacturer-CAD, logo, trademark or vendor-data
rights.

GrowBIM 0.2.0 adds the explicit original one-storey IFC4 First Shipment facility
profile: 216 m² gross floor, enclosed rooms/doors, bench/canopy geometry, service
spaces and typed connected MEP ports. The selected-MEP review route is separate
from unchanged legacy rack-only placement exchange. Upstream verification passed
136 contract/project/IFC tests and 88 PostgreSQL service tests, plus live authenticated
HTTP acceptance/replay, stale/conflicting-request rejection, malformed geometry/access
rejection, HEAD preservation and export. This does not qualify arbitrary CAD editing,
Bonsai round trips, calibrated equipment or professional engineering.

These Python wheels are authoring/qualification dependencies only. They must not
become prerequisites for native gameplay. Later upstream changes require a new
immutable package version, source record and lock update together; never overwrite
an existing version's wheel bytes.
