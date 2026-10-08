"""Offline regression tests for native bootstrap failure and provenance recovery."""

import json
from pathlib import Path
import sys


from scripts import bootstrap_native as native


COMMIT = "2750401336fb7c95f6619657a46a7e798661341c"
SOURCE = "https://github.com/microsoft/vcpkg.git"


def fixture_root(tmp_path, monkeypatch):
    root = tmp_path / "source"
    dependencies = root / "dependencies"
    dependencies.mkdir(parents=True)
    (dependencies / "native-lock.json").write_text(json.dumps({
        "vcpkg": {"repository": SOURCE, "commit": COMMIT}, "ports": {},
    }))
    (dependencies / "vcpkg.json").write_text(json.dumps({
        "name": "test-native", "dependencies": [], "overrides": [],
    }))
    (dependencies / "vcpkg-configuration.json").write_text(json.dumps({
        "default-registry": {"kind": "builtin", "baseline": COMMIT},
    }))
    script = root / "scripts/bootstrap_native.py"
    script.parent.mkdir()
    script.write_text("# synthetic bootstrap source\n")
    monkeypatch.setattr(native, "ROOT", root)
    monkeypatch.setattr(native, "LOCK", dependencies / "native-lock.json")
    monkeypatch.setattr(native, "CHECKOUT", root / ".work/tools/vcpkg")
    monkeypatch.setattr(native, "EVIDENCE", root / ".build/native-bootstrap.json")
    monkeypatch.setattr(native, "__file__", str(script))
    monkeypatch.setattr(native, "ATTEMPT", root / ".build/native-bootstrap-attempt.json")
    return root


def test_failed_attempt_preserves_successful_provenance_and_reuses_binary(tmp_path, monkeypatch):
    root = fixture_root(tmp_path, monkeypatch)
    checkout = native.CHECKOUT
    (checkout / ".git").mkdir(parents=True)
    script = checkout / ("bootstrap-vcpkg.bat" if native.os.name == "nt" else "bootstrap-vcpkg.sh")
    script.write_text("pinned script\n")
    binary = checkout / ("vcpkg.exe" if native.os.name == "nt" else "vcpkg")
    binary.write_bytes(b"trusted binary")
    inputs = {str(path.relative_to(root)): native.digest(path) for path in (
        native.LOCK, root / "dependencies/vcpkg.json",
        root / "dependencies/vcpkg-configuration.json", root / "scripts/bootstrap_native.py",
    )}
    source = {"repository": SOURCE, "commit": COMMIT, "tree": "tree-id", "bootstrap_script_sha256": native.digest(script)}
    successful = {"schema": 1, "status": "success", "inputs": inputs, "source": source,
                  "output": {"path": str(binary.relative_to(root)), "sha256": native.digest(binary)}}
    native.write_evidence(successful)
    original = native.EVIDENCE.read_bytes()
    monkeypatch.setattr(native.shutil, "which", lambda executable: sys.executable)

    def git_command(args, record, *, cwd=root):
        operation = args[1:] if args[1] != "-C" else args[3:]
        if operation == ["--version"]:
            return "git version fixture"
        if operation == ["remote", "get-url", "origin"]:
            return SOURCE
        if operation == ["rev-parse", "HEAD"]:
            return COMMIT
        if operation == ["rev-parse", "HEAD^{tree}"]:
            return "tree-id"
        if operation == ["status", "--porcelain", "--untracked-files=all"]:
            return " M scripts/buildsystems/vcpkg.cmake" if dirty[0] else ""
        if operation == ["ls-files", "--others", "--ignored", "--exclude-standard", "--", "triplets"]:
            return "triplets/custom.cmake" if custom_triplet[0] else ""
        raise AssertionError(args)

    monkeypatch.setattr(native, "run", git_command)
    dirty = [True]
    custom_triplet = [False]
    assert native.main() == 1
    assert native.EVIDENCE.read_bytes() == original
    assert json.loads(native.ATTEMPT.read_text())["status"] == "failed"
    dirty[0] = False
    custom_triplet[0] = True
    assert native.main() == 1
    assert native.EVIDENCE.read_bytes() == original
    custom_triplet[0] = False
    assert native.main() == 0
    assert not native.ATTEMPT.exists()
    assert json.loads(native.EVIDENCE.read_text())["reused_verified_output"] is True
    assert native.digest(binary) == successful["output"]["sha256"]
    trusted = native.EVIDENCE.read_bytes()
    binary.write_bytes(b"tampered binary")
    assert native.main() == 1
    assert native.EVIDENCE.read_bytes() == trusted


def test_failed_initial_fetch_never_publishes_incomplete_checkout(tmp_path, monkeypatch):
    fixture_root(tmp_path, monkeypatch)
    monkeypatch.setattr(native.shutil, "which", lambda executable: sys.executable)

    def fail_fetch(args, record, *, cwd=native.ROOT):
        if "init" in args:
            Path(args[-1]).mkdir(parents=True, exist_ok=True)
            return ""
        if "fetch" in args:
            raise RuntimeError("offline fetch failure")
        if args[-1] == "--version" or "remote" in args:
            return "git version fixture"
        raise AssertionError(args)

    monkeypatch.setattr(native, "run", fail_fetch)
    assert native.main() == 1
    assert json.loads(native.ATTEMPT.read_text())["error"] == "offline fetch failure"
    assert not native.CHECKOUT.exists()

    def repaired_fetch(args, record, *, cwd=native.ROOT):
        if "init" in args:
            stage = Path(args[-1])
            (stage / ".git").mkdir(parents=True)
            (stage / ("bootstrap-vcpkg.bat" if native.os.name == "nt" else "bootstrap-vcpkg.sh")).write_text("bootstrap")
            return ""
        if "remote" in args:
            return SOURCE
        if "rev-parse" in args:
            return "tree-id" if "HEAD^{tree}" in args else COMMIT
        if "status" in args or "ls-files" in args:
            return ""
        if "fetch" in args or "checkout" in args or "--version" in args:
            return "git version fixture"
        (native.CHECKOUT / ("vcpkg.exe" if native.os.name == "nt" else "vcpkg")).write_bytes(b"bootstrap output")
        return ""

    monkeypatch.setattr(native, "run", repaired_fetch)
    assert native.main() == 0
    assert native.CHECKOUT.is_dir()
    assert json.loads(native.EVIDENCE.read_text())["status"] == "success"
    assert not native.ATTEMPT.exists()
    assert list(native.CHECKOUT.parent.glob(".vcpkg-stage-*")) == []
