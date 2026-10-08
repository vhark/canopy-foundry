"""IFC local-length geometry -> RH Z-up metres -> GLB or Unreal, exactly once.

``placement_matrix`` expects a native IFC placement in the model's declared length
unit, not geometry or a matrix previously normalized to SI. Mesh vertices supplied
to ``transform_mesh`` are in the same units as its matrix's linear basis. GLB
export is RH Y-up metres; Unreal bridge output is LH Z-up centimetres. Do not
send already-converted GLB coordinates through the Unreal conversion again.
"""

from __future__ import annotations

import math
from typing import NamedTuple

import ifcopenshell.util.placement
import ifcopenshell.util.unit

MAX_POINTS = 1_000_000
MAX_TRIANGLES = 2_000_000

# -90 degrees about X: RH Z-up metres -> RH Y-up metres; inverse is the transpose.
SEMANTIC_TO_GLB = ((1., 0., 0., 0.), (0., 0., 1., 0.), (0., -1., 0., 0.), (0., 0., 0., 1.))
GLB_TO_SEMANTIC = ((1., 0., 0., 0.), (0., 0., -1., 0.), (0., 1., 0., 0.), (0., 0., 0., 1.))


class MeshTransform(NamedTuple):
    vertices: tuple[tuple[float, float, float], ...]
    triangles: tuple[tuple[int, int, int], ...]
    normals: tuple[tuple[float, float, float], ...]
    tangents: tuple[tuple[float, float, float, float], ...]


def length_unit_scale(model) -> float:
    """Return metres per native IFC4 project coordinate; accept m, mm or foot."""
    if model.schema != "IFC4":
        raise ValueError(f"IFC schema {model.schema!r}: expected IFC4")
    projects = model.by_type("IfcProject")
    if len(projects) != 1 or projects[0].UnitsInContext is None:
        raise ValueError("IFC project LENGTHUNIT declaration is missing or ambiguous")
    units = [unit for unit in projects[0].UnitsInContext.Units if unit.UnitType == "LENGTHUNIT"]
    if len(units) != 1:
        raise ValueError(f"IFC LENGTHUNIT: expected exactly one, found {len(units)}")
    unit = units[0]
    if unit.is_a("IfcSIUnit"):
        supported = unit.Name == "METRE" and unit.Prefix in (None, "MILLI")
    else:
        supported = unit.is_a("IfcConversionBasedUnit") and unit.Name == "foot"
    if not supported:
        raise ValueError(f"IFC LENGTHUNIT {unit}: only m, mm and foot are supported")
    try:
        scale = float(ifcopenshell.util.unit.calculate_unit_scale(model))
    except (ValueError, TypeError, AttributeError, KeyError, ZeroDivisionError) as exc:
        raise ValueError(f"IFC LENGTHUNIT {unit}: invalid conversion") from exc
    if not math.isfinite(scale) or scale <= 0 or (
        unit.is_a("IfcConversionBasedUnit") and not math.isclose(scale, 0.3048, rel_tol=0, abs_tol=1e-12)
    ):
        raise ValueError(f"IFC LENGTHUNIT {unit}: unsupported conversion to metres")
    return scale


def _finite_number(value, label):
    try:
        result = float(value)
    except (ValueError, TypeError, OverflowError) as exc:
        raise ValueError(f"{label}: expected finite number") from exc
    if not math.isfinite(result):
        raise ValueError(f"{label}: expected finite number")
    return result


def _matrix(matrix):
    try:
        if len(matrix) != 4 or any(len(row) != 4 for row in matrix):
            raise ValueError("matrix: expected finite 4x4 affine frame")
        rows = tuple(tuple(_finite_number(value, "matrix") for value in row) for row in matrix)
    except (TypeError, OverflowError) as exc:
        raise ValueError("matrix: expected finite 4x4 affine frame") from exc
    if rows[3] != (0., 0., 0., 1.):
        raise ValueError("matrix: expected finite 4x4 affine frame")
    return rows


def placement_matrix(placement, length_scale=1.0):
    """Compose native IFC placement; scale world translation to SI once, not axes."""
    scale = _finite_number(length_scale, "length_scale")
    if scale <= 0:
        raise ValueError("length_scale: expected positive metres per IFC length unit")
    if placement is None or not placement.is_a("IfcLocalPlacement"):
        raise ValueError("IFC placement: expected IfcLocalPlacement")
    try:
        native = ifcopenshell.util.placement.get_local_placement(placement)
        rows = tuple(tuple(float(native[i, j]) * (scale if j == 3 and i < 3 else 1.) for j in range(4)) for i in range(4))
    except (ValueError, TypeError, AttributeError, IndexError, RecursionError) as exc:
        raise ValueError(f"IFC placement {placement}: invalid native frame") from exc
    try:
        rows = _matrix(rows)
        _inverse_linear(rows)
    except ValueError as exc:
        raise ValueError(f"IFC placement {placement}: {exc}") from exc
    return rows


def _triples(values, label, limit=MAX_POINTS):
    try:
        if len(values) > limit:
            raise ValueError(f"{label}: limit {limit} exceeded")
    except TypeError:
        pass  # Streams are bounded by the enumerated limit below.
    try:
        iterator = iter(values)
    except TypeError as exc:
        raise ValueError(f"{label}: expected triples") from exc
    result = []
    for index, value in enumerate(iterator):
        if index >= limit:
            raise ValueError(f"{label}: limit {limit} exceeded")
        try:
            if len(value) != 3:
                raise ValueError(f"{label}[{index}]: expected three finite coordinates")
            result.append(tuple(_finite_number(component, f"{label}[{index}]") for component in value))
        except TypeError as exc:
            raise ValueError(f"{label}[{index}]: expected three finite coordinates") from exc
    return tuple(result)


def transform_points(points, matrix):
    """Apply affine frame to finite bounded triples; result is immutable."""
    m = _matrix(matrix)
    return tuple(
        tuple(
            _finite_number(m[r][0]*x + m[r][1]*y + m[r][2]*z + m[r][3], "points: transformed coordinate")
            for r in range(3)
        )
        for x, y, z in _triples(points, "points")
    )


def semantic_to_glb(points):
    """RH Z-up metres to GLB RH Y-up metres; no rescaling or reflection."""
    return transform_points(points, SEMANTIC_TO_GLB)


def glb_to_semantic(points):
    """RH Y-up GLB metre coordinates to RH Z-up semantic metres."""
    return transform_points(points, GLB_TO_SEMANTIC)


def semantic_to_unreal(points):
    """RH Z-up metres to Unreal LH Z-up centimetres, once at the bridge."""
    return transform_points(points, ((100., 0., 0., 0.), (0., -100., 0., 0.), (0., 0., 100., 0.), (0., 0., 0., 1.)))


def unreal_to_semantic(points):
    """Unreal LH Z-up centimetres back to semantic RH Z-up metres."""
    return transform_points(points, ((.01, 0., 0., 0.), (0., -.01, 0., 0.), (0., 0., .01, 0.), (0., 0., 0., 1.)))


def _inverse_linear(m):
    (a, b, c, _), (d, e, f, _), (g, h, i, _) = m[:3]
    cof = ((e*i-f*h, f*g-d*i, d*h-e*g),
           (c*h-b*i, a*i-c*g, b*g-a*h),
           (b*f-c*e, c*d-a*f, a*e-b*d))
    determinant = a*cof[0][0] + b*cof[0][1] + c*cof[0][2]
    if not math.isfinite(determinant) or determinant == 0:
        raise ValueError("matrix: singular or nonfinite linear frame")
    inverse = tuple(tuple(cof[k][r] / determinant for k in range(3)) for r in range(3))
    if not all(math.isfinite(x) for row in inverse for x in row):
        raise ValueError("matrix: singular or nonfinite linear frame")
    return inverse, determinant


def _unit(v, label):
    length = math.hypot(*v)
    if not math.isfinite(length) or length == 0:
        raise ValueError(f"{label}: zero-length or nonfinite direction")
    return tuple(x / length for x in v)


def transform_mesh(vertices, triangles, normals, *, matrix, tangents=()):
    """Transform triangle mesh; reflect winding and TBN parity if determinant < 0.

    Normals and optional tangent quadruples (direction xyz, handedness ±1) are
    per vertex. Tangents are reprojected onto the transformed normal plane;
    normal uses inverse transpose of the full linear basis.
    """
    m = _matrix(matrix)
    inverse, determinant = _inverse_linear(m)
    positions = _triples(vertices, "vertices")
    directions = _triples(normals, "normals")
    if len(directions) != len(positions):
        raise ValueError("normals: expected one per vertex")
    try:
        if len(triangles) > MAX_TRIANGLES:
            raise ValueError(f"triangles: limit {MAX_TRIANGLES} exceeded")
    except TypeError:
        pass
    faces = []
    for index, face in enumerate(triangles):
        if index >= MAX_TRIANGLES:
            raise ValueError(f"triangles: limit {MAX_TRIANGLES} exceeded")
        try:
            if len(face) != 3 or any(type(vertex) is not int or vertex < 0 or vertex >= len(positions) for vertex in face):
                raise ValueError(f"triangle[{index}]: invalid vertex indices")
        except TypeError as exc:
            raise ValueError(f"triangle[{index}]: expected three indices") from exc
        faces.append((face[0], face[2], face[1]) if determinant < 0 else tuple(face))
    normals_out = []
    for normal in directions:
        normals_out.append(_unit(tuple(sum(inverse[k][r]*normal[k] for k in range(3)) for r in range(3)), "normals"))
    tangent_out = []
    for index, tangent in enumerate(tangents):
        if index >= len(positions) or len(tangent) != 4:
            raise ValueError("tangents: expected one quadruple per vertex")
        xyz = tuple(_finite_number(v, f"tangent[{index}]") for v in tangent[:3])
        sign = _finite_number(tangent[3], f"tangent[{index}]")
        if sign not in (-1., 1.):
            raise ValueError(f"tangent[{index}]: handedness must be +1 or -1")
        n = normals_out[index]
        transformed = tuple(sum(m[r][k]*xyz[k] for k in range(3)) for r in range(3))
        dot = sum(transformed[k]*n[k] for k in range(3))
        tangent_out.append((*_unit(tuple(transformed[k]-dot*n[k] for k in range(3)), "tangent"), sign * (-1. if determinant < 0 else 1.)))
    if tangent_out and len(tangent_out) != len(positions):
        raise ValueError("tangents: expected one per vertex")
    transformed_positions = transform_points(positions, m)
    if not all(math.isfinite(v) for position in transformed_positions for v in position):
        raise ValueError("vertices: transform produced nonfinite position")
    return MeshTransform(transformed_positions, tuple(faces), tuple(normals_out), tuple(tangent_out))
