import importlib
import json

import pytest

from scripts import build_common

def test_program_qualification_rejects_installed_editor(tmp_path, monkeypatch):
    qualifier = importlib.import_module("scripts.qualify_engine_core")
    engine = tmp_path / "UE_5.8"
    marker = engine / "Engine/Build/InstalledBuild.txt"
    marker.parent.mkdir(parents=True)
    marker.touch()
    monkeypatch.setattr(qualifier, "ROOT", tmp_path)
    monkeypatch.setattr(qualifier.build_core, "host_platform", lambda: "mac-arm64")

    with pytest.raises(build_common.BuildFailure, match="source-only.*installed"):
        qualifier.qualify(engine)


def consumer_record():
    return {
        "area_square_metres": 5574.1824,
        "platform": "linux-x64",
        "cpp_version": 202002,
        "pointer_bits": 64,
        "rtti": False,
        "exceptions": False,
        "crt": "libc++",
        "compiler_id": "Clang",
        "compiler_version": "20.1.8",
    }


def core_manifest():
    return {
        "platform": "linux-x64",
        "compiler": {"id": "Clang", "version": "clang version 20.1.8 (released compiler)"},
        "abi": {"crt": "libc++", "rtti": False, "exceptions": False},
    }


@pytest.mark.parametrize("field,value", [
    ("platform", "mac-arm64"),
    ("cpp_version", 201703),
    ("pointer_bits", 32),
    ("rtti", True),
    ("exceptions", True),
    ("crt", "libstdc++"),
    ("compiler_id", "GNU"),
    ("compiler_version", "20.1.7"),
])
def test_incompatible_native_consumer_cannot_qualify(field, value):
    qualifier = importlib.import_module("scripts.qualify_engine_core")
    record = consumer_record()
    record[field] = value
    with pytest.raises(build_common.BuildFailure):
        qualifier.validate_consumer_output(json.dumps(record), core_manifest())


@pytest.mark.parametrize("area", [18288.0, float("nan"), float("inf"), "5574.1824", True])
def test_incorrect_or_unmeasured_conversion_cannot_qualify(area):
    qualifier = importlib.import_module("scripts.qualify_engine_core")
    record = consumer_record()
    record["area_square_metres"] = area
    with pytest.raises(build_common.BuildFailure):
        qualifier.validate_consumer_output(json.dumps(record), core_manifest())


def test_missing_runtime_abi_measurement_cannot_qualify():
    qualifier = importlib.import_module("scripts.qualify_engine_core")
    record = consumer_record()
    del record["exceptions"]
    with pytest.raises(build_common.BuildFailure):
        qualifier.validate_consumer_output(json.dumps(record), core_manifest())
