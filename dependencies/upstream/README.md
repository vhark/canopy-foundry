# Immutable OpenCEA authoring dependencies

These wheels contain the existing OpenCEA/GrowBIM packages from source commit
`91b3b6b855cea542667fcd9794fd3aaccb208e1f`, archived from the clean local OpenCEA
repository. The source repository has no configured publication remote; the
artifacts are therefore retained here rather than resolved from a sibling editable
checkout. `uv.lock` records their relative paths and SHA-256 hashes.

| Artifact | SHA-256 |
|---|---|
| `opencea-0.1.0-py3-none-any.whl` | `6bc833fcf21644c260aeeb21ce6912443a36a8926ccf48a6fdd173550dcfbbb9` |
| `growbim-0.1.0-py3-none-any.whl` | `1e4999bef0cfecc86a95e58d0915fac29b0d6f1c6f80621a59e86cc8126428f2` |

## Source and build evidence

The archive contains only `packages/opencea` and `packages/growbim` from that
commit. Its SHA-256 is
`8b55043e3bcd0fe60702cb22b6302be21a4327c395a4fb4978769ae5f050d3aa`;
`git get-tar-commit-id` confirmed the full source commit above.

The wheels were built using uv 0.12.3 and Python 3.12.13:

```sh
git archive --format=tar --output=upstream-source.tar \
  91b3b6b855cea542667fcd9794fd3aaccb208e1f packages/opencea packages/growbim
mkdir upstream-source
tar -xf upstream-source.tar -C upstream-source
uv build --wheel --no-sources --python 3.12.13 \
  upstream-source/packages/opencea --out-dir wheels
uv build --wheel --no-sources --python 3.12.13 \
  upstream-source/packages/growbim --out-dir wheels
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

The packages provide the existing narrow schemas, authoring and IFC interfaces.
They do **not** implement the B01 general-facility/MEP profile. That milestone must
produce new immutable approved artifacts and update this record and `uv.lock`
together. These Python wheels are authoring/qualification dependencies only;
they must not become prerequisites for the native gameplay runtime.
