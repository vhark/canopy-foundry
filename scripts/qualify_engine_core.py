"""Qualify the Release standalone core archive with a real UE 5.8.1 UBT Program."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import sys

try:
    from . import build_common as common, build_core, build_game
except ImportError:
    import build_common as common
    import build_core
    import build_game

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / "tests/engine-core"
NAME = "CanopyCoreQualification"


def compiler_version(version: str, compiler_id: str) -> str:
    if compiler_id == "MSVC":
        match = re.search(r"\bVersion (\d+\.\d+\.\d+)\b", version)
    elif compiler_id == "AppleClang":
        match = re.search(r"\bApple clang version (\d+\.\d+\.\d+)\b", version)
    elif compiler_id == "Clang":
        match = re.search(r"\bclang version (\d+\.\d+\.\d+)\b", version)
    else:
        raise common.BuildFailure("Unsupported core compiler identity")
    if not match:
        raise common.BuildFailure("Cannot resolve actual core compiler version")
    return match.group(1)


def _reject_constant(value: str) -> None:
    raise ValueError(f"Non-finite JSON value: {value}")


def _unique_fields(pairs: list[tuple[str, object]]) -> dict:
    fields = dict(pairs)
    if len(fields) != len(pairs):
        raise ValueError("Duplicate JSON fields")
    return fields


def validate_consumer_output(output: str, core_manifest: dict) -> dict:
    try:
        record = json.loads(output, parse_constant=_reject_constant, object_pairs_hook=_unique_fields)
        compiler = core_manifest["compiler"]
        abi = core_manifest["abi"]
        expected = {
            "platform": core_manifest["platform"], "cpp_version": 202002,
            "pointer_bits": 64, "rtti": abi["rtti"], "exceptions": abi["exceptions"],
            "crt": abi["crt"], "compiler_id": compiler["id"],
            "compiler_version": compiler_version(compiler["version"], compiler["id"]),
        }
        if not isinstance(record, dict) or set(record) != set(expected) | {"area_square_metres"}:
            raise ValueError("missing, extra, or malformed consumer fields")
        area = record["area_square_metres"]
        if type(area) not in (int, float) or not math.isfinite(area) or not math.isclose(area, 5574.1824, rel_tol=0, abs_tol=1e-7):
            raise ValueError("core conversion is incorrect or non-finite")
        for field, value in expected.items():
            if type(record[field]) is not type(value) or record[field] != value:
                raise ValueError(f"native ABI measurement mismatch: {field}")
    except (ValueError, KeyError, TypeError) as error:
        raise common.BuildFailure(f"Invalid native core consumer output: {error}") from error
    return record




def select_core(root: Path) -> tuple[dict, Path]:
    manifest_path = root / ".build/core/release/canopy-core-manifest.json"
    core = common.load_json(manifest_path)
    host = build_core.host_platform()
    profile = common.load_json(root / "config/toolchains.json")["unreal"][host]
    tool = build_game.qualified_compiler(root, host, profile)
    build_game.validate_core_manifest(core, {
        "platform": host, "config": "Release", "compiler": tool["compiler"],
        "abi": {"crt": profile["crt_release"] if host == "win64" else profile["crt"],
                "rtti": profile["rtti"], "exceptions": profile["exceptions"]},
    })
    if core.get("unreal_qualified") is not False:
        raise common.BuildFailure("Expected original standalone core provenance")
    library = Path(core["library"]["path"])
    if not library.is_absolute() or not library.resolve().is_relative_to((root / ".build/core").resolve()):
        raise common.BuildFailure("Selected core library is outside the native build tree")
    current = build_core.preflight(root, "Release")
    if (core.get("inputs_sha256") != common.input_checksums(root)
            or any(core.get(key) != current[key] for key in ("bootstrap_sha256", "vcpkg_commit", "tools"))
            or core["compiler"]["binary_sha256"] != common.sha256(Path(core["compiler"]["path"]))):
        raise common.BuildFailure("Standalone core inputs, compiler or pinned tools changed")
    triplet = profile["qualified_triplet"]
    if core.get("vcpkg_triplet") != triplet:
        raise common.BuildFailure("Core dependency triplet differs from approved profile")
    sdk = core["host_sdk"]
    for key, value in tool["qualification"].items():
        if key == "toolchain_root":
            continue
        observed = sdk.get(key)
        if host == "win64" and key == "windows_sdk" and isinstance(observed, str):
            observed = observed.rstrip("\\/")
        if observed != value:
            raise common.BuildFailure(f"Selected SDK differs from core: {key}")
    if host == "mac-arm64":
        developer = Path(common.run(["xcode-select", "-p"], cwd=root).strip()).resolve()
        if (not Path(sdk["macos_sdk_path"]).resolve().is_relative_to(developer)
                or core.get("deployment_target") != profile["deployment_target"]):
            raise common.BuildFailure("Selected Apple developer directory or deployment target changed")
    return core, manifest_path




def receipt_executable(project: Path, config: str, host: str) -> tuple[Path, Path]:
    platform = {"mac-arm64": "Mac", "linux-x64": "Linux", "win64": "Win64"}[host]
    receipts = list((project / "Binaries" / platform).glob("*.target"))
    matched = [(path, common.load_json(path)) for path in receipts]
    matched = [(path, data) for path, data in matched if data.get("TargetName") == NAME and data.get("Configuration") == config]
    if len(matched) != 1:
        raise common.BuildFailure("Missing or ambiguous UBT target receipt")
    receipt, data = matched[0]
    if data.get("TargetType") != "Program" or data.get("Platform") != platform:
        raise common.BuildFailure("UBT receipt does not describe the requested Program")
    launch = data.get("Launch")
    if not isinstance(launch, str) or not launch.startswith("$(ProjectDir)/"):
        raise common.BuildFailure("UBT receipt has no project-local launch executable")
    executable = (project / launch[len("$(ProjectDir)/"):]).resolve()
    if (not executable.is_relative_to(project.resolve()) or not executable.is_file()
            or not any(item.get("Type") == "Executable" and item.get("Path") == launch
                       for item in data.get("BuildProducts", []))):
        raise common.BuildFailure("UBT receipt launch is not a produced executable")
    return receipt, executable


def qualify(engine_root: Path) -> list[Path]:
    output_root = ROOT / ".build/engine-qualification" / build_core.host_platform()
    for configuration in ("Development", "Shipping"):
        (output_root / configuration / "manifest.json").unlink(missing_ok=True)
    if not engine_root.is_absolute() or not engine_root.is_dir():
        raise common.BuildFailure("--engine-root must be an installed absolute source path")
    engine_root = engine_root.resolve()
    requirements = common.load_json(ROOT / "config/toolchains.json")["unreal"]
    approved = requirements["source"]
    boot_path = ROOT / ".build/engine-bootstrap.json"
    boot = common.load_json(boot_path)
    if (boot.get("schema") != 1 or boot.get("status") != "success" or boot.get("scope") != "unreal-build-tools"
            or boot.get("engine_root") != str(engine_root)
            or boot.get("platform") != ("win-x64" if build_core.host_platform() == "win64" else build_core.host_platform())
            or any(boot.get("source", {}).get(key) != approved[key]
                   for key in ("tag", "commit", "archive_root", "archive_sha256"))
            or boot.get("inputs", {}).get("config_sha256") != common.sha256(ROOT / "config/toolchains.json")
            or boot.get("inputs", {}).get("bootstrap_sha256") != common.sha256(ROOT / "scripts/bootstrap_engine.py")):
        raise common.BuildFailure("Engine bootstrap provenance is missing or changed")
    dependencies = boot["gitdependencies"]
    gitdeps_path = (engine_root / dependencies["path"]).resolve()
    if (not gitdeps_path.is_relative_to(engine_root)
            or common.sha256(gitdeps_path) != dependencies["sha256"]
            or common.sha256(engine_root / ".gitdepsignore") != dependencies["filter_sha256"]
            or dependencies.get("returncode") != 0):
        raise common.BuildFailure("Official engine dependency client or selected filter changed")
    version_path = engine_root / "Engine/Build/Build.version"
    version = common.load_json(version_path)
    if (boot["source"].get("build_version") != version
            or boot["source"].get("build_version_sha256") != common.sha256(version_path)
            or tuple(version.get(key) for key in ("MajorVersion", "MinorVersion", "PatchVersion"))
            != tuple(requirements["engine"][key] for key in ("major", "minor", "patch"))):
        raise common.BuildFailure("Installed engine Build.version differs from approved source")
    dotnet = (engine_root / boot["dotnet"]["path"]).resolve()
    if (not dotnet.is_relative_to(engine_root) or boot["dotnet"]["sha256"] != common.sha256(dotnet)
            or not boot["dotnet"]["version"].startswith(requirements["source"]["dotnet_directory"] + ".")
            or boot["dotnet"].get("info_returncode") != 0 or boot["dotnet"].get("version_returncode") != 0):
        raise common.BuildFailure("Bundled approved .NET SDK changed or lacks bootstrap proof")
    core, core_path = select_core(ROOT)
    core_manifest_hash = common.sha256(core_path)
    qualifier_hash = common.sha256(Path(__file__))
    host = core["platform"]
    fixture_hashes = {str(p.relative_to(FIXTURE)): common.sha256(p) for p in sorted(FIXTURE.rglob("*")) if p.is_file()}
    if set(fixture_hashes) != {f"{NAME}Consumer.uproject", f"Source/{NAME}.Target.cs",
                               f"Source/{NAME}/{NAME}.Build.cs", f"Source/{NAME}/Private/Main.cpp"}:
        raise common.BuildFailure("Qualification fixture has unexpected inputs")
    ubt_project = engine_root / "Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj"
    ubt = engine_root / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
    ubt_project_hash = common.sha256(ubt_project)
    bootstrap_hash = common.sha256(boot_path)
    environment = os.environ.copy()
    environment.pop("CANOPY_UE_SOURCE_ARCHIVE_URL", None)
    environment["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"
    environment["DOTNET_GENERATE_ASPNET_CERTIFICATE"] = "false"
    environment["DOTNET_ROOT"] = str(dotnet.parent)
    environment["PATH"] = str(dotnet.parent) + os.pathsep + environment.get("PATH", "")
    environment["CANOPY_CORE_INCLUDE"] = str(ROOT / "core/include")
    environment["CANOPY_CORE_LIBRARY"] = core["library"]["path"]
    if host == "win64":
        environment["CANOPY_UBT_MSVC_VERSION"] = core["host_sdk"]["msvc"].strip("\\/")
        environment["CANOPY_UBT_WINDOWS_SDK"] = core["host_sdk"]["windows_sdk"].strip("\\/")
    elif host == "linux-x64":
        architecture = Path(os.environ["CANOPY_LINUX_TOOLCHAIN_ROOT"]).resolve()
        if architecture.name != "x86_64-unknown-linux-gnu":
            raise common.BuildFailure("Qualified Linux toolchain is not the expected multiarch target")
        environment["LINUX_MULTIARCH_ROOT"] = str(architecture.parent)
    ubt_command = [str(dotnet), "build", str(ubt_project), "-c", "Development", "-v", "quiet"]
    common.run(ubt_command, cwd=engine_root, env=environment)
    if not ubt.is_file():
        raise common.BuildFailure("Official UBT build did not produce UnrealBuildTool.dll")
    ubt_hash = common.sha256(ubt)
    results = []
    for config in ("Development", "Shipping"):
        destination = output_root / config
        success = destination / "manifest.json"
        project = destination / "project"
        if project.exists():
            shutil.rmtree(project)
        shutil.copytree(FIXTURE, project)
        copied = {str(p.relative_to(project)): common.sha256(p) for p in project.rglob("*") if p.is_file()}
        if copied != fixture_hashes:
            raise common.BuildFailure("Copied qualification fixture differs from original")
        # UE 5.8 keeps Program receipts project-local only when the project
        # descriptor basename differs from the target (UEBuildTarget.cs:1924-1937).
        working_descriptor = project / (NAME + "Consumer.uproject")
        args = [str(dotnet), str(ubt), NAME, {"mac-arm64": "Mac", "win64": "Win64", "linux-x64": "Linux"}[host], config,
                f"-Project={working_descriptor}", f"-Architecture={'arm64' if host == 'mac-arm64' else 'x64'}",
                "-NoUBA", "-NoHotReload", "-NoUBTMakefiles"]
        common.run(args, cwd=engine_root, env=environment)
        receipt, executable = receipt_executable(project, config, host)
        native_output = common.run([str(executable)], cwd=project, env=environment)
        measured = validate_consumer_output(native_output, core)
        executable_evidence = {"path": str(executable), "sha256": common.sha256(executable)}
        if host == "mac-arm64":
            macho = common.run(["xcrun", "--sdk", core["host_sdk"]["macos_sdk_path"],
                                "vtool", "-show-build", str(executable)], cwd=project, env=environment)
            minimum_os = re.findall(r"^\s*minos\s+([0-9.]+)\s*$", macho, re.MULTILINE)
            if minimum_os != [core["deployment_target"]]:
                raise common.BuildFailure("Engine consumer macOS deployment target differs from core")
            executable_evidence["mach_o_build_version"] = macho
        if (common.input_checksums(ROOT) != core["inputs_sha256"]
                or common.sha256(core_path) != core_manifest_hash
                or common.sha256(Path(core["library"]["path"])) != core["library"]["sha256"]
                or common.sha256(Path(core["compiler"]["path"])) != core["compiler"]["binary_sha256"]
                or common.sha256(ubt) != ubt_hash or common.sha256(ubt_project) != ubt_project_hash
                or common.sha256(dotnet) != boot["dotnet"]["sha256"]
                or common.sha256(boot_path) != bootstrap_hash
                or common.sha256(Path(__file__)) != qualifier_hash
                or common.sha256(version_path) != boot["source"]["build_version_sha256"]
                or common.sha256(working_descriptor) != fixture_hashes[NAME + "Consumer.uproject"]
                or any(common.sha256(project / relative) != digest for relative, digest in fixture_hashes.items())
                or {str(p.relative_to(FIXTURE)): common.sha256(p) for p in FIXTURE.rglob("*") if p.is_file()} != fixture_hashes):
            raise common.BuildFailure("Qualification source, build tool, or library changed during execution")
        evidence = {"schema": 1, "status": "success", "scope": "unreal-core-consumer", "platform": host,
                    "configuration": config, "engine": {"root": str(engine_root), "source": boot["source"],
                    "build_version": version, "build_version_sha256": common.sha256(version_path),
                    "ubt_project": {"path": str(ubt_project), "sha256": ubt_project_hash},
                    "ubt": {"path": str(ubt), "sha256": ubt_hash}, "bootstrap": {"path": str(boot_path), "sha256": bootstrap_hash}},
                    "dotnet": boot["dotnet"], "compiler": core["compiler"], "host_sdk": core["host_sdk"],
                    "core": {"manifest_path": str(core_path), "manifest_sha256": core_manifest_hash, "library": core["library"], "scope": core["scope"]},
                    "fixture_sha256": fixture_hashes, "working_descriptor": {"path": str(working_descriptor), "sha256": common.sha256(working_descriptor)},
                    "receipt": {"path": str(receipt), "sha256": common.sha256(receipt)},
                    "executable": executable_evidence, "qualifier_sha256": qualifier_hash,
                    "commands": {"ubt_build": ubt_command, "ubt": args, "native": [str(executable)]},
                    "output": measured, "native_stdout": native_output,
                    "child_return_codes": {"ubt_build": 0, "ubt": 0, "native": 0}}
        common.write_manifest(success, evidence)
        results.append(success)
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        for result in qualify(args.engine_root):
            print(result)
        return 0
    except (common.BuildFailure, OSError, ValueError, KeyError, TypeError) as error:
        print(f"Engine qualification failed: {error}", file=sys.stderr)
        return error.code if isinstance(error, common.BuildFailure) else 1


if __name__ == "__main__":
    raise SystemExit(main())
