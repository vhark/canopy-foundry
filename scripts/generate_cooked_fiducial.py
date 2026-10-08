"""Generate a private, original RH Y-up metre glTF 2.0 fixture for the UE cook gate.

Run in the pinned canopy-author environment from the repository root. The JSON sidecar
records the three basis stages; it is an import input, never the runtime oracle.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from canopy_author import coordinates

# Parent is translated and rotated +90 degrees; child is translated and mirrored X.
# Both transformations are in semantic RH Z-up metres, BEFORE GLB export.
FRAME = ((0., -1., 0., 1.6), (-1., 0., 0., 1.15), (0., 0., 1., .55), (0., 0., 0., 1.))
LOCAL = ((0., 0., 0.), (.8, 0., 0.), (0., .45, 0.), (0., 0., .3))
FACES = ((0, 2, 1), (0, 1, 3), (0, 3, 2), (1, 2, 3))
PORTS = (
    ("Supply", (.8, .08, .10), (1., 0., 0.), ((0., -.04, -.04), (0., .04, -.04), (0., 0., .08))),
    ("Return", (.12, .45, .2), (0., 1., 0.), ((-.04, 0., -.04), (.04, 0., -.04), (0., 0., .08))),
)


def _triangle(a, b, c, desired=None):
    u = tuple(b[i] - a[i] for i in range(3))
    v = tuple(c[i] - a[i] for i in range(3))
    n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    magnitude = math.sqrt(sum(t*t for t in n))
    if not magnitude:
        raise ValueError("Degenerate fiducial face")
    normal = tuple(t/magnitude for t in n)
    if desired and sum(normal[i]*desired[i] for i in range(3)) < 0:
        return _triangle(a, c, b, desired)
    return (a, b, c), normal


def _mesh(include_ports):
    vertices, normals, indices = [], [], []
    interior = (.12, .09, .06)
    for face in FACES:
        a, b, c = (LOCAL[i] for i in face)
        center = tuple((a[i]+b[i]+c[i])/3 for i in range(3))
        outward = tuple(center[i]-interior[i] for i in range(3))
        points, normal = _triangle(a, b, c, outward)
        vertices.extend(points)
        normals.extend((normal,)*3)
        indices.extend(range(len(vertices)-3, len(vertices)))
    if include_ports:
        for _, center, direction, offsets in PORTS:
            points, normal = _triangle(*(tuple(center[i]+offset[i] for i in range(3)) for offset in offsets), desired=direction)
            vertices.extend(points)
            normals.extend((normal,)*3)
            indices.extend(range(len(vertices)-3, len(vertices)))
    faces = tuple(tuple(indices[i:i+3]) for i in range(0, len(indices), 3))
    semantic = coordinates.transform_mesh(vertices, faces, normals, matrix=FRAME)
    exported = coordinates.transform_mesh(semantic.vertices, semantic.triangles, semantic.normals,
                                           matrix=coordinates.SEMANTIC_TO_GLB)
    return exported


def generate(destination: Path):
    destination.mkdir(parents=True, exist_ok=True)
    binary = bytearray()
    views, accessors, meshes = [], [], []

    def accessor(raw, target, count, kind, component, minimum=None, maximum=None):
        offset = len(binary)
        binary.extend(raw)
        binary.extend(b"\0"*((-len(binary)) % 4))
        view = len(views)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(raw), "target": target})
        item = {"bufferView": view, "componentType": component, "count": count, "type": kind}
        if minimum is not None:
            item.update(min=minimum, max=maximum)
        accessors.append(item)
        return len(accessors)-1

    for name, include_ports in (("Fiducial", True), ("UCX_Fiducial_00", False)):
        mesh = _mesh(include_ports)
        verts = mesh.vertices
        positions = accessor(b"".join(struct.pack("<3f", *v) for v in verts), 34962, len(verts), "VEC3", 5126,
                             [min(v[i] for v in verts) for i in range(3)],
                             [max(v[i] for v in verts) for i in range(3)])
        normals = accessor(b"".join(struct.pack("<3f", *n) for n in mesh.normals), 34962, len(verts), "VEC3", 5126)
        indexes = [index for tri in mesh.triangles for index in tri]
        faces = accessor(struct.pack("<"+"H"*len(indexes), *indexes), 34963, len(indexes), "SCALAR", 5123)
        meshes.append({"name": name, "primitives": [{"attributes": {"POSITION": positions, "NORMAL": normals}, "indices": faces, "mode": 4}]})
    gltf = {"asset": {"version": "2.0", "generator": "Canopy Foundry original cooked fiducial"},
            "scene": 0, "scenes": [{"nodes": [0, 1]}],
            "nodes": [{"name": "Fiducial", "mesh": 0}, {"name": "UCX_Fiducial_00", "mesh": 1}],
            "meshes": meshes, "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(binary)}]}
    encoded = json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode("utf-8")
    encoded += b" "*((-len(encoded)) % 4)
    body = bytes(binary)
    glb = struct.pack("<4sII", b"glTF", 2, 12+8+len(encoded)+8+len(body))
    glb += struct.pack("<I4s", len(encoded), b"JSON")+encoded
    glb += struct.pack("<I4s", len(body), b"BIN\0")+body
    (destination / "CookedFiducial.glb").write_bytes(glb)

    def port_data(port):
        name, local, direction, _ = port
        semantic = coordinates.transform_points((local,), FRAME)[0]
        glb_point = coordinates.semantic_to_glb((semantic,))[0]
        semantic_normal = tuple(sum(FRAME[i][j]*direction[j] for j in range(3)) for i in range(3))
        return {"name": name, "semantic_m": semantic, "export_glb_m": glb_point,
                "semantic_normal": semantic_normal,
                "export_glb_normal": coordinates.semantic_to_glb((semantic_normal,))[0]}

    provenance = {"format": "canopy-cooked-fiducial-1", "glb_sha256": hashlib.sha256(glb).hexdigest(),
                  "source_basis": "RH Z-up metres; parent T(1.7,.9,.4) Rz(+90deg), child T(.25,.1,.15) mirror-X",
                  "export_basis": "RH Y-up metres; semantic (x,y,z) -> GLB (x,z,-y), no scale or reflection",
                  "import_basis": "UE 5.8.1 Interchange GLTF ConvertVec3 (x,z,y), mesh metre->cm x100 exactly once",
                  "semantic_frame": FRAME, "ports": [port_data(port) for port in PORTS]}
    (destination / "CookedFiducial.basis.json").write_text(json.dumps(provenance, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True, help="private generated directory, not game Content")
    generate(parser.parse_args().output)
