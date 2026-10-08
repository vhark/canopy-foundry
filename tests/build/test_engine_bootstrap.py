import hashlib
import plistlib
from pathlib import Path
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
    # UE 5.8.3 exports UStruct properties through JsonObjectConverter's camel-case mapping.
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
        "engine": "5.8.3-0+++UE5+Release-5.8",
        "measurements": {"second": 60, "revision": 3, "appliedControl": .75, "completedStage": 4},
    }
    path = tmp_path / "f03.json"
    path.write_text(json.dumps(report), encoding="utf-8")
    build_game.validate_packaged_report(build_game.read_engine_report(path), "f03-native-room", "5.8.3")
    for field, invalid in (("case", "b02-cooked-fiducial"), ("status", "fail"),
                           ("engine", "5.8.1-0+++UE5+Release-5.8"), ("schema", True)):
        with pytest.raises(build_common.BuildFailure):
            build_game.validate_packaged_report({**report, field: invalid}, "f03-native-room", "5.8.3")
    with pytest.raises(build_common.BuildFailure):
        build_game.validate_packaged_report({**report, "measurements": {"completedStage": 3}},
                                            "f03-native-room", "5.8.3")


def test_cooked_report_requires_measured_geometry_and_complete_port_frames(tmp_path):
    measurements = {
        "renderVertices": 18, "collisionVertices": 12, "collisionUniqueCorners": 4,
        "collisionRayBlocked": True,
        "ports": [{"position": [152, -35, 65], "direction": [0, 1, 0]},
                  {"position": [115, -103, 75], "direction": [-1, 0, 0]}],
    }
    report = {"schema": 1, "case": "b02-cooked-fiducial", "status": "pass",
              "engine": "5.8.3-0+++UE5+Release-5.8", "measurements": measurements}
    path = tmp_path / "b02.json"
    path.write_text(json.dumps(report), encoding="utf-8")
    build_game.validate_packaged_report(build_game.read_engine_report(path), "b02-cooked-fiducial", "5.8.3")
    for field, invalid in (("collisionVertices", 3), ("collisionUniqueCorners", 5),
                           ("collisionRayBlocked", False), ("ports", [{}, {}])):
        with pytest.raises(build_common.BuildFailure):
            build_game.validate_packaged_report(
                {**report, "measurements": {**measurements, field: invalid}}, "b02-cooked-fiducial", "5.8.3")



def installed_fixture(tmp_path, monkeypatch):
    root, engine = tmp_path / "checkout", tmp_path / "UE_5.8"
    root.mkdir()
    version = {"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3,
               "Changelist": 58210709, "BranchName": "++UE5+Release-5.8"}
    files = {
        "Engine/Build/Build.version": json.dumps(version).encode(),
        "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor": b"official editor",
        "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll": b"official ubt",
        "Engine/Binaries/ThirdParty/DotNet/10.0/mac-arm64/dotnet": b"official dotnet",
        "Engine/Build/BatchFiles/Mac/Build.sh": b"official build",
        "Engine/Build/BatchFiles/RunUAT.sh": b"official uat",
        "Engine/Source/Runtime/Engine/Public/Engine.h": b"official header",
        "Engine/Binaries/Mac/UnrealEditor.app/Contents/Info.plist":
            plistlib.dumps({"CFBundleShortVersionString": "5.8.3", "CFBundleExecutable": "UnrealEditor"}),
    }
    for name, content in files.items():
        path = engine / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    pinned = {"app_name": "UE_5.8", "app_version": "5.8.3-58210709+++UE5+Release-5.8-Mac",
              "build_version_sha256": build_common.sha256(engine / "Engine/Build/Build.version"),
              "editor_sha256": build_common.sha256(engine / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"),
              "ubt_sha256": build_common.sha256(engine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll")}
    config = root / "config/toolchains.json"
    config.parent.mkdir()
    config.write_text(json.dumps({"unreal": {"engine": {"major": 5, "minor": 8, "patch": 3},
                                             "source": {"dotnet_directory": "10.0"},
                                             "installed": {"mac-arm64": pinned}}}))
    script = root / "scripts/bootstrap_engine.py"
    script.parent.mkdir()
    script.write_bytes(Path(bootstrap_engine.__file__).read_bytes())
    registry = tmp_path / "LauncherInstalled.dat"
    registry.write_text(json.dumps({"InstallationList": [
        {"InstallLocation": str(engine), "NamespaceId": "ue", "AppName": pinned["app_name"],
         "AppVersion": pinned["app_version"]}]}))
    items = tmp_path / "Manifests"
    items.mkdir()
    (items / "UE.item").write_text(json.dumps({
        "InstallLocation": str(engine), "AppName": pinned["app_name"],
        "AppVersionString": pinned["app_version"], "bIsIncompleteInstall": False,
        "InstallationGuid": "UE", "ManifestLocation": str(engine / ".egstore")}))
    store = engine / ".egstore"
    store.mkdir()
    (store / "UE.manifest").write_bytes(b"launcher manifest")
    monkeypatch.setattr(bootstrap_engine, "LAUNCHER_REGISTRY", registry)
    monkeypatch.setattr(bootstrap_engine, "LAUNCHER_MANIFESTS", items)
    monkeypatch.setattr(bootstrap_engine, "EVIDENCE", root / ".build/engine-bootstrap.json")
    monkeypatch.setattr(bootstrap_engine, "run", lambda argv, **kwargs: "10.0.1" if "--version" in argv else "SDK 10")
    tool = {"compiler": {"id": "AppleClang", "version": "27", "binary_sha256": "compiler"},
            "qualification": {"xcode": "27.0"}}
    return root, engine, tool, pinned, registry, items


def test_installed_registration_detects_payload_and_receipt_changes(tmp_path, monkeypatch):
    root, engine, tool, pinned, registry, items = installed_fixture(tmp_path, monkeypatch)
    evidence = bootstrap_engine.register_installed_editor(root, engine, "mac-arm64", tool)
    assert evidence["scope"] == "unreal-installed-editor"
    assert "source" not in evidence and "gitdependencies" not in evidence
    result = bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)
    assert result["installed"] is True and result["boot_path"] == root / ".build/engine-bootstrap.json"
    assert result["dependency_manifest"] == evidence["installed"]["inventory"]
    assert evidence["dotnet"]["version_returncode"] == 0
    input_file = engine / "Engine/Source/Runtime/Engine/Public/Engine.h"
    input_file.write_bytes(b"altered header")
    with pytest.raises(build_common.BuildFailure, match="inventory"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)
    input_file.write_bytes(b"official header")
    wrong = json.loads(registry.read_text())
    wrong["InstallationList"][0]["AppVersion"] = "5.8.1-older"
    registry.write_text(json.dumps(wrong))
    with pytest.raises(build_common.BuildFailure, match="Launcher"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)
    registry.write_text(json.dumps({"InstallationList": [
        {"InstallLocation": str(engine), "NamespaceId": "ue", "AppName": pinned["app_name"],
         "AppVersion": pinned["app_version"]}]}))
    receipt = items / "UE.item"
    wrong = json.loads(receipt.read_text())
    wrong["AppVersionString"] = "5.8.1-older"
    receipt.write_text(json.dumps(wrong))
    with pytest.raises(build_common.BuildFailure, match="Launcher"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)


def test_installed_failure_preserves_evidence_and_generated_caches_are_excluded(tmp_path, monkeypatch):
    root, engine, tool, _, _, _ = installed_fixture(tmp_path, monkeypatch)
    boot = root / ".build/engine-bootstrap.json"
    boot.parent.mkdir()
    boot.write_bytes(b"prior evidence")
    altered = engine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
    altered.write_bytes(b"tampered ubt")
    with pytest.raises(build_common.BuildFailure, match="UnrealBuildTool"):
        bootstrap_engine.register_installed_editor(root, engine, "mac-arm64", tool)
    assert boot.read_bytes() == b"prior evidence"
    altered.write_bytes(b"official ubt")
    bootstrap_engine.register_installed_editor(root, engine, "mac-arm64", tool)
    for relative in (
        "Engine/Saved/Logs/generated.bin", "Engine/DerivedDataCache/generated.bin",
        "Engine/Intermediate/UbtRuns/generated.bin", "Engine/Plugins/Runtime/Test/Saved/generated.bin",
        ".egstore/Pending/generated.bin", "Engine/Intermediate/ProjectFiles/PrimaryProjectPath.txt",
        "Engine/Intermediate/ProjectFiles/PrimaryProjectName.txt",
        "Engine/Intermediate/Build/Mac/Resources/Info-Editor.Template.plist",
        "Engine/Intermediate/Build/Mac/Resources/Info.Template.plist",
        "Engine/Intermediate/Build/BuildCookRun/StagedBuild_CanopyFoundry.ini",
        "Engine/Intermediate/TargetInfo.json",
        "Engine/Intermediate/Build/BuildRulesProjects/UE5Rules/UE5Rules.csproj",
        "Engine/Intermediate/Build/BuildRulesProjects/UE5ProgramRules/UE5ProgramRules.csproj",
        "Engine/Plugins/ScriptPlugin/Source/ScriptGeneratorUbtPlugin/ScriptGeneratorUbtPlugin.ubtplugin.csproj.props",
        "Engine/Intermediate/Build/Mac/arm64/UnrealEditor/Development/Core/SharedDefinitions.Core.Cpp20.h",
        "Engine/Intermediate/Build/Mac/arm64/UnrealEditor/Development/Core/SharedPCH.Core.Cpp20.h",
        "Engine/Intermediate/Build/Mac/arm64/UnrealEditor/Development/Core/SharedPCH.Core.Cpp20.h.gch.rsp",
    ):
        path = engine / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"generated")
    bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)
    with pytest.raises(build_common.BuildFailure, match="compiler"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", {**tool, "compiler": {"id": "wrong"}})
    config = root / "config/toolchains.json"
    config.write_text(config.read_text() + " ")
    with pytest.raises(build_common.BuildFailure, match="checkout"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)


@pytest.mark.parametrize("relative", (
    "Engine/Intermediate/Build/BuildRules/UE5Rules.dll",
    "Engine/Intermediate/Build/Mac/UnrealGame/Inc/Engine/UHT/Actor.generated.h",
    "Engine/Intermediate/Build/Mac/arm64/UnrealEditor/Development/Core/SharedPCH.Core.Cpp20.h.gch",
    "Engine/Intermediate/Build/Mac/arm64/UnrealEditor/Development/Core/Definitions.h",
))
def test_installed_admission_includes_shipped_intermediate_inputs(tmp_path, monkeypatch, relative):
    root, engine, tool, _, _, _ = installed_fixture(tmp_path, monkeypatch)
    shipped = engine / relative
    shipped.parent.mkdir(parents=True, exist_ok=True)
    shipped.write_bytes(b"shipped build input")
    bootstrap_engine.register_installed_editor(root, engine, "mac-arm64", tool)
    bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)
    shipped.write_bytes(b"modified build input")
    with pytest.raises(build_common.BuildFailure):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)


def test_full_editor_validation_rechecks_selected_dependencies_and_native_tool(tmp_path):
    root = tmp_path / "checkout"
    engine = tmp_path / "source"
    (root / "config").mkdir(parents=True)
    (root / "scripts").mkdir()
    (root / "scripts/bootstrap_engine.py").write_bytes(Path(bootstrap_engine.__file__).read_bytes())
    version = {"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3}
    version_path = engine / "Engine/Build/Build.version"
    version_path.parent.mkdir(parents=True)
    version_path.write_text(json.dumps(version))
    source = {"tag": "5.8.3-release", "commit": "a" * 40,
              "archive_root": "EpicGames-UnrealEngine-" + "a" * 40, "archive_sha256": "b" * 64,
              "dotnet_directory": "10.0"}
    config = root / "config/toolchains.json"
    config.write_text(json.dumps({"unreal": {"engine": {"major": 5, "minor": 8, "patch": 3},
                                             "source": source}}))
    paths = ("Engine/Build/Commit.gitdeps.xml", "Engine/Binaries/Mac/example",
             "Engine/Binaries/DotNET/GitDependencies/osx-arm64/GitDependencies",
             "Engine/Binaries/ThirdParty/DotNet/10.0/mac-arm64/dotnet",
             "Engine/Build/BatchFiles/Mac/Build.sh", "Engine/Build/BatchFiles/RunUAT.sh")
    for name in paths:
        path = engine / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"official")
    content = b"official"
    sha1 = hashlib.sha1(content).hexdigest()
    (engine / ".uedependencies").write_text(
        f'<WorkingManifest><Files><File Name="Engine/Binaries/Mac/example" '
        f'Hash="{sha1}" ExpectedHash="{sha1}" /></Files></WorkingManifest>')
    dependency = bootstrap_engine._dependency_manifest(engine)
    gitdeps_path = paths[2]
    dotnet_path = paths[3]
    tool = {"compiler": {"id": "AppleClang", "binary_sha256": "native"}}
    record = {"schema": 1, "status": "success", "scope": "unreal-full-editor",
              "engine_root": str(engine), "platform": "mac-arm64", "native_toolchain": tool,
              "source": {**source, "build_version": version, "build_version_sha256": build_common.sha256(version_path)},
              "gitdependencies": {"path": gitdeps_path, "sha256": build_common.sha256(engine / gitdeps_path),
                                  "returncode": 0, "filter_sha256": None, **dependency},
              "build_inputs": {paths[1]: build_common.sha256(engine / paths[1])},
              "dotnet": {"path": dotnet_path, "sha256": build_common.sha256(engine / dotnet_path),
                         "version_returncode": 0},
              "inputs": {"config_sha256": build_common.sha256(config),
                         "bootstrap_sha256": build_common.sha256(root / "scripts/bootstrap_engine.py")}}
    boot = root / ".build/engine-bootstrap.json"
    boot.parent.mkdir()
    boot.write_text(json.dumps(record))
    assert bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool) == {
        "boot_path": boot, "installed": False, "dependency_manifest": dependency}
    with pytest.raises(build_common.BuildFailure, match="compiler"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", {"compiler": {"id": "wrong"}})
    (engine / paths[1]).write_bytes(b"changed")
    with pytest.raises(build_common.BuildFailure, match="GitDependencies"):
        bootstrap_engine.validate_editor(root, engine, "mac-arm64", tool)


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
