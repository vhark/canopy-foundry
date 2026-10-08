"""IFC native placement and distinct semantic, GLB, and Unreal basis boundaries."""

import math
from uuid import uuid4

import ifcopenshell
import pytest
from growbim.ifc import build_fixture

from canopy_author.coordinates import (
    SEMANTIC_TO_GLB,
    glb_to_semantic,
    length_unit_scale,
    placement_matrix,
    semantic_to_glb,
    semantic_to_unreal,
    transform_mesh,
    transform_points,
    unreal_to_semantic,
)


def _model(unit):
    artifacts = build_fixture("indoor", str(uuid4()), str(uuid4()), length_unit=unit)
    return ifcopenshell.file.from_string(artifacts["model.ifc"].decode("utf-8"))


@pytest.mark.parametrize("unit,expected", [("m", 1.0), ("mm", 0.001), ("ft", 0.3048)])
def test_native_ifc_units_and_nested_translated_quarter_turn(unit, expected):
    model = _model(unit)
    assert length_unit_scale(model) == pytest.approx(expected)
    factor = 1 / expected
    point = lambda xyz: model.createIfcCartesianPoint(tuple(float(n) * factor for n in xyz))
    direction = lambda xyz: model.createIfcDirection(tuple(float(n) for n in xyz))
    parent = model.createIfcLocalPlacement(
        None,
        model.createIfcAxis2Placement3D(
            point((2, 3, 4)), direction((0, 0, 1)), direction((0, 1, 0))
        ),
    )
    child = model.createIfcLocalPlacement(
        parent,
        model.createIfcAxis2Placement3D(
            point((5, 7, 11)), direction((0, 0, 1)), direction((1, 0, 0))
        ),
    )
    matrix = placement_matrix(child, length_unit_scale(model))
    assert isinstance(matrix, tuple) and all(isinstance(row, tuple) for row in matrix)
    actual = transform_points(((0, 0, 0), (1, 2, 3)), matrix)
    for got, wanted in zip(actual, ((-5, 8, 15), (-7, 9, 18))):
        assert got == pytest.approx(wanted)
    assert placement_matrix(child, length_unit_scale(model)) == matrix


def test_unknown_missing_and_ambiguous_units_rejected_with_context():
    model = _model("m")
    project = model.by_type("IfcProject")[0]
    assignment = project.UnitsInContext
    length = next(unit for unit in assignment.Units if unit.UnitType == "LENGTHUNIT")
    assignment.Units = tuple(unit for unit in assignment.Units if unit != length)
    with pytest.raises(ValueError, match="LENGTHUNIT"):
        length_unit_scale(model)
    assignment.Units = (*assignment.Units, length, model.createIfcSIUnit(UnitType="LENGTHUNIT", Name="METRE", Prefix="MILLI"))
    with pytest.raises(ValueError, match="LENGTHUNIT"):
        length_unit_scale(model)
    assignment.Units = (*tuple(unit for unit in assignment.Units if unit.UnitType != "LENGTHUNIT"), model.createIfcSIUnit(UnitType="LENGTHUNIT", Name="METRE", Prefix="CENTI"))
    with pytest.raises(ValueError, match="LENGTHUNIT"):
        length_unit_scale(model)


def test_native_degenerate_placement_rejected_as_offending_frame():
    model = _model("m")
    placement = model.createIfcLocalPlacement(
        None,
        model.createIfcAxis2Placement3D(
            model.createIfcCartesianPoint((1., 2., 3.)),
            model.createIfcDirection((0., 0., 0.)),
            model.createIfcDirection((1., 0., 0.)),
        ),
    )
    with pytest.raises(ValueError, match="IFC placement"):
        placement_matrix(placement, length_unit_scale(model))


def test_mirror_reverses_faces_and_adjusts_normal_and_tangent_basis():
    matrix = ((-2., 0., 0., 4.), (0., 3., 0., -5.), (0., 0., 4., 6.), (0., 0., 0., 1.))
    result = transform_mesh(
        ((0, 0, 0), (1, 0, 0), (0, 1, 0)), ((0, 1, 2),),
        ((0, 0, 1),) * 3, matrix=matrix, tangents=((1, 0, 0, 1),) * 3,
    )
    assert result.vertices == ((4., -5., 6.), (2., -5., 6.), (4., -2., 6.))
    assert result.triangles == ((0, 2, 1),)
    assert result.normals == ((0., 0., 1.),) * 3
    assert result.tangents == ((-1., 0., 0., -1.),) * 3
    assert transform_mesh(((0, 0, 0),), (), ((0, 0, 1),), matrix=matrix).tangents == ()


def test_inverse_transpose_normal_and_tangent_orthogonality():
    matrix = ((2., 0., 0., 0.), (0., 3., 0., 0.), (0., 0., 4., 0.), (0., 0., 0., 1.))
    normal = (1 / math.sqrt(2), 1 / math.sqrt(2), 0)
    tangent = (1 / math.sqrt(2), -1 / math.sqrt(2), 0, -1)
    result = transform_mesh(((1, 2, 3),), (), (normal,), matrix=matrix, tangents=(tangent,))
    expected = (3 / math.sqrt(13), 2 / math.sqrt(13), 0)
    assert result.normals[0] == pytest.approx(expected)
    assert sum(a * b for a, b in zip(result.normals[0], result.tangents[0][:3])) == pytest.approx(0)
    assert result.tangents[0][3] == -1


@pytest.mark.parametrize("matrix", [
    SEMANTIC_TO_GLB,
    ((1., 0., 2., 0.), (0., 2., 1., 0.), (0., 0., 1., 0.), (0., 0., 0., 1.)),
], ids=["glb-quarter-turn", "nonuniform-shear"])
def test_transformed_normals_follow_geometric_face_orientation(matrix):
    mesh = transform_mesh(
        ((0, 0, 0), (2, 0, 0), (0, 3, 0)), ((0, 1, 2),),
        ((0, 0, 1),) * 3, matrix=matrix, tangents=((1, 0, 0, 1),) * 3,
    )
    origin, a, b = (mesh.vertices[index] for index in mesh.triangles[0])
    u = tuple(x - y for x, y in zip(a, origin))
    v = tuple(x - y for x, y in zip(b, origin))
    cross = (u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2], u[0]*v[1] - u[1]*v[0])
    expected = tuple(component / math.hypot(*cross) for component in cross)
    for normal, tangent in zip(mesh.normals, mesh.tangents):
        assert normal == pytest.approx(expected)
        assert math.hypot(*tangent[:3]) == pytest.approx(1)
        assert sum(n * t for n, t in zip(normal, tangent[:3])) == pytest.approx(0)


def test_invalid_frames_mesh_shapes_indices_and_nonfinite_rejected():
    identity = ((1., 0., 0., 0.), (0., 1., 0., 0.), (0., 0., 1., 0.), (0., 0., 0., 1.))
    with pytest.raises(ValueError, match="matrix"):
        transform_points(((0, 0, 0),), ((1, 0, 0, 0),))
    with pytest.raises(ValueError, match="finite"):
        transform_points(((math.nan, 0, 0),), identity)
    with pytest.raises(ValueError, match="finite"):
        transform_points(((1e308, 0, 0),), ((1e308, 0, 0, 0), *identity[1:]))
    with pytest.raises(ValueError, match="singular"):
        transform_mesh(((0, 0, 0),), (), ((0, 0, 1),), matrix=((0, 0, 0, 0), *identity[1:]))
    with pytest.raises(ValueError, match="triangle"):
        transform_mesh(((0, 0, 0),), ((0, 1, 2),), ((0, 0, 1),), matrix=identity)
    with pytest.raises(ValueError, match="normals"):
        transform_mesh(((0, 0, 0),), (), (), matrix=identity)
    with pytest.raises(ValueError, match="tangent"):
        transform_mesh(((0, 0, 0),), (), ((0, 0, 1),), matrix=identity, tangents=((0, 0, 0, 1),))
    class Oversized:
        def __len__(self):
            return 1_000_001

        def __iter__(self):
            raise AssertionError("must reject before consuming oversized points")

    with pytest.raises(ValueError, match="limit"):
        transform_points(Oversized(), identity)


def test_glb_is_right_handed_metres_not_unreal_centimetres():
    semantic = ((1.25, 2.5, 3.75), (-2., 0., 1.))
    assert semantic_to_glb(semantic) == ((1.25, 3.75, -2.5), (-2., 1., 0.))
    assert glb_to_semantic(semantic_to_glb(semantic)) == semantic
    assert semantic_to_unreal(semantic) == ((125., -250., 375.), (-200., -0., 100.))
    assert unreal_to_semantic(semantic_to_unreal(semantic)) == semantic
    assert transform_points(semantic, SEMANTIC_TO_GLB) == semantic_to_glb(semantic)
    mesh = transform_mesh(
        ((0, 0, 0), (1, 0, 0), (0, 1, 0)), ((0, 1, 2),), ((0, 0, 1),) * 3,
        matrix=((100, 0, 0, 0), (0, -100, 0, 0), (0, 0, 100, 0), (0, 0, 0, 1)),
    )
    assert mesh.triangles == ((0, 2, 1),)
    assert mesh.normals == ((0., 0., 1.),) * 3
