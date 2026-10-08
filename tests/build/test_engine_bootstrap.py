import hashlib
import importlib
import io
import json
import tarfile

import pytest

from scripts import build_common

from scripts import bootstrap_engine, build_game


def test_full_editor_records_original_manifest_and_selected_files(tmp_path):
    build = tmp_path / "Engine/Build"
    build.mkdir(parents=True)
    (build / "Commit.gitdeps.xml").write_text("<DependencyManifest/>")
    (tmp_path / "Engine/Binaries/Mac").mkdir(parents=True)
    (tmp_path / "Engine/Binaries/Mac/example").write_bytes(b"original")
    digest = hashlib.sha1(b"original").hexdigest()
    (tmp_path / ".uedependencies").write_text(
        f'<WorkingManifest><Files><File Name="Engine/Binaries/Mac/example" '
        f'Hash="{digest}" ExpectedHash="{digest}" /></Files></WorkingManifest>')
    evidence = bootstrap_engine._dependency_manifest(tmp_path)
    assert evidence["selected_file_count"] == 1
    (tmp_path / "Engine/Binaries/Mac/example").write_bytes(b"altered")
    with pytest.raises(build_common.BuildFailure):
        bootstrap_engine._dependency_manifest(tmp_path)
    (tmp_path / "Engine/Binaries/Mac/example").unlink()
    with pytest.raises(build_common.BuildFailure, match="incomplete"):
        bootstrap_engine._dependency_manifest(tmp_path)



def test_f03_report_rejects_skipped_or_missing_automation(tmp_path):
    # UE 5.8.1 exports UStruct properties through JsonObjectConverter's camel-case mapping.
    tests = [{"fullTestPath": name, "state": "Success", "errors": 0}
             for name in sorted(build_game.F03_TESTS)]
    report = {"succeeded": len(tests), "failed": 0, "notRun": 0, "tests": tests}
    path = tmp_path / "index.json"
    path.write_text(json.dumps(report), encoding="utf-8-sig")
    report = build_game.read_engine_report(path)
    tests = report["tests"]
    build_game.validate_automation_report(report)
    tests[0]["state"] = "Skipped"
    with pytest.raises(build_common.BuildFailure):
        build_game.validate_automation_report(report)
    tests[0]["state"] = "Success"
    report["tests"] = tests[:-1]
    with pytest.raises(build_common.BuildFailure):
        build_game.validate_automation_report(report)


def test_packaged_report_rejects_wrong_case_engine_and_incomplete_state(tmp_path):
    report = {
        "schema": 1, "case": "f03-native-room", "status": "pass",
        "engine": "5.8.1-0+++UE5+Release-5.8",
        "measurements": {"second": 60, "revision": 3, "appliedControl": .75, "completedStage": 4},
    }
    path = tmp_path / "f03.json"
    path.write_text(json.dumps(report), encoding="utf-8")
    build_game.validate_packaged_report(build_game.read_engine_report(path), "f03-native-room")
    for field, invalid in (("case", "b02-cooked-fiducial"), ("status", "fail"),
                           ("engine", "5.8.2-0+++UE5+Release-5.8"), ("schema", True)):
        with pytest.raises(build_common.BuildFailure):
            build_game.validate_packaged_report({**report, field: invalid}, "f03-native-room")
    with pytest.raises(build_common.BuildFailure):
        build_game.validate_packaged_report({**report, "measurements": {"completedStage": 3}},
                                            "f03-native-room")


def test_cooked_report_requires_measured_geometry_and_complete_port_frames(tmp_path):
    measurements = {
        "renderVertices": 18, "collisionVertices": 12, "collisionUniqueCorners": 4,
        "collisionRayBlocked": True,
        "ports": [{"position": [152, -35, 65], "direction": [0, 1, 0]},
                  {"position": [115, -103, 75], "direction": [-1, 0, 0]}],
    }
    report = {"schema": 1, "case": "b02-cooked-fiducial", "status": "pass",
              "engine": "5.8.1-0+++UE5+Release-5.8", "measurements": measurements}
    path = tmp_path / "b02.json"
    path.write_text(json.dumps(report), encoding="utf-8")
    build_game.validate_packaged_report(build_game.read_engine_report(path), "b02-cooked-fiducial")
    for field, invalid in (("collisionVertices", 3), ("collisionUniqueCorners", 5),
                           ("collisionRayBlocked", False), ("ports", [{}, {}])):
        with pytest.raises(build_common.BuildFailure):
            build_game.validate_packaged_report(
                {**report, "measurements": {**measurements, field: invalid}}, "b02-cooked-fiducial")



def source_archive(tmp_path, *, patch=1, escape=False):
    root = "EpicGames-UnrealEngine-" + "1" * 40
    archive = tmp_path / "source.tar.gz"
    version = json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": patch}).encode()
    with tarfile.open(archive, "w:gz") as output:
        member = tarfile.TarInfo(f"{root}/Engine/Build/Build.version")
        member.size = len(version)
        output.addfile(member, io.BytesIO(version))
        if escape:
            member = tarfile.TarInfo(f"{root}/../../escaped")
            member.size = 6
            output.addfile(member, io.BytesIO(b"escape"))
    requirements = {
        "engine": {"major": 5, "minor": 8, "patch": 1},
        "source": {
            "commit": "1" * 40,
            "archive_root": root,
            "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        },
    }
    return archive, requirements


def test_wrong_archive_hash_cannot_install_engine(tmp_path):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    archive, requirements = source_archive(tmp_path)
    requirements["source"]["archive_sha256"] = "0" * 64
    destination = tmp_path / "engine"
    with pytest.raises(build_common.BuildFailure):
        bootstrap.extract_source(archive, destination, requirements)
    assert not destination.exists()


def test_matching_archive_hash_cannot_admit_wrong_engine_patch(tmp_path):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    archive, requirements = source_archive(tmp_path, patch=3)
    destination = tmp_path / "engine"
    with pytest.raises(build_common.BuildFailure):
        bootstrap.extract_source(archive, destination, requirements)
    assert not destination.exists()


def test_archive_cannot_escape_its_installation(tmp_path):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    archive, requirements = source_archive(tmp_path, escape=True)
    destination = tmp_path / "engine"
    with pytest.raises(build_common.BuildFailure):
        bootstrap.extract_source(archive, destination, requirements)
    assert not destination.exists()
    assert not (tmp_path / "escaped").exists()


def test_boolean_patch_cannot_substitute_for_engine_version(tmp_path):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    archive, requirements = source_archive(tmp_path, patch=True)
    destination = tmp_path / "engine"
    with pytest.raises(build_common.BuildFailure):
        bootstrap.extract_source(archive, destination, requirements)
    assert not destination.exists()


def test_existing_engine_is_not_overwritten(tmp_path):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    archive, requirements = source_archive(tmp_path)
    destination = tmp_path / "engine"
    destination.mkdir()
    marker = destination / "owner-data"
    marker.write_bytes(b"existing engine installation")
    with pytest.raises(build_common.BuildFailure):
        bootstrap.extract_source(archive, destination, requirements)
    assert marker.read_bytes() == b"existing engine installation"
    assert not (destination / "Engine/Build/Build.version").exists()


def test_failed_evidence_publication_restores_staged_source(tmp_path, monkeypatch):
    bootstrap = importlib.import_module("scripts.bootstrap_engine")
    installed = tmp_path / "staging/source"
    installed.mkdir(parents=True)
    (installed / "payload").write_bytes(b"owned staged source")
    destination = tmp_path / "engine"
    blocked_parent = tmp_path / "not-a-directory"
    blocked_parent.write_bytes(b"unrelated owner data")
    monkeypatch.setattr(bootstrap, "EVIDENCE", blocked_parent / "evidence.json")
    record = {"engine_root": str(destination)}
    with pytest.raises(OSError):
        bootstrap._publish_source(installed, destination, record)
    assert not destination.exists()
    assert (installed / "payload").read_bytes() == b"owned staged source"
    assert blocked_parent.read_bytes() == b"unrelated owner data"

    evidence = tmp_path / "evidence.json"
    monkeypatch.setattr(bootstrap, "EVIDENCE", evidence)
    bootstrap._publish_source(installed, destination, record)
    assert not installed.exists()
    assert (destination / "payload").read_bytes() == b"owned staged source"
    assert json.loads(evidence.read_text())["engine_root"] == str(destination)
