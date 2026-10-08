"""Bounded checks for the game-only semantic projection before FlatBuffer allocation.

Upstream validators establish accepted IFC authority; these checks protect the smaller
projection and its reference graph, not reinterpret upstream acceptance contracts.
"""
from math import isfinite
from uuid import UUID

from .source import AcceptedSource, MAX_ELEMENTS, MAX_PORTS, MAX_ROOMS, PROFILE


def uuid(value, label):
    try:
        if str(UUID(value)) != value:
            raise ValueError()
    except (TypeError, ValueError, AttributeError) as exc:
        raise ValueError(f"pack invalid {label} UUID") from exc


def digest(value, label):
    if not isinstance(value, str) or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise ValueError(f"pack invalid {label} digest")


def text(value, label):
    if not isinstance(value, str) or len(value.encode("utf-8")) > 4096:
        raise ValueError(f"pack {label} string capacity exceeded")


def unique(rows, key, label):
    values = [row[key] for row in rows]
    if len(values) != len(set(values)):
        raise ValueError(f"pack duplicate {label}")
    return set(values)


def number(value, label, minimum=0, maximum=1_000_000):
    if not isinstance(value, (int, float)) or not isfinite(value) or not minimum <= value < maximum:
        raise ValueError(f"pack invalid {label}")


def point(values, label):
    if len(values) != 3:
        raise ValueError(f"pack {label} requires three coordinates")
    for value in values:
        number(value, label, -1_000_000, 1_000_001)


def polygon(row, key):
    points = row[key]
    if not 3 <= len(points) <= 64:
        raise ValueError(f"pack {key} polygon vertex capacity exceeded")
    for vertex in points:
        point(vertex, key)


def frame(row):
    matrix = row["frame"]
    if len(matrix) != 4 or any(len(values) != 4 for values in matrix):
        raise ValueError("pack frame must be 4x4")
    for values in matrix:
        for value in values:
            number(value, "frame", -1_000_000, 1_000_001)
    if tuple(matrix[3]) != (0, 0, 0, 1):
        raise ValueError("pack frame is not affine")
    point(row["position"], "position")
    if any(abs(matrix[i][3] - row["position"][i]) > 1e-8 for i in range(3)):
        raise ValueError("pack frame origin differs from position")
    axes = tuple(tuple(matrix[r][c] for r in range(3)) for c in range(3))
    for i in range(3):
        for j in range(3):
            if abs(sum(axes[i][k] * axes[j][k] for k in range(3)) - (1.0 if i == j else 0.0)) > 1e-6:
                raise ValueError("pack frame is not rigid")
    a, b, c = axes
    determinant = a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
    if abs(determinant - 1) > 1e-6:
        raise ValueError("pack frame orientation invalid")


def validate(source: AcceptedSource):
    if not isinstance(source, AcceptedSource) or source.profile != PROFILE:
        raise ValueError("pack unsupported source profile")
    uuid(source.project_id, "project")
    digest(source.revision, "revision")
    digest(source.accepted_design_revision, "accepted design revision")
    if set(source.files) != {"program.json", "model.ifc", "identity-map.json", "catalog.json"}:
        raise ValueError("pack incomplete file digest set")
    for name, value in source.files.items():
        digest(value, name)
    for field in ("profile", "valid_from", "valid_until"):
        text(getattr(source, field), field)
    for field in ("reviewer", "reason"):
        text(source.decision[field], field)
    collections = ((source.identity["elements"], MAX_ELEMENTS, "identity"),
                   (source.catalog["items"], MAX_ELEMENTS, "catalog"),
                   (source.sources, MAX_ELEMENTS, "provenance"),
                   (source.storeys, 64, "storey"), (source.rooms, MAX_ROOMS, "room"),
                   (source.zones, MAX_ROOMS, "zone"),
                   (source.equipment, MAX_ELEMENTS, "equipment"),
                   (source.canopies, MAX_ELEMENTS, "canopy"),
                   (source.ports, MAX_PORTS, "port"))
    for rows, limit, label in collections:
        if len(rows) > limit:
            raise ValueError(f"pack {label} capacity exceeded")
    identities = source.identity["elements"]
    identity_ids = unique(identities, "id", "identity")
    identity_guids = unique(identities, "ifc_global_id", "IFC GlobalId")
    by_guid = {row["ifc_global_id"]: row for row in identities}
    for row in identities:
        uuid(row["id"], "source identity")
        text(row["ifc_global_id"], "IFC GlobalId")
        text(row["ifc_class"], "IFC class")
        if row["evidence_kind"] not in ("proposed", "reported") or row["state"] not in ("active", "retired"):
            raise ValueError("pack unknown identity evidence/state")
        if row["asset_id"] is not None and row["evidence_kind"] == "proposed":
            raise ValueError("pack proposed element cannot claim installed asset")
    catalog_ids = unique(source.catalog["items"], "id", "catalog type")
    provenance_ids = unique(source.sources, "id", "provenance source")
    for row in source.sources:
        uuid(row["id"], "provenance source")
        for key in ("author", "recorded_at", "description"):
            text(row[key], "provenance " + key)
    if len(source.identity["source_ids"]) > MAX_ELEMENTS:
        raise ValueError("pack geometry provenance capacity exceeded")
    references = set(source.decision["source_ids"]) | set(source.identity["source_ids"])
    if len(source.decision["source_ids"]) > MAX_ELEMENTS or len(source.decision["source_ids"]) != len(set(source.decision["source_ids"])):
        raise ValueError("pack duplicate or unbounded decision sources")
    for ref in references:
        uuid(ref, "source evidence")
    for row in source.catalog["items"]:
        uuid(row["id"], "catalog type")
        text(row["name"], "catalog name")
        if row["schema_version"] != "0.1":
            raise ValueError("pack unknown product-row schema")
        if row["evidence_status"] not in ("unknown", "declared", "independently_tested"):
            raise ValueError("pack unknown catalog evidence")
        if len(row["source_ids"]) > MAX_ELEMENTS:
            raise ValueError("pack catalog source capacity exceeded")
        for ref in row["source_ids"]:
            uuid(ref, "catalog source")
        references.update(row["source_ids"])
        for prop in row["properties"].values():
            references.update(prop.get("source_ids", ()))
    if not references <= provenance_ids:
        raise ValueError("pack missing provenance source reference")
    storey_ids = unique(source.storeys, "ifc_global_id", "storey")
    room_ids = unique(source.rooms, "ifc_global_id", "room")
    unique(source.zones, "name", "zone")
    unique(source.equipment, "source_uuid", "equipment")
    unique(source.canopies, "ifc_global_id", "canopy")
    port_ids = unique(source.ports, "ifc_global_id", "port")
    unique(source.ports, "source_uuid", "port source")
    for storey in source.storeys:
        text(storey["ifc_global_id"], "storey IFC GlobalId")
        text(storey["name"], "storey name")
        polygon(storey, "floor_polygon")
        number(storey["gross_floor_m2"], "gross floor area", 0.000001)
    for room in source.rooms:
        text(room["ifc_global_id"], "room IFC GlobalId")
        text(room["storey_global_id"], "room storey")
        text(room["name"], "room name")
        text(room["zone"], "zone name")
        polygon(room, "polygon")
        if room["storey_global_id"] not in storey_ids:
            raise ValueError("pack room storey reference missing")
        number(room["area_m2"], "room area", 0.000001)
        number(room["height_m"], "room height", 0.000001, 10000)
    for zone in source.zones:
        text(zone["name"], "zone name")
        for ref in zone["room_global_ids"]:
            text(ref, "zone room IFC GlobalId")
        room_refs = zone["room_global_ids"]
        if len(room_refs) > MAX_ROOMS or len(room_refs) != len(set(room_refs)) or not set(room_refs) <= room_ids:
            raise ValueError("pack zone room reference invalid")
        if set(room_refs) != {r["ifc_global_id"] for r in source.rooms if r["zone"] == zone["name"]}:
            raise ValueError("pack zone membership invalid")
        number(zone["area_m2"], "zone area", 0.000001)
    if {row["name"] for row in source.zones} != {row["zone"] for row in source.rooms}:
        raise ValueError("pack zone coverage incomplete")
    for row in source.equipment:
        uuid(row["source_uuid"], "equipment")
        text(row["ifc_global_id"], "equipment IFC GlobalId")
        uuid(row["type_uuid"], "equipment type")
        if row["type_uuid"] not in catalog_ids:
            raise ValueError("pack equipment type reference missing")
        if row["source_uuid"] not in identity_ids or by_guid.get(row["ifc_global_id"], {}).get("id") != row["source_uuid"]:
            raise ValueError("pack equipment identity reference invalid")
        if by_guid[row["ifc_global_id"]]["product_id"] != row["type_uuid"]:
            raise ValueError("pack equipment type differs from identity")
        if row["evidence_kind"] != by_guid[row["ifc_global_id"]]["evidence_kind"]:
            raise ValueError("pack equipment evidence differs from identity")
        text(row["name"], "equipment name")
        frame(row)
        if len(row["envelope"]) != 3:
            raise ValueError("pack equipment envelope requires three dimensions")
        for dimension in row["envelope"]:
            number(dimension, "equipment envelope", 0.000001, 10000)
        number(row["front_clearance_m"], "equipment clearance", 0, 10000)
    for canopy in source.canopies:
        text(canopy["ifc_global_id"], "canopy IFC GlobalId")
        polygon(canopy, "polygon")
        number(canopy["area_m2"], "canopy area", 0.000001)
        if canopy["ifc_global_id"] not in identity_guids:
            raise ValueError("pack canopy identity reference missing")
    ports_by_guid = {row["ifc_global_id"]: row for row in source.ports}
    for row in source.ports:
        uuid(row["source_uuid"], "port")
        for key in ("ifc_global_id", "host_global_id", "peer_global_id"):
            text(row[key], "port " + key)
        if by_guid.get(row["ifc_global_id"], {}).get("id") != row["source_uuid"]:
            raise ValueError("pack port identity reference missing")
        if row["host_global_id"] not in identity_guids:
            raise ValueError("pack port host reference missing")
        if row["peer_global_id"] not in port_ids or row["peer_global_id"] == row["ifc_global_id"]:
            raise ValueError("pack port peer reference missing")
        if row["system"] not in ("AIRCONDITIONING", "WATERSUPPLY", "ELECTRICAL") or row["kind"] not in ("DUCT", "PIPE", "CABLE") or row["flow"] not in ("SOURCE", "SINK"):
            raise ValueError("pack port system/kind/flow enum unsupported")
        if (row["system"], row["kind"]) not in (("AIRCONDITIONING", "DUCT"), ("WATERSUPPLY", "PIPE"), ("ELECTRICAL", "CABLE")):
            raise ValueError("pack port system/kind pairing invalid")
        frame(row)
    for port in source.ports:
        peer = ports_by_guid[port["peer_global_id"]]
        if peer["peer_global_id"] != port["ifc_global_id"] or peer["system"] != port["system"] or peer["kind"] != port["kind"] or peer["flow"] == port["flow"]:
            raise ValueError("pack port peer topology incompatible")
