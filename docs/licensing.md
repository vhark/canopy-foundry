# Licensing, rights and distribution decisions

Decision baseline 2026-10-07; public noncommercial source licensing authorized 2026-10-08. This is an engineering/commercial control plan, not legal advice or a substitute for signed agreements.

## Original game and repository

The `vhark/canopy-foundry` repository publishes original project code, documentation and project-owned content under [PolyForm Noncommercial 1.0.0](../LICENSE), except files or components carrying their own license. The standard license permits noncommercial use, changes and redistribution and defines additional permitted personal and organizational uses. Preserve the full terms and required copyright notice when redistributing. Commercial rights outside those permissions require a separate agreement from the relevant copyright holders; the contributors retain their own rights.

This is **source-available**, not OSI open source: the [Open Source Definition](https://opensource.org/osd) does not permit restricting commercial fields of use. Publication does not transfer trademarks, grant rights in Unreal Engine or third-party assets, or replace upstream MIT/CC0 and other licenses. The immutable OpenCEA/GrowBIM distributions on the implementation branch retain their embedded upstream notices and grants; the project's noncommercial restriction does not override them.

The working title is **Grownetics: Canopy Foundry**. Public source publication is not trademark clearance, a vendor endorsement, or approval to sell/distribute a packaged game. Resolve the publishing entity, contributor/contractor rights and title/store/signing obligations before commercial distribution.

No confidential facility model, licensed vendor master or private dataset may be published as a fixture. Existing repositories remain separate. Public CI logs and artifacts require the same disclosure review as tracked source; owner-only execution does not make their output private. Engine archives, source trees, SDKs and raw engine build logs stay outside public repository artifacts.

## Unreal

Unreal is a proprietary commercial dependency. The current [Epic licensing summary](https://www.unrealengine.com/license), inspected in a browser on 2026-10-07 after static access returned 403, describes royalty-based games, a standard 5% royalty on attributable lifetime gross product revenue above USD 1 million, Epic Games Store revenue exclusions, and release/reporting obligations. Budget a royalty reserve; do not assume sponsorship, publisher advances, DLC or other attributable receipts are excluded. Counsel/accounting must apply the actual accepted [EULA](https://www.unrealengine.com/eula/unreal), exclusions, notice timing and any negotiated/eligible program terms before revenue or distribution.

The non-game seat-based licensing category is distinct. A future separately licensed business training/design application may need a different analysis; calling a B2B product a game does not establish its category. Engine source access is not an open-source license. Do not publish engine code/binaries or distribute editor tools except through channels the accepted terms permit.

The older publicly accessible Epic PDF EULA reviewed during research is historical evidence, **not** the contractual authority for the chosen 5.8.1 account/license. Current terms and account acceptance must be archived by the publisher before implementation distribution.

## Authoring tools and generated output

- Local metadata identifies OpenCEA core as MIT and GrowBIM as MIT with CC0-1.0 resources. Preserve file-level notices and review actual pinned source before reuse.
- IfcOpenShell 0.8.5 is LGPL-3.0-or-later in the inspected dependency metadata. Blender/Bonsai carry their own open-source terms. The game does not link those tool binaries into its Unreal runtime or bundle their installers by default.
- Run authoring/conversion in separate, supported tool environments; this is a deliberate distribution boundary, not a blanket legal conclusion that every integration is automatically permissible. Review copyleft and Unreal non-compatible-license restrictions before distributing any combined tool/plugin.
- [Blender's license FAQ](https://www.blender.org/about/license/) permits commercial use of creator-owned output; it does not grant rights in third-party CAD, textures, logos, photography, fonts or scans used as input.
- RFA/Revit, DWG/STEP converters, Datasmith exporters, Marketplace/Fab assets, EOS/Steam SDKs, fonts and audio each require their own license and platform/distribution review. The engine EULA does not license every adjacent asset.

## Vendor assets, data and marks

A public download is not permission to redistribute an asset or derivative in a commercial game. BIMobject/BIMsmith design-workflow grants are not assumed to cover game packaging. Obtain rights from the appropriate holder for geometry, derivative/LOD conversion, materials/textures, logos/names, marketing captures, performance/photometric tables, update distribution, platforms, territories, duration and existing-save continuity. Review any source-portal restrictions as well as the manufacturer's permission.

Use generic original equipment when a grant is absent. Do not copy a distinctive protected product mesh/logo and call it generic. Factual source-backed dimensions and independently authored representations still receive a documented IP/brand review before commercial release. Supplier photos can be reference-only without becoming redistributable textures.

Every shipped asset has an approved rights entry, exact source/approval hashes and release scope. Original studio art also has a creator/assignment record. Unknown rights fail closed. Restricted masters and legal originals live in access-controlled storage, not Git LFS or a game pak. Approved cooked assets are less editable, not inherently immune to extraction; negotiate rights for actual game distribution rather than claiming encryption guarantees secrecy.

Sponsors fund permitted content/promotion; they do not alter physics or scoring. A real SKU's performance can differ only through admitted data/model versions. Handle withdrawal and perpetual existing-save data rights explicitly as specified in the [vendor program](vendor-program.md); no silent model replacement under a still-branded label.

## Facilities, simulation and players

Real facility models can expose commercially sensitive layout, installed equipment or location. Obtain owner authorization, redact unnecessary identifiers and use original fictional qualification fixtures by default. Measured weather/crop/telemetry datasets require source/use/redistribution rights and a declared validation purpose.

A model labeled calibrated or validated is only qualified within its recorded stated-use domain; it is not building, horticultural, safety, medical or financial certification. Cannabis content receives jurisdiction/age-rating/store review. Co-op authentication and opt-in diagnostics require privacy review and data minimization; offline play does not require telemetry consent.

## Release approval record

R02/R04/V03 must produce an artifact-bound approval checklist: publisher/contributor rights; current engine/SDK terms; SBOM/notices; rights ledger; trademark/marketing approval; content rating; privacy; signing/store requirements; financial reporting obligations; license expiry/withdrawal behavior; and archive/retention permissions. Missing approval blocks the affected content or claim. It does not justify pretending that a hypothetical vendor agreement already exists.
