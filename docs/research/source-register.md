# Source register and observed boundaries

Initial research date: 2026-10-07. This register separates inspected capability from decisions and future work. The planning research did not execute a game, qualify performance, acquire vendor rights or run Bonsai. Implementation has since started; see the [current build status](../../README.md#native-source-build) and [skill evaluation](skill-evaluation.md). A standalone core build is not a game or BIM qualification.

## Local authoritative evidence

The following table records inspected interfaces in the existing sibling repositories. F01 now retains immutable OpenCEA/GrowBIM wheels built from source commit `91b3b6b855cea542667fcd9794fd3aaccb208e1f`; [artifact provenance and hashes](../../dependencies/upstream/README.md) distinguish those approved bytes from a sibling editable checkout. These artifacts preserve the existing narrow functionality, not B01's future general-facility/MEP profile.

| ID | Existing repository/path | Observed scope |
|---|---|---|
| L01 | `opencea/docs/contracts.md` | OpenCEA 0.1 program, optional 0.2 design envelopes, provenance, immutable revisions/CAS/retries; not qualified engineering design |
| L02 | `opencea/packages/opencea/src/opencea/schemas/0.1/` and `schemas/0.2/` | Packaged authoritative JSON schemas; no game-owned duplicate schema authority |
| L03 | `opencea/packages/opencea/src/opencea/identity.py` | Separate source/product/element/installed-asset/role/channel identities and lifecycle/temporal validation |
| L04 | `opencea/packages/growbim/src/growbim/ifc/__init__.py` | Existing lazy API: fixtures, candidate validation, reference-room/rack operations and placement reconciliation |
| L05 | `opencea/packages/growbim/src/growbim/ifc/_native.py` | IFC4 fixture generation/validation, units and nominal systems; not a duct/utility solver or general equipment asset library |
| L06 | `opencea/packages/growbim/src/growbim/ifc/reference_room.py` and `exchange.py` | Real narrow one-room authoring and strict selected-rack rigid-placement reconciliation; arbitrary imported edits are rejected |
| L07 | `opencea/packages/growbim/src/growbim/resources/catalog/reference-rack.json` | Source-backed Metro MQ-2448G-80-M4 envelope, not detailed licensed manufacturer CAD; internal members/caster sweep/cultivation suitability unknown |
| L08 | `opencea/packages/growbim/src/growbim/project/store.py` and `jobs/store.py` | POSIX `fcntl` local locking, so not a Windows runtime dependency |
| L09 | `opencea/docs/compatibility.md` and `Grownetics Sim/docs/one-room-qualification.md` | Historical Blender 5.2.2 LTS / Bonsai 0.8.5 / IfcOpenShell 0.8.5 and independent web-ifc 0.0.78 qualification for the bounded rack placement case |
| L10 | `Grownetics Sim/src/grownetics_sim/design.py`, `bridge.py` and `pyproject.toml` | Existing JSON bridge/projected view/candidate acceptance flow, Python3.12 and sibling-editable dependencies; no general game engine or calibrated crop model |
| L11 | `Grownetics Sim/README.md` | Explicit synthetic-study boundaries, source-backed rack, no real controls, commercial asset permissions not established |

Existing Python entrypoints include `growbim.ifc.build_reference_room`, `move_reference_rack`, `resize_reference_room`, `validate_candidate`, `reconcile_placement_edit`, `reference_access` and element lifecycle operations. GrowBIM CLI has `design accept`, project/import/export and jobs commands; there is no existing general facility/rack/duct game-export CLI. The new creator tool and generalized upstream profile in B01–B06 are new work.

Existing `IfcSystem`/group/service relations do not establish MEP ports, flow directions, capacity maps or working climate control. Game purchase/installation must not fabricate a reported real-world asset. Schema/version migration must preserve historical accepted bytes and old qualification cases.

## Primary engine/platform evidence

| ID | Source | Used for / limit |
|---|---|---|
| E01 | [Official Unreal 5.8 announcement and hotfix notes](https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274) | Inspected official announcement dated 2026-06-17 and 5.8.1 hotfix notes. Chosen baseline is 5.8.1; no engine was installed or built here. The news page's static fetch returned 403; the official forum API was readable. |
| E02 | [macOS requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.8) | Xcode26.1.1 recommended, 26.4 incompatible; Mac Nanite/VSM beta and hardware RT/MegaLights experimental support are not mandatory product features. Engine minima are not measured game requirements. |
| E03 | [Linux requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine?application_version=5.8) | Linux x86_64 toolchain/sysroot, v26 clang20.1.8, Vulkan/driver constraints; native Mac build path cannot be replaced by a Windows cross-compile claim. |
| E04 | [Visual Studio requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.8) | VS2026 18.0 supported/recommended for general development; MSVC14.50 and SDK10.0.26100 selected. Exact installed builds/hashes are captured at F01. |
| E05 | [Lumen performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine?application_version=5.7) | Documents internal-resolution/TSR versus output-resolution tradeoff and High/Epic budgets. This 5.7 guidance motivates measurement, not a claim that 5.8.1 or this game achieves the same numbers. |
| E06 | [Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine), [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine), [PSO precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine) | Proposed instrumentation/streaming/shader preparation tools, not measured game performance. |
| E07 | [Current Epic licensing summary](https://www.unrealengine.com/license) and [EULA](https://www.unrealengine.com/eula/unreal) | Licensing summary inspected in real browser after static403; royalty/seat categories distinct. Publisher must archive actual accepted current terms. Historical public PDF was not treated as current contract. |

All hardware, frame-time, memory, instance and acceleration budgets in this repository are **chosen targets**, not claims from Epic or results measured in this planning task. Beta/experimental features remain optional even when available on selected hardware.

## Asset/authoring and rights sources

| ID | Primary source | Evidence and limit |
|---|---|---|
| A01 | [IfcOpenShell geometry iterator](https://docs.ifcopenshell.org/ifcopenshell/geometry_iterator.html) | Cached/reused triangulation; geometry conversion alone does not preserve all semantic/MEP/game data |
| A02 | [Bonsai official add-on](https://extensions.blender.org/add-ons/bonsai/) and [authoring guide](https://docs.bonsaibim.org/studio/guides/authoring/starting_new_project.html) | External IFC editing; IFC and .blend saved separately; no supposed general native RFA import |
| A03 | [Blender licensing FAQ](https://www.blender.org/about/license/) | Creator-owned output can be commercial; source/vendor assets retain independent rights |
| A04 | [glTF support](https://dev.epicgames.com/documentation/unreal-engine/gltf-file-format-support-in-unreal-engine?application_version=5.7), [Datasmith file types](https://dev.epicgames.com/documentation/en-us/unreal-engine/datasmith-supported-software-and-file-types?application_version=5.7), [Datasmith platforms](https://dev.epicgames.com/documentation/en-us/unreal-engine/datasmith-supported-platforms?application_version=5.7) | 5.7 documentation is a lead, not proof of the pinned 5.8.1 pipeline. Platform/file-format support pages need exact-path qualification; GLB + semantic sidecar is the selected baseline, Datasmith optional/offline. |
| A05 | [BIMobject terms](https://business.bimobject.com/terms-of-service-eula/) | Content/design-use restrictions and retained rights; not assumed permission for commercial game redistribution |
| A06 | [BIMsmith terms](https://bimsmith.com/legal/terms-and-conditions) | Design-process service/content grant is not assumed sublicensable game distribution |
| A07 | [Apple Developer ID](https://developer.apple.com/developer-id/) and [notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) | Release signing/notarization requirements, not evidence this new game has a signed build |
| A08 | [GitHub secure Actions use](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions) | Separate trusted native/asset/signing runners from untrusted pull requests |

## Real equipment acquisition leads

These are official product/data leads, not verified partnerships, granted rights or assets downloaded into the repo. Variant-specific dimensions/curves and legal permissions must be confirmed in V01/V02.

- [GGS/Pipp rolling benches](https://ggs-greenhouse.com/rolling-benches-vertical-growing/): actual greenhouse mobile-aisle products; no CAD download established on the inspected page.
- [Montel GREENRAK mobile benches](https://www.montel.com/vertical-farming/products/mobile-grow-systems/mobile-benches) and [GROW&ROLL mechanical-assist racks](https://www.montel.com/vertical-farming/products/mobile-grow-systems/heavy-duty-mechanical-assist-grow-system): tiered/mobile equipment; public specifications are not a redistributable master model.
- [Greenheck AX inline fans](https://www.greenheck.com/products/fans/inline/ax): official page links RFA/DWG models; family max airflow/pressure must not be combined into a fictitious operating point. Seek configured selection data and redistribution rights.
- [Quest 335](https://www.questclimate.com/dehumidifiers/quest-335/) and [ducted RFA page](https://www.questclimate.com/documentation/3d-models/quest-335-ducted-revit-rfa/): official model lead; moisture capacity changes with stated ambient conditions; no constant all-weather extraction claim.
- [Philips GreenPower toplighting compact specification](https://www.assets.signify.com/is/content/Signify/Assets/philips-lighting/global/20260127-greenpower-led-toplighting-compact.pdf): variants, dimensions/optics/connectors; no detailed game-mesh rights established.
- [Fluence SPYDR 3](https://fluence-led.com/products/spydr-series/spydr-3/): rack lighting variants/specifications; ask for licensed CAD, optical/performance data and marks.
- [Munters Euroemme EDS HE](https://www.climatecontrolairtreatment.com/emea/en/products/air-distribution/circulation-fans/munters-euroemme-eds-he/): circulation fan, not a substitute for a ducted inline fan operating curve.
- [Netafim FertiKit 5G](https://www.netafim.com/en/products-and-solutions/tools/fertigation/fertikit-5g/): configurable fertigation assemblies; selection/configuration matters; no downloadable CAD established on the inspected page.

## Explicit unresolved external prerequisites

Engine/SDK account entitlement and approved build machines; upstream general-facility/MEP implementation and immutable release; signed SKU-specific content/data/mark permissions; qualified operating/crop datasets and reviewers for stronger validity claims; publisher/store/signing accounts; and real playtest participants. These are assigned gates in the plan, not fields to fill with invented values. Generic original equipment and honestly illustrative models keep the core game independent of vendor participation and calibration sponsorship.
