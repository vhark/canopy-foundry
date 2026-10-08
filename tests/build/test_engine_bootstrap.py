import hashlib
import importlib
import io
import json
import tarfile

import pytest

from scripts import build_common


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
