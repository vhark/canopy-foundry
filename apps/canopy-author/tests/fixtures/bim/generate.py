"""Generate original accepted First Shipment fixture using upstream project review APIs.

Run with: python apps/canopy-author/tests/fixtures/bim/generate.py --root .work/b02-original
The output is an actual accepted revision, never a relabeled candidate.
"""
import argparse
import json
from pathlib import Path
from uuid import NAMESPACE_URL, uuid5

from growbim.ifc import build_facility
from growbim.project.store import LocalProjectStore


def id_for(kind):
    return str(uuid5(NAMESPACE_URL, "canopy-b02-original-fixture/" + kind))


def generate(root):
    store = LocalProjectStore(root)
    created = store.create("Original First Shipment", id_for("create"), id_for("project"))
    source = id_for("source")
    program = created["program"] | {"sources": [{
        "id": source, "description": "Original illustrated First Shipment facility",
        "author": "Canopy Foundry", "recorded_at": "2026-09-28T00:00:00Z",
    }]}
    sourced = store.accept(created["project_id"], created["revision"], id_for("program"), program)
    artifacts = build_facility(program, source)
    result = store.accept_design(created["project_id"], sourced["revision"], id_for("design"), artifacts, {
        "reviewer": "Original fixture review", "reason": "Review original illustrative design",
        "source_ids": [source],
    })
    return {"root": str(root), "project_id": created["project_id"], "revision": result["revision"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    print(json.dumps(generate(parser.parse_args().root), sort_keys=True))
