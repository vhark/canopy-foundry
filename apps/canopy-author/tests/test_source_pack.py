"""Real upstream acceptance, rather than a renamed IFC candidate, drives compilation."""
import hashlib
import json
from pathlib import Path
from uuid import UUID, uuid4

import pytest
from growbim.ifc import build_facility
from growbim.project.store import LocalProjectStore

from canopy_author.pack import compile_pack
from canopy_author.source import SourceError, read_source


def uid():
    return str(uuid4())


@pytest.fixture
def accepted(tmp_path):
    store = LocalProjectStore(tmp_path)
    created = store.create("Facility", uid())
    source_id = uid()
    program = created["program"] | {"sources": [{
        "id": source_id, "description": "Original illustrated facility geometry",
        "author": "Canopy author regression", "recorded_at": "2026-09-28T00:00:00Z",
    }]}
    sourced = store.accept(created["project_id"], created["revision"], uid(), program)
    artifacts = build_facility(program, source_id)
    accepted_revision = store.accept_design(created["project_id"], sourced["revision"], uid(), artifacts, {
        "reviewer": "Regression reviewer", "reason": "Original illustrative fixture", "source_ids": [source_id],
    })["revision"]
    return store.root, created["project_id"], accepted_revision, artifacts


def test_real_acceptance_preserves_exact_bytes_and_floor_not_canopy(accepted):
    root, project, revision, artifacts = accepted
    source = read_source(root, project, revision)
    assert source.revision == revision
    assert source.files["model.ifc"] == hashlib.sha256(artifacts["model.ifc"]).hexdigest()
    assert source.files["identity-map.json"] == hashlib.sha256(artifacts["identity-map.json"]).hexdigest()
    assert sum(s["gross_floor_m2"] for s in source.storeys) == pytest.approx(216)
    assert sum(r["area_m2"] for r in source.rooms) == pytest.approx(216)
    assert sum(c["area_m2"] for c in source.canopies) == pytest.approx(33.6)
    assert len(source.rooms) == 8
    assert len(source.ports) == 6
    assert len(source.equipment) == 3
    assert all(UUID(e["type_uuid"]) for e in source.equipment)
    assert all(row["asset_id"] is None for row in source.identity["elements"])
    assert all(p["peer_global_id"] != p["ifc_global_id"] for p in source.ports)
    assert sum(z["area_m2"] for z in source.zones if z["name"] == "production") == pytest.approx(72)

def test_real_equipment_bounds_room_height_and_port_frames(accepted):
    root, project, revision, _ = accepted
    source = read_source(root, project, revision)
    assert {room["height_m"] for room in source.rooms} == {3.0}
    fan = next(item for item in source.equipment if "fan" in item["name"])
    assert fan["envelope"] == pytest.approx((1, 0.8, 1.5))
    assert fan["front_clearance_m"] == pytest.approx(0.6)
    assert fan["frame"][0][3] == pytest.approx(fan["position"][0])
    assert len(source.ports[0]["frame"]) == 4
    assert source.ports[0]["frame"][3] == (0.0, 0.0, 0.0, 1.0)
    assert source.valid_from == source.valid_until == ""


def test_independent_review_and_catalog_evidence_sources_are_retained(tmp_path):
    from opencea.contracts import dumps, loads

    store = LocalProjectStore(tmp_path)
    created = store.create("Source union", uid())
    original, review, data = uid(), uid(), uid()
    sources = [
        dict(id=key, description=label, author="Original review fixture", recorded_at="2026-09-28T00:00:00Z")
        for key, label in ((original, "geometry"), (review, "independent review"), (data, "catalog data"))
    ]
    program = created["program"] | {"sources": sources}
    sourced = store.accept(created["project_id"], created["revision"], uid(), program)
    artifacts = build_facility(program, original)
    catalog = loads(artifacts["catalog.json"])
    catalog["items"][0]["source_ids"].append(data)
    artifacts["catalog.json"] = dumps(catalog)
    accepted = store.accept_design(created["project_id"], sourced["revision"], uid(), artifacts, {
        "reviewer": "Independent reviewer", "reason": "Reviewed geometry and data",
        "source_ids": [original, review],
    })
    source = read_source(store.root, created["project_id"], accepted["revision"])
    assert {row["id"] for row in source.sources} == {original, review, data}
    assert set(source.decision["source_ids"]) == {original, review}
    assert compile_pack(source)[4:8] == b"CFP2"


def test_accepted_rotated_connector_pair_preserves_full_frame(tmp_path):
    import ifcopenshell
    from opencea.contracts import dumps, loads

    store = LocalProjectStore(tmp_path)
    created = store.create("Rotated ports", uid())
    source_id = uid()
    program = created["program"] | {"sources": [{
        "id": source_id, "description": "Rotated original ports",
        "author": "Canopy original review", "recorded_at": "2026-09-28T00:00:00Z",
    }]}
    sourced = store.accept(created["project_id"], created["revision"], uid(), program)
    artifacts = build_facility(program, source_id)
    model = ifcopenshell.file.from_string(artifacts["model.ifc"].decode())
    air = next(rel for rel in model.by_type("IfcRelConnectsPorts") if rel.RelatingPort.SystemType == "AIRCONDITIONING")
    for port in (air.RelatingPort, air.RelatedPort):
        port.ObjectPlacement.RelativePlacement.RefDirection.DirectionRatios = (0.0, 1.0, 0.0)
    model_raw = model.to_string().encode()
    identity = loads(artifacts["identity-map.json"])
    identity["model_sha256"] = hashlib.sha256(model_raw).hexdigest()
    artifacts = artifacts | {"model.ifc": model_raw, "identity-map.json": dumps(identity)}
    accepted = store.accept_design(created["project_id"], sourced["revision"], uid(), artifacts, {
        "reviewer": "Port orientation reviewer", "reason": "Reviewed aligned rigid port pair",
        "source_ids": [source_id],
    })
    source = read_source(store.root, created["project_id"], accepted["revision"])
    air_ports = [port for port in source.ports if port["system"] == "AIRCONDITIONING"]
    assert len(air_ports) == 2
    assert all(port["frame"][0][0] == pytest.approx(0) and port["frame"][1][0] == pytest.approx(1) for port in air_ports)
    assert compile_pack(source)[4:8] == b"CFP2"


def test_pack_is_flatbuffer_deterministic_and_not_raw_source(accepted):
    root, project, revision, artifacts = accepted
    source = read_source(root, project, revision)
    binary = compile_pack(source)
    assert binary[:4] != b"{\"sc"
    assert binary[4:8] == b"CFP2"
    assert compile_pack(read_source(root, project, revision)) == binary
    assert revision.encode() in binary
    assert artifacts["model.ifc"][:128] not in binary
    assert b"instance_id" not in binary
    assert b"asset_id" not in binary
    assert len(binary) < 256 * 1024


def test_flatbuffer_reader_sees_revision_digest_and_real_room_area(accepted):
    import flatbuffers
    from flatbuffers.table import Table

    root, project, revision, _ = accepted
    source = read_source(root, project, revision)
    binary = compile_pack(source)
    buf = bytearray(binary)
    facility = Table(buf, flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, 0))
    assert facility.String(facility.Pos + facility.Offset(8)).decode() == revision
    assert facility.String(facility.Pos + facility.Offset(12)).decode() == source.files["model.ifc"]
    rooms_offset = facility.Offset(32)
    assert facility.VectorLen(rooms_offset) == 8
    first_room_offset = facility.Vector(rooms_offset)
    room = Table(buf, facility.Indirect(first_room_offset))
    assert room.Get(flatbuffers.number_types.Float64Flags, room.Pos + room.Offset(14)) == pytest.approx(source.rooms[0]["area_m2"])


def test_proposal_or_unaccepted_revision_rejected(accepted):
    root, project, revision, _ = accepted
    from growbim.project.revisions import revision_history
    history = list(revision_history(root, project, revision))
    proposal = history[0][0]["parent_revision"]
    with pytest.raises(SourceError, match="accepted design"):
        read_source(root, project, proposal)


def test_tampered_model_hash_rejected_with_filename(accepted):
    root, project, revision, _ = accepted
    path = Path(root) / "projects" / project / "revisions" / revision / "model.ifc"
    path.write_bytes(path.read_bytes() + b"tampered")
    with pytest.raises(SourceError, match="model.ifc"):
        read_source(root, project, revision)


def test_tampered_identity_rejected_with_filename(accepted):
    root, project, revision, _ = accepted
    path = Path(root) / "projects" / project / "revisions" / revision / "identity-map.json"
    identity = json.loads(path.read_bytes())
    identity["elements"][0]["asset_id"] = uid()
    path.write_text(json.dumps(identity))
    with pytest.raises(SourceError, match="identity-map.json"):
        read_source(root, project, revision)


def test_unknown_or_malformed_pack_input_rejected(accepted):
    root, project, revision, _ = accepted
    source = read_source(root, project, revision)
    from dataclasses import replace
    with pytest.raises(ValueError, match="profile"):
        compile_pack(replace(source, profile="unsupported"))
    with pytest.raises(ValueError, match="digest"):
        compile_pack(replace(source, files={"model.ifc": "bad"}))
    forged_identity = source.identity | {"elements": [
        source.identity["elements"][0] | {"asset_id": uid()},
        *source.identity["elements"][1:],
    ]}
    with pytest.raises(ValueError, match="proposed"):
        compile_pack(replace(source, identity=forged_identity))


@pytest.mark.parametrize(("field", "mutation", "diagnostic"), [
    ("ports", lambda row: row | {"peer_global_id": "missing"}, "peer"),
    ("ports", lambda row: row | {"host_global_id": "missing"}, "host"),
    ("equipment", lambda row: row | {"type_uuid": uid()}, "type"),
    ("rooms", lambda row: row | {"storey_global_id": "missing"}, "storey"),
    ("ports", lambda row: row | {"kind": "UNKNOWN"}, "kind"),
    ("ports", lambda row: row | {"frame": ((1, 0, 0, 0),) * 4}, "frame"),
])
def test_pack_rejects_broken_references_and_frames(accepted, field, mutation, diagnostic):
    from dataclasses import replace

    root, project, revision, _ = accepted
    source = read_source(root, project, revision)
    records = getattr(source, field)
    with pytest.raises(ValueError, match=diagnostic):
        compile_pack(replace(source, **{field: (mutation(records[0]), *records[1:])}))


def test_pack_rejects_duplicate_identity_and_unbounded_provenance(accepted):
    from dataclasses import replace

    root, project, revision, _ = accepted
    source = read_source(root, project, revision)
    with pytest.raises(ValueError, match="duplicate"):
        compile_pack(replace(source, identity=source.identity | {"elements": [
            *source.identity["elements"], source.identity["elements"][0],
        ]}))
    with pytest.raises(ValueError, match="capacity"):
        compile_pack(replace(source, sources=source.sources * 4100))


def test_inherited_accepted_artifacts_retain_original_review(accepted):
    root, project, revision, _ = accepted
    store = LocalProjectStore(root)
    later = store.accept(project, revision, uid(), store.inspect(project)["program"] | {"name": "Later concept"})["revision"]
    source = read_source(root, project, later)
    assert source.accepted_design_revision == revision
    assert source.decision["reviewer"] == "Regression reviewer"
    assert source.files["model.ifc"] == read_source(root, project, revision).files["model.ifc"]


def test_compilation_stops_before_unrelated_history_archive_limit(accepted, monkeypatch):
    from growbim.project import revisions

    root, project, revision, _ = accepted
    folder = Path(root) / "projects" / project / "revisions" / revision
    current_size = sum((folder / name).stat().st_size for name in (
        "manifest.json", "program.json", "model.ifc", "identity-map.json", "catalog.json",
    ))
    monkeypatch.setattr(revisions, "MAX_ARCHIVE", current_size + 1)
    assert read_source(root, project, revision).accepted_design_revision == revision
