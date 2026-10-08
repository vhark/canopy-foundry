# Source register and observed boundaries

Initial research date: 2026-10-07. This register separates inspected capability from decisions and future work. The planning research did not execute a game, qualify performance, acquire vendor rights or run Bonsai. Implementation has since started; see the [current build status](../../README.md#native-source-build) and [skill evaluation](skill-evaluation.md). A standalone core build is not a game or BIM qualification.

## Local authoritative evidence

The table records existing sibling interfaces and their qualified extensions. The frozen authoring environment retains unchanged OpenCEA 0.1.0 bytes from `91b3b6b855cea542667fcd9794fd3aaccb208e1f` and GrowBIM 0.2.0 from B01 commit `093e09656f04ecbf48677c43ceb92ce549d09ff5`; [artifact provenance and hashes](../../dependencies/upstream/README.md) distinguish these immutable wheels from sibling editable checkouts.

| ID | Existing repository/path | Observed scope |
|---|---|---|
| L01 | `opencea/docs/contracts.md` | OpenCEA 0.1 program, optional 0.2 design envelopes, provenance, immutable revisions/CAS/retries; not qualified engineering design |
| L02 | `opencea/packages/opencea/src/opencea/schemas/0.1/` and `schemas/0.2/` | Packaged authoritative JSON schemas; no game-owned duplicate schema authority |
| L03 | `opencea/packages/opencea/src/opencea/identity.py` | Separate source/product/element/installed-asset/role/channel identities and lifecycle/temporal validation |
| L04 | `opencea/packages/growbim/src/growbim/ifc/__init__.py` | Lazy fixture, reference-room/rack and lifecycle APIs; 0.2.0 adds `build_facility` and separate `reconcile_facility_edit` |
| L05 | `opencea/packages/growbim/src/growbim/ifc/_native.py` | IFC4 fixture generation/validation and new facility-profile dispatch; not a duct/utility solver or detailed asset library |
| L06 | `opencea/packages/growbim/src/growbim/ifc/reference_room.py` and `exchange.py` | Real narrow one-room authoring and strict selected-rack rigid-placement reconciliation; arbitrary imported edits are rejected |
| L07 | `opencea/packages/growbim/src/growbim/resources/catalog/reference-rack.json` | Source-backed Metro MQ-2448G-80-M4 envelope, not detailed licensed manufacturer CAD; internal members/caster sweep/cultivation suitability unknown |
| L08 | `opencea/packages/growbim/src/growbim/project/store.py` and `jobs/store.py` | POSIX `fcntl` local locking, so not a Windows runtime dependency |
| L09 | `opencea/docs/compatibility.md` and `Grownetics Sim/docs/one-room-qualification.md` | Historical Blender 5.2.2 LTS / Bonsai 0.8.5 / IfcOpenShell 0.8.5 and independent web-ifc 0.0.78 qualification for the bounded rack placement case |
| L10 | `Grownetics Sim/src/grownetics_sim/design.py`, `bridge.py` and `pyproject.toml` | Existing JSON bridge/projected view/candidate acceptance flow, Python3.12 and sibling-editable dependencies; no general game engine or calibrated crop model |
| L11 | `Grownetics Sim/README.md` | Explicit synthetic-study boundaries, source-backed rack, no real controls, commercial asset permissions not established |
| L12 | B01 `growbim/ifc/facility.py`, `exchange.py` and service `core/projects.py` | Original 216 m² rectangular facility, actual MEP ports, native access/geometry checks and reviewed selected-MEP CAS/replay. Upstream 136 native/project/contract and 88 service tests plus live HTTP rejection/acceptance/export passed; frozen downstream wheel imported all four accepted revisions with exact artifact digests. |

Python entrypoints include the legacy reference-room/lifecycle operations and B01's new facility construction and selected-MEP reconciliation. GrowBIM CLI provides design acceptance and project/import/export commands; the new creator compiler and game-export tooling in B02–B06 remain separate work.

IFC grouping alone does not establish connectivity or performance. B01 now supplies explicit IFC4 ports/connections and typed system admission, not capacity maps or working climate control. Game purchase/installation must never fabricate reported real-world assets; new profile/schema versions must preserve historical accepted bytes and qualification cases.

## Primary engine/platform evidence

| ID | Source | Used for / limit |
|---|---|---|
| E01 | [Official Unreal 5.8 announcement and hotfix notes](https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274) | Inspected official announcement dated 2026-06-17 and 5.8.1 hotfix notes. The initial baseline was 5.8.1; the current lock is 5.8.3. No engine was installed during the initial planning research; subsequent authorized source and native UBT consumer execution are recorded in the F01 report. The news page's static fetch returned403; the official forum API was readable. |
| E02 | [macOS requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.8) | Initial 5.8.1 research recommended Xcode26.1.1 and rejected26.4. That historical constraint is not the current 5.8.3/Xcode27.0 profile; the unchanged installed 5.8.3 UBT accepts SDK27 and compiles the real project Editor. Mac Nanite/VSM beta and hardware RT/MegaLights experimental support are not mandatory product features. Engine minima are not measured game requirements. |
| E03 | [Linux requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine?application_version=5.8) | Linux x86_64 toolchain/sysroot, v26 clang20.1.8, Vulkan/driver constraints; native Mac build path cannot be replaced by a Windows cross-compile claim. |
| E04 | [Visual Studio requirements, UE5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.8) | VS2026 18.0 supported/recommended for general development; MSVC14.50 and SDK10.0.26100 selected. Exact installed builds/hashes are captured at F01. |
| E05 | [Lumen performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine?application_version=5.7) | Documents internal-resolution/TSR versus output-resolution tradeoff and High/Epic budgets. This 5.7 guidance motivates measurement, not a claim that 5.8.1 or this game achieves the same numbers. |
| E06 | [Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine), [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine), [PSO precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine) | Proposed instrumentation/streaming/shader preparation tools, not measured game performance. |
| E07 | [Current Epic licensing summary](https://www.unrealengine.com/license) and [EULA](https://www.unrealengine.com/eula/unreal) | Licensing summary inspected in real browser after static403; royalty/seat categories distinct. Publisher must archive actual accepted current terms. Historical public PDF was not treated as current contract. |
| E08 | [Epic's Unreal Engine source-access procedure](https://www.unrealengine.com/en-US/ue-on-github) | Account sign-in, GitHub connection/OAuth and organization invitation acceptance are required; a previously unsigned EULA must be reviewed/accepted by the account owner. Initial CLI access returned404. After the owner's link and organization invitation acceptance, the exact UE5.8.1 source was retrieved and pinned; [six approved native UBT consumer executions passed](f01-build-evidence.md#qualified-native-matrix-and-f01-closure). No separate license terms were accepted by the agent. |
| E09 | [Apple Xcode SDK/system requirements](https://developer.apple.com/xcode/system-requirements/) | Checked 2026-10-08: Xcode27 is stable, 27.1 is RC and 27.2 is beta. Local Xcode27.0 build27A266a supplies macOS SDK27.0 and AppleClang21.0.0. The migration pins that stable toolchain; an RC/beta or future patch is not silently admitted. Local native evidence is scoped in the [build instructions](../../README.md#full-editor-room-and-cooked-coordinate-gate). |
| E10 | [Epic UE-396802 diagnosis and fix discussion](https://forums.unrealengine.com/t/ue5-8-unable-to-build-game-on-macos-27/2746676) | UE5.8.3 still fails Xcode27 app finalization when the build executor leaves stdin closed. Epic's linked `dev-5.8` fix supplies an empty stdin to Xcode. Local reproduction matched the scheme-pre-action failure, including with `-NoUBA`; the normal Xcode-driven game scheme succeeded without modifying installed Epic binaries. The project uses Xcode compilation/finalization followed by UAT cook/stage/package/archive; native package qualification is recorded separately. |

All hardware, frame-time, memory, instance and acceleration budgets in this repository are **chosen targets**, not claims from Epic or results measured in this planning task. Beta/experimental features remain optional even when available on selected hardware.

## Asset/authoring and rights sources

| ID | Primary source | Evidence and limit |
|---|---|---|
| A01 | [IfcOpenShell geometry iterator](https://docs.ifcopenshell.org/ifcopenshell/geometry_iterator.html) | Cached/reused triangulation; geometry conversion alone does not preserve all semantic/MEP/game data |
| A02 | [Bonsai official add-on](https://extensions.blender.org/add-ons/bonsai/) and [authoring guide](https://docs.bonsaibim.org/studio/guides/authoring/starting_new_project.html) | External IFC editing; IFC and .blend saved separately; no supposed general native RFA import |
| A03 | [Blender licensing FAQ](https://www.blender.org/about/license/) | Creator-owned output can be commercial; source/vendor assets retain independent rights |
| A04 | [glTF support](https://dev.epicgames.com/documentation/unreal-engine/gltf-file-format-support-in-unreal-engine?application_version=5.7), [Datasmith file types](https://dev.epicgames.com/documentation/en-us/unreal-engine/datasmith-supported-software-and-file-types?application_version=5.7), [Datasmith platforms](https://dev.epicgames.com/documentation/en-us/unreal-engine/datasmith-supported-platforms?application_version=5.7) | 5.7 documentation is a lead, not proof of the pinned 5.8.3 pipeline. Platform/file-format support pages need exact-path qualification; GLB + semantic sidecar is the selected baseline, Datasmith optional/offline. |
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

F01 source entitlement/native core qualification and B01's first facility-profile release are cleared. Remaining gates include full engine/editor and reference-hardware provisioning for packaged runtime work; broader construction/application round trips; signed SKU-specific content/data/mark permissions; qualified operating/crop datasets and reviewers for stronger validity claims; publisher/store/signing accounts; and real playtest participants. These are assigned gates, not fields to fill with invented values. Generic original equipment and honestly illustrative models keep the core game independent of vendor participation and calibration sponsorship.
