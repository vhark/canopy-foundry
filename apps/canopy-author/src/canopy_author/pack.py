"""Stable game-only FlatBuffers compiler; IFC/source records never enter the payload."""
from __future__ import annotations

from hashlib import sha256
from math import isfinite

import flatbuffers

from .pack_validation import validate
from .source import AcceptedSource

MAX_BYTES = 256 * 1024


def _string(builder, value):
    if not isinstance(value, str) or len(value.encode("utf-8")) > 4096:
        raise ValueError("pack string exceeds capacity")
    return builder.CreateString(value)


def _vector(builder, values):
    builder.StartVector(4, len(values), 4)
    for value in reversed(values):
        builder.PrependUOffsetTRelative(value)
    return builder.EndVector()


def _table(builder, types, values):
    """Schema order is explicit: slot index equals the declared .fbs field index."""
    builder.StartObject(len(types))
    for index in reversed(range(len(types))):
        kind, value = types[index], values[index]
        if kind == "offset":
            builder.PrependUOffsetTRelativeSlot(index, value, 0)
        elif kind == "double":
            builder.PrependFloat64Slot(index, value, 0.0)
        elif kind == "uint":
            builder.PrependUint32Slot(index, value, 0)
        elif kind == "bool":
            builder.PrependBoolSlot(index, value, False)
    return builder.EndObject()


def _strings(builder, values):
    return _vector(builder, [_string(builder, item) for item in values])


def _point(builder, xyz):
    if len(xyz) != 3 or not all(isinstance(x, (int, float)) and isfinite(x) and abs(x) <= 1_000_000 for x in xyz):
        raise ValueError("pack point is nonfinite or outside coordinate bounds")
    return _table(builder, ("double",) * 3, xyz)


def _polygon(builder, points):
    if not 3 <= len(points) <= 64:
        raise ValueError("pack polygon vertex capacity exceeded")
    return _vector(builder, [_point(builder, point) for point in points])


def _frame(builder, frame):
    return _table(builder, ("double",) * 16, tuple(value for row in frame for value in row))


def _envelope(builder, xyz):
    return _table(builder, ("double",) * 3, xyz)


def _rows(builder, rows, fields):
    def one(row):
        values = []
        types = []
        for name, kind in fields:
            value = row[name]
            if kind == "string":
                value = _string(builder, value)
                kind = "offset"
            elif kind == "strings":
                value = _strings(builder, value)
                kind = "offset"
            elif kind == "point":
                value = _point(builder, value)
                kind = "offset"
            elif kind == "polygon":
                value = _polygon(builder, value)
                kind = "offset"
            elif kind == "frame":
                value = _frame(builder, value)
                kind = "offset"
            elif kind == "envelope":
                value = _envelope(builder, value)
                kind = "offset"
            elif kind == "double" and (not isinstance(value, (int, float)) or not isfinite(value) or value < 0 or value > 1_000_000):
                raise ValueError(f"pack invalid area {name}")
            values.append(value)
            types.append(kind)
        return _table(builder, types, values)
    return _vector(builder, [one(row) for row in rows])



def compile_pack(source: AcceptedSource) -> bytes:
    """Compile a validated accepted source; do not mint campaign instances."""
    validate(source)
    ids = source.identity["elements"]
    builder = flatbuffers.Builder(4096)
    files = _rows(builder, [dict(name=k, sha256=v) for k, v in sorted(source.files.items())], (("name", "string"), ("sha256", "string")))
    identity = _rows(builder, [dict(source_uuid=e["id"], ifc_global_id=e["ifc_global_id"], ifc_class=e["ifc_class"], evidence_kind=e["evidence_kind"], state=e["state"]) for e in sorted(ids, key=lambda r: r["id"])], tuple((k, "string") for k in ("source_uuid", "ifc_global_id", "ifc_class", "evidence_kind", "state")))
    storeys = _rows(builder, sorted(source.storeys, key=lambda r: r["ifc_global_id"]), (("ifc_global_id", "string"), ("name", "string"), ("floor_polygon", "polygon"), ("gross_floor_m2", "double")))
    rooms = _rows(builder, sorted(source.rooms, key=lambda r: r["ifc_global_id"]), (("ifc_global_id", "string"), ("name", "string"), ("storey_global_id", "string"), ("zone", "string"), ("polygon", "polygon"), ("area_m2", "double"), ("height_m", "double")))
    zones = _rows(builder, [row | {"room_global_ids": sorted(row["room_global_ids"])} for row in sorted(source.zones, key=lambda r: r["name"])], (("name", "string"), ("room_global_ids", "strings"), ("area_m2", "double")))
    equipment = _rows(builder, sorted(source.equipment, key=lambda r: r["source_uuid"]), (("source_uuid", "string"), ("ifc_global_id", "string"), ("type_uuid", "string"), ("name", "string"), ("position", "point"), ("evidence_kind", "string"), ("frame", "frame"), ("envelope", "envelope"), ("front_clearance_m", "double")))
    canopies = _rows(builder, sorted(source.canopies, key=lambda r: r["ifc_global_id"]), (("ifc_global_id", "string"), ("polygon", "polygon"), ("area_m2", "double")))
    ports = _rows(builder, sorted(source.ports, key=lambda r: r["source_uuid"]), (("source_uuid", "string"), ("ifc_global_id", "string"), ("host_global_id", "string"), ("peer_global_id", "string"), ("system", "string"), ("kind", "string"), ("flow", "string"), ("position", "point"), ("frame", "frame")))
    catalog = _rows(builder, [dict(type_uuid=e["id"], name=e["name"], evidence_status=e["evidence_status"], source_ids=sorted(e["source_ids"])) for e in sorted(source.catalog["items"], key=lambda row: row["id"])], (("type_uuid", "string"), ("name", "string"), ("evidence_status", "string"), ("source_ids", "strings")))
    provenance = _rows(builder, [dict(source_uuid=e["id"], author=e["author"], recorded_at=e["recorded_at"], description=e["description"]) for e in sorted(source.sources, key=lambda row: row["id"])], (("source_uuid", "string"), ("author", "string"), ("recorded_at", "string"), ("description", "string")))
    vals = [1, _string(builder, source.project_id), _string(builder, source.revision),
            _string(builder, source.accepted_design_revision), _string(builder, source.files["model.ifc"]),
            _string(builder, source.profile), _string(builder, source.valid_from), _string(builder, source.valid_until),
            _string(builder, source.decision["reviewer"]), _string(builder, source.decision["reason"]),
            _strings(builder, sorted(source.decision["source_ids"])), files, identity, storeys, rooms, zones,
            equipment, canopies, ports, catalog, provenance, False]
    pack = _table(builder, ("uint",) + ("offset",) * 20 + ("bool",), vals)
    builder.Finish(pack, file_identifier=b"CFP2")
    binary = bytes(builder.Output())
    if len(binary) > MAX_BYTES:
        raise ValueError("pack byte capacity exceeded")
    return binary


def pack_sha256(binary: bytes) -> str:
    return sha256(binary).hexdigest()
