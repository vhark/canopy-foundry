"""Verified immutable GrowBIM revisions projected to game-only SI semantics."""
from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
from itertools import chain
from math import isfinite
from pathlib import Path

import ifcopenshell
import ifcopenshell.util.element
from growbim.ifc import validate_candidate
from growbim.project.revisions import revision_history
from growbim.project.store import LocalProjectStore
from growbim.storage import validate_digest
from opencea.contracts import ContractError, loads, parse_uuid

from .coordinates import length_unit_scale, placement_matrix, transform_points

PROFILE = "org.grownetics.first-shipment-facility.v1"
MAX_ELEMENTS = 4096
MAX_ROOMS = 512
MAX_PORTS = 1024


class SourceError(ValueError):
    """Rejected accepted-source projection, with offending file or IFC entity."""


@dataclass(frozen=True)
class AcceptedSource:
    project_id: str
    revision: str
    accepted_design_revision: str
    files: dict[str, str]
    profile: str
    valid_from: str
    valid_until: str
    decision: dict
    identity: dict
    catalog: dict
    sources: tuple[dict, ...]
    storeys: tuple[dict, ...]
    rooms: tuple[dict, ...]
    zones: tuple[dict, ...]
    equipment: tuple[dict, ...]
    canopies: tuple[dict, ...]
    ports: tuple[dict, ...]


def _property(entity, set_name, key):
    props = ifcopenshell.util.element.get_psets(entity)
    return props.get(set_name, {}).get(key)


def _polygon(entity, scale, *, top=False):
    """Extract the real swept outer polyline, not an IFC name/fixture recipe."""
    label = f"model.ifc {entity.is_a()} {entity.GlobalId}"
    try:
        (representation,) = entity.Representation.Representations
        (solid,) = representation.Items
        if not solid.is_a("IfcExtrudedAreaSolid") or not solid.SweptArea.is_a("IfcArbitraryClosedProfileDef"):
            raise SourceError(f"{label}: unsupported geometric profile")
        points = tuple(tuple(float(v) for v in point.Coordinates) for point in solid.SweptArea.OuterCurve.Points)
        if len(points) < 4 or len(points) > 65 or points[0] != points[-1] or any(len(p) != 2 for p in points):
            raise SourceError(f"{label}: invalid closed polygon")
        z = float(solid.Depth) if top else 0.0
        local = placement_matrix(entity.ObjectPlacement, length_scale=scale)
        # The qualified facility profile has a canonical, zero-offset solid frame.
        pos = solid.Position
        if tuple(pos.Location.Coordinates) != (0.0, 0.0, 0.0) or tuple(pos.Axis.DirectionRatios) != (0.0, 0.0, 1.0) or tuple(pos.RefDirection.DirectionRatios) != (1.0, 0.0, 0.0):
            raise SourceError(f"{label}: unsupported extrusion frame")
        polygon = transform_points(((x * scale, y * scale, z * scale) for x, y in points[:-1]), local)
        if not all(isfinite(v) for vertex in polygon for v in vertex):
            raise SourceError(f"{label}: nonfinite polygon")
        area = abs(sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(polygon, polygon[1:] + polygon[:1]))) / 2
        if not 0 < area < 1_000_000:
            raise SourceError(f"{label}: invalid floor/canopy area")
        return polygon, area
    except (AttributeError, TypeError, IndexError, ValueError) as exc:
        if isinstance(exc, SourceError):
            raise
        raise SourceError(f"{label}: invalid polygon: {exc}") from exc


def _envelope(entity, scale):
    label = f"model.ifc {entity.is_a()} {entity.GlobalId}"
    try:
        (representation,) = entity.Representation.Representations
        (solid,) = representation.Items
        points = solid.SweptArea.OuterCurve.Points
        length = float(points[1].Coordinates[0]) * scale
        width = float(points[2].Coordinates[1]) * scale
        height = float(solid.Depth) * scale
        if not all(isfinite(v) and 0 < v < 10000 for v in (length, width, height)):
            raise ValueError("nonpositive or nonfinite dimensions")
        return length, width, height
    except (AttributeError, TypeError, IndexError, ValueError) as exc:
        raise SourceError(f"{label}: invalid physical envelope: {exc}") from exc


def _extract(model, identity, scale):
    elements = identity["elements"]
    if len(elements) > MAX_ELEMENTS:
        raise SourceError("identity-map.json elements: capacity exceeded")
    if len(model.by_type("IfcSpace")) > MAX_ROOMS or len(model.by_type("IfcDistributionPort")) > MAX_PORTS:
        raise SourceError("model.ifc: room/port capacity exceeded")
    crosswalk = {row["ifc_global_id"]: row for row in elements if row["state"] == "active"}
    storeys = []
    for storey in model.by_type("IfcBuildingStorey"):
        floors = [element for rel in storey.ContainsElements for element in rel.RelatedElements if element.is_a("IfcSlab")]
        if len(floors) != 1:
            raise SourceError(f"model.ifc IfcBuildingStorey {storey.GlobalId}: requires one floor")
        polygon, area = _polygon(floors[0], scale, top=True)
        storeys.append(dict(ifc_global_id=storey.GlobalId, name=storey.Name or "", floor_polygon=polygon, gross_floor_m2=area))
    rooms = []
    zones = {}
    for room in model.by_type("IfcSpace"):
        parents = [rel.RelatingObject for rel in room.Decomposes if rel.is_a("IfcRelAggregates") and rel.RelatingObject.is_a("IfcBuildingStorey")]
        if len(parents) != 1:
            raise SourceError(f"model.ifc IfcSpace {room.GlobalId}: missing unique storey")
        polygon, area = _polygon(room, scale)
        zone = _property(room, "OpenCEA_FacilityRoom", "Zone")
        if not isinstance(zone, str) or not zone:
            raise SourceError(f"model.ifc IfcSpace {room.GlobalId}: missing zone")
        rooms.append(dict(ifc_global_id=room.GlobalId, name=room.Name or "", storey_global_id=parents[0].GlobalId, zone=zone, polygon=polygon, area_m2=area, height_m=_envelope(room, scale)[2]))
        zones.setdefault(zone, []).append((room.GlobalId, area))
    equipment = []
    for row in elements:
        if row["product_id"] is None or row["state"] != "active":
            continue
        entity = model.by_guid(row["ifc_global_id"])
        frame = placement_matrix(entity.ObjectPlacement, length_scale=scale)
        position = transform_points(((0.0, 0.0, 0.0),), frame)[0]
        clearance = _property(entity, "OpenCEA_ServiceClearance", "Front")
        if clearance is None or not isfinite(float(clearance)) or float(clearance) < 0:
            raise SourceError(f"model.ifc {entity.is_a()} {entity.GlobalId}: invalid front service clearance")
        equipment.append(dict(source_uuid=row["id"], ifc_global_id=entity.GlobalId, type_uuid=row["product_id"], name=entity.Name or "", position=position, evidence_kind=row["evidence_kind"], frame=frame, envelope=_envelope(entity, scale), front_clearance_m=float(clearance) * scale))
    canopies = []
    for entity in model.by_type("IfcCovering"):
        polygon, area = _polygon(entity, scale, top=True)
        canopies.append(dict(ifc_global_id=entity.GlobalId, polygon=polygon, area_m2=area))
    peers = {}
    for relation in model.by_type("IfcRelConnectsPorts"):
        for left, right in ((relation.RelatingPort, relation.RelatedPort), (relation.RelatedPort, relation.RelatingPort)):
            if left.GlobalId in peers:
                raise SourceError(f"model.ifc IfcDistributionPort {left.GlobalId}: multiple peers")
            peers[left.GlobalId] = right.GlobalId
    ports = []
    for port in model.by_type("IfcDistributionPort"):
        row = crosswalk.get(port.GlobalId)
        hosts = [rel.RelatingObject for rel in port.Nests if rel.is_a("IfcRelNests")]
        if row is None or len(hosts) != 1 or port.GlobalId not in peers or peers[port.GlobalId] not in crosswalk:
            raise SourceError(f"model.ifc IfcDistributionPort {port.GlobalId}: missing identity/host/peer")
        frame = placement_matrix(port.ObjectPlacement, length_scale=scale)
        pos = transform_points(((0.0, 0.0, 0.0),), frame)[0]
        ports.append(dict(source_uuid=row["id"], ifc_global_id=port.GlobalId, host_global_id=hosts[0].GlobalId, peer_global_id=peers[port.GlobalId], system=port.SystemType or "", kind=port.PredefinedType or "", flow=port.FlowDirection or "", position=pos, frame=frame))
    return (tuple(sorted(storeys, key=lambda e: e["ifc_global_id"])),
            tuple(sorted(rooms, key=lambda e: e["ifc_global_id"])),
            tuple(dict(name=name, room_global_ids=tuple(sorted(g for g, _ in entries)), area_m2=sum(a for _, a in entries)) for name, entries in sorted(zones.items())),
            tuple(sorted(equipment, key=lambda e: e["source_uuid"])),
            tuple(sorted(canopies, key=lambda e: e["ifc_global_id"])),
            tuple(sorted(ports, key=lambda e: e["source_uuid"])))


def _check_file_diagnostics(root: Path, project_id: str, revision: str) -> None:
    """Name a corrupt accepted file before upstream verification reports storage failure."""
    folder = root / "projects" / parse_uuid(project_id) / "revisions" / validate_digest(revision)
    manifest_path = folder / "manifest.json"
    if any(path.is_symlink() for path in (root / "projects", folder.parent.parent, folder.parent, folder, manifest_path)):
        raise SourceError("manifest.json: symlink in accepted revision")
    if not manifest_path.is_file() or manifest_path.stat().st_size > 4 * 1024 * 1024:
        raise SourceError("manifest.json: missing or oversized")
    manifest = loads(manifest_path.read_bytes())
    for name in ("program.json", "model.ifc", "identity-map.json", "catalog.json"):
        if name not in manifest.get("files", {}):
            continue
        path = folder / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 4 * 1024 * 1024:
            raise SourceError(f"{name}: missing, symlink or oversized accepted file")
        if sha256(path.read_bytes()).hexdigest() != manifest["files"][name]:
            raise SourceError(f"{name}: accepted file SHA-256 differs from manifest.json")


def read_source(root: Path, project_id: str, revision: str) -> AcceptedSource:
    """Require an upstream-verified accepted revision with an actual design acceptance ancestor."""
    try:
        _check_file_diagnostics(Path(root), project_id, revision)
        head = LocalProjectStore(Path(root)).head(project_id)
        if head is None:
            raise SourceError("manifest.json: no accepted HEAD")
        current = None
        for record in revision_history(Path(root), project_id, head):
            if record[0]["revision"] == revision:
                current = record
                break
        if current is None:
            raise SourceError(f"manifest.json: {revision} is not in accepted design HEAD history")
        manifest, _, program_raw, artifacts = current
        history = revision_history(Path(root), project_id, revision)
        if set(artifacts) != {"model.ifc", "identity-map.json", "catalog.json"} or manifest["schema_version"] != "0.2":
            raise SourceError("manifest.json: accepted design artifacts required")
        design_revision = None
        decision = None
        for entry in chain((manifest,), (record[0] for record in history)):
            if entry["request"].get("operation") == "accept_design" and all(
                entry["files"].get(name) == manifest["files"][name] for name in artifacts
            ):
                design_revision = entry["revision"]
                decision = entry["request"]["decision"]
                break
        if design_revision is None:
            raise SourceError("manifest.json: accepted design receipt not found in verified history")
        program = loads(program_raw)
        identity = loads(artifacts["identity-map.json"])
        catalog = loads(artifacts["catalog.json"])
        validate_candidate(artifacts, program)
        model = ifcopenshell.file.from_string(artifacts["model.ifc"].decode("utf-8"))
        project = model.by_type("IfcProject")
        profile = _property(project[0], "OpenCEA_Facility", "Profile") if len(project) == 1 else None
        if profile != PROFILE:
            raise SourceError(f"model.ifc IfcProject: unsupported profile {profile!r}")
        scale = length_unit_scale(model)
        extracted = _extract(model, identity, scale)
        source_ids = set(identity["source_ids"]) | set(decision["source_ids"])
        for item in catalog["items"]:
            source_ids.update(item["source_ids"])
            for prop in item["properties"].values():
                source_ids.update(prop.get("source_ids", ()))
        sources = [s for s in program["sources"] if s["id"] in source_ids]
        if len(sources) != len(source_ids):
            raise SourceError("program.json: accepted geometry, reviewer or catalog evidence source missing")
        if any(row["asset_id"] is not None and row["evidence_kind"] == "proposed" for row in identity["elements"]):
            raise SourceError("identity-map.json: proposed installation carries asset_id")
        return AcceptedSource(project_id=project_id, revision=revision, accepted_design_revision=design_revision,
                              files=dict(sorted(manifest["files"].items())), profile=profile,
                              valid_from="", valid_until="",
                              decision=decision, identity=identity, catalog=catalog,
                              sources=tuple(sorted(sources, key=lambda s: s["id"])),
                              storeys=extracted[0], rooms=extracted[1], zones=extracted[2],
                              equipment=extracted[3], canopies=extracted[4], ports=extracted[5])
    except SourceError:
        raise
    except (ContractError, StopIteration, OSError, UnicodeError, ValueError, KeyError, TypeError, RuntimeError) as exc:
        raise SourceError(f"accepted design {project_id}/{revision}: {exc}") from exc
