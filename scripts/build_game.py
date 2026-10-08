"""Preflight and package an existing game on a qualified native UE 5.8.1 host."""

import argparse
import hashlib
import json
import math
import os
import platform
import re
import shutil
import sys
from pathlib import Path

try:
    from . import build_common as common, build_core, bootstrap_engine
except ImportError:
    import build_common as common
    import build_core
    import bootstrap_engine

ROOT = Path(__file__).resolve().parent.parent
PLATFORMS = {"win64": "Win64", "linux-x64": "Linux", "mac-arm64": "Mac"}
F03_TESTS = frozenset({"Canopy.F03.ManagementDebt", "Canopy.F03.ViewOnly",
                       "Canopy.F03.BoundedHandoff", "Canopy.F03.ViewBindings",
                       "Canopy.F03.MenuFirstOpen", "Canopy.F03.TargetBatchCompletion"})


def editor_command(engine: Path, host: str) -> Path:
    # Epic AutomationTool/AutomationUtils/CommandletUtils.cs:GetEditorCommandletExe.
    return engine / {"mac-arm64": "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor",
                     "linux-x64": "Engine/Binaries/Linux/UnrealEditor",
                     "win64": "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"}[host]


def editor_build_command(engine: Path, project: Path, host: str) -> list[str]:
    relative = {"win64": "Build.bat", "mac-arm64": "Mac/Build.sh", "linux-x64": "Linux/Build.sh"}[host]
    script = engine / "Engine/Build/BatchFiles" / relative
    bootstrap = [] if host == "win64" else ["-buildubt", "-buildscw"]
    return [str(script), "CanopyFoundryEditor", PLATFORMS[host], "Development",
            f"-Project={project}", "-NoHotReload", "-NoUBTMakefiles", *bootstrap]


def read_engine_report(path: Path) -> dict:
    """Epic ForceUTF8 reports include a BOM; repository manifests remain strict UTF-8."""
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        raise common.BuildFailure(f"Invalid or missing engine report {path}: {error}") from error
    if not isinstance(value, dict):
        raise common.BuildFailure(f"Expected engine report object: {path}")
    return value


def validate_automation_report(report: dict) -> None:
    tests = report.get("tests")
    if not isinstance(tests, list) or len(tests) != len(F03_TESTS):
        raise common.BuildFailure("UE automation report does not contain every F03 test")
    if (type(report.get("failed")) is not int or report["failed"] != 0
            or type(report.get("notRun")) is not int or report["notRun"] != 0
            or type(report.get("succeeded")) is not int or report["succeeded"] != len(F03_TESTS)):
        raise common.BuildFailure("F03 automation reported failed, skipped or incomplete tests")
    if {item.get("fullTestPath") for item in tests} != F03_TESTS or any(
            item.get("state") != "Success" or item.get("errors") != 0 for item in tests):
        raise common.BuildFailure("F03 automation cases did not all succeed")


def validate_packaged_report(report: dict, case: str) -> None:
    if (type(report.get("schema")) is not int or report["schema"] != 1
            or report.get("case") != case or report.get("status") != "pass"
            or not isinstance(report.get("engine"), str) or not report["engine"].startswith("5.8.1-")
            or not isinstance(report.get("measurements"), dict)):
        raise common.BuildFailure(f"Invalid packaged qualification report for {case}")
    measurements = report["measurements"]
    if case == "f03-native-room":
        valid = (measurements.get("completedStage") == 4 and measurements.get("second") == 60
                 and type(measurements.get("revision")) is int and measurements["revision"] >= 3
                 and measurements.get("appliedControl") == .75)
    elif case == "b02-cooked-fiducial":
        ports = measurements.get("ports")
        valid = (type(measurements.get("renderVertices")) is int and measurements["renderVertices"] >= 18
                 and type(measurements.get("collisionVertices")) is int and measurements["collisionVertices"] >= 4
                 and measurements.get("collisionUniqueCorners") == 4
                 and measurements.get("collisionRayBlocked") is True
                 and isinstance(ports, list) and len(ports) == 2
                 and all(isinstance(port, dict) and all(
                     isinstance(port.get(key), list) and len(port[key]) == 3
                     and all(type(value) in (int, float) and math.isfinite(value) for value in port[key])
                     for key in ("position", "direction")) for port in ports))
    else:
        raise common.BuildFailure(f"Unknown packaged qualification case: {case}")
    if not valid:
        raise common.BuildFailure(f"Incomplete packaged qualification state for {case}")




def native_platform() -> str:
    return {("Windows", "AMD64"): "win64", ("Linux", "x86_64"): "linux-x64", ("Darwin", "arm64"): "mac-arm64"}.get((platform.system(), platform.machine()), "unsupported")


def arm64_macho(path: Path) -> bool:
    def thin(header: bytes) -> bool:
        if len(header) < 32 or header[:4] not in (bytes.fromhex("cffaedfe"), bytes.fromhex("feedfacf")):
            return False
        endian = "little" if header[:4] == bytes.fromhex("cffaedfe") else "big"
        return int.from_bytes(header[4:8], endian) == 0x0100000c and int.from_bytes(header[12:16], endian) == 2

    with path.open("rb") as stream:
        header = stream.read(32)
        if thin(header):
            return True
        magic = header[:4]
        if magic not in (bytes.fromhex("cafebabe"), bytes.fromhex("cafebabf"), bytes.fromhex("bebafeca"), bytes.fromhex("bfbafeca")):
            return False
        endian = "big" if magic[:2] == bytes.fromhex("cafe") else "little"
        width = 32 if magic in (bytes.fromhex("cafebabf"), bytes.fromhex("bfbafeca")) else 20
        count = int.from_bytes(header[4:8], endian)
        if not 0 < count <= 32:
            return False
        stream.seek(8)
        for _ in range(count):
            arch = stream.read(width)
            if len(arch) != width:
                return False
            if int.from_bytes(arch[:4], endian) != 0x0100000c:
                continue
            offset = int.from_bytes(arch[8:16 if width == 32 else 12], endian)
            if offset + 32 > path.stat().st_size:
                return False
            stream.seek(offset)
            return thin(stream.read(32))
    return False


def packaged_executable(files: list[Path], host: str) -> bool:
    expected_name = {
        "win64": r"CanopyFoundry(?:-Win64-(?:Shipping|Development))?\.exe",
        "linux-x64": r"CanopyFoundry(?:-Linux-(?:Shipping|Development))?",
        "mac-arm64": r"CanopyFoundry",
    }.get(host)
    if expected_name is None:
        return False
    for path in files:
        if not path.is_file() or path.is_symlink() or re.fullmatch(expected_name, path.name) is None:
            continue
        location = path.as_posix()
        if host == "win64" and "/Binaries/Win64/" not in location:
            continue
        if host == "linux-x64" and ("/Binaries/Linux/" not in location or not path.stat().st_mode & 0o111):
            continue
        if host == "mac-arm64" and (".app/Contents/MacOS/CanopyFoundry" not in location or not path.stat().st_mode & 0o111):
            continue
        if host == "mac-arm64":
            if arm64_macho(path):
                return True
            continue
        with path.open("rb") as stream:
            header = stream.read(128)
        if (host == "linux-x64" and len(header) >= 64 and header[:6] == b"\x7fELF\x02\x01"
              and int.from_bytes(header[16:18], "little") in (2, 3)
              and int.from_bytes(header[18:20], "little") == 62):
            return True
        elif host == "win64" and len(header) >= 64 and header[:2] == b"MZ":
            pe = int.from_bytes(header[60:64], "little")
            if 64 <= pe <= path.stat().st_size - 26:
                with path.open("rb") as stream:
                    stream.seek(pe)
                    pe_header = stream.read(26)
                if (pe_header[:4] == b"PE\0\0"
                        and int.from_bytes(pe_header[4:6], "little") == 0x8664
                        and int.from_bytes(pe_header[22:24], "little") & 0x0002
                        and int.from_bytes(pe_header[24:26], "little") == 0x20b):
                    return True
    return False


def validate_core_manifest(manifest: dict, expected: dict) -> None:
    if manifest.get("schema") != 1 or manifest.get("scope") != "standalone-core":
        raise common.BuildFailure("core manifest schema/scope mismatch")
    for field in ("platform", "config"):
        if manifest.get(field) != expected[field]:
            raise common.BuildFailure(f"core manifest {field} mismatch: expected {expected[field]}, got {manifest.get(field)}")
    compiler = manifest.get("compiler", {})
    for field in ("id", "version"):
        if compiler.get(field) != expected["compiler"][field]:
            raise common.BuildFailure(f"core manifest compiler {field} mismatch")
    if not isinstance(compiler.get("binary_sha256"), str) or len(compiler["binary_sha256"]) != 64:
        raise common.BuildFailure("core manifest compiler binary hash missing")
    if "binary_sha256" in expected["compiler"] and compiler["binary_sha256"] != expected["compiler"]["binary_sha256"]:
        raise common.BuildFailure("core manifest compiler binary checksum mismatch")
    if manifest.get("abi") != expected["abi"]:
        raise common.BuildFailure("core manifest abi mismatch (CRT/RTTI/exceptions)")
    library = manifest.get("library", {})
    if not library.get("path") or common.sha256(Path(library["path"])) != library.get("sha256"):
        raise common.BuildFailure("core manifest library checksum mismatch")


def validate_xcode_version(version: str) -> None:
    if not re.search(r"^Xcode 26\.1\.1$", version, re.MULTILINE):
        raise common.BuildFailure(f"Xcode mismatch: required 26.1.1, found {version.strip()}")


def validate_windows_sdk(visual_studio: str, tools: str, sdk: str, requirements: dict) -> None:
    if visual_studio != requirements["visual_studio"] or not tools.startswith(requirements["msvc"] + ".") or not sdk.startswith(requirements["windows_sdk"] + "."):
        raise common.BuildFailure("Windows SDK/VS/MSVC mismatch: require VS 18.0, MSVC 14.50, SDK 10.0.26100")


def qualified_compiler(root: Path, host: str, requirements: dict) -> dict:
    if host == "mac-arm64":
        xcode = common.run(["xcodebuild", "-version"], cwd=root)
        validate_xcode_version(xcode)
        sdk_path = str(Path(common.run(["xcrun", "--sdk", "macosx", "--show-sdk-path"], cwd=root).strip()).resolve())
        sdk = common.run(["xcrun", "--sdk", sdk_path, "--show-sdk-version"], cwd=root).strip()
        compiler_path = Path(common.run(["xcrun", "--sdk", sdk_path, "--find", "clang++"], cwd=root).strip()).resolve()
        identity = common.run([str(compiler_path), "--version"], cwd=root).splitlines()[0]
        compiler_id = "AppleClang"
        qualification = {"xcode": xcode.strip(), "macos_sdk": sdk, "macos_sdk_path": sdk_path}
    elif host == "win64":
        sdk = os.environ.get("WindowsSDKVersion", "").rstrip("\\/")
        tools = os.environ.get("VCToolsVersion", "")
        visual_studio = os.environ.get("VisualStudioVersion", "")
        validate_windows_sdk(visual_studio, tools, sdk, requirements)
        binary = shutil.which("cl.exe")
        if not binary:
            raise common.BuildFailure("MSVC cl.exe missing from qualified developer environment")
        compiler_path = Path(binary).resolve()
        identity = common.run([str(compiler_path), "/?"], cwd=root).splitlines()[0]
        if "19.50" not in identity:
            raise common.BuildFailure(f"MSVC binary mismatch: require compiler 19.50 / toolset 14.50, found {identity}")
        compiler_id = "MSVC"
        qualification = {"visual_studio": visual_studio, "msvc": tools, "windows_sdk": sdk}
    else:
        tc = os.environ.get("CANOPY_LINUX_TOOLCHAIN_ROOT", "")
        sysroot = os.environ.get("CANOPY_LINUX_SYSROOT", "")
        if (not tc or not sysroot or not Path(tc).is_absolute() or not Path(sysroot).is_absolute()
                or Path(tc).name != "x86_64-unknown-linux-gnu"
                or Path(tc).parent.name != requirements["archive"]["root"] or not Path(sysroot).is_dir()):
            raise common.BuildFailure("Missing explicitly qualified v26 Linux toolchain/fixed sysroot")
        if Path(tc).resolve() != Path(sysroot).resolve():
            raise common.BuildFailure("Core and Unreal must use the same fixed Linux sysroot")
        compiler_path = (Path(tc) / "bin/clang++").resolve()
        if not compiler_path.is_file():
            raise common.BuildFailure("Missing v26 clang++ executable")
        identity = common.run([str(compiler_path), "--version"], cwd=root).splitlines()[0]
        if requirements["clang"] not in identity:
            raise common.BuildFailure(f"Linux clang mismatch: require {requirements['clang']}, found {identity}")
        actual_sysroot = str(Path(sysroot).resolve())
        compiler_id = "Clang"
        qualification = {"toolchain_root": str(Path(tc).resolve()), "sysroot": actual_sysroot, **common.linux_libcxx()}
    return {"compiler": {"id": compiler_id, "version": identity, "binary_sha256": common.sha256(compiler_path)}, "qualification": qualification}


def uat_command(engine: Path, project: Path, platform_name: str, configuration: str, archive: Path) -> list[str]:
    script = engine / "Engine/Build/BatchFiles" / ("RunUAT.bat" if platform_name == "win64" else "RunUAT.sh")
    return [str(script), "BuildCookRun", f"-project={project}", "-noP4", "-unattended", "-utf8output", "-build", "-cook", "-stage", "-pak", "-package", "-archive", f"-archivedirectory={archive}", f"-platform={PLATFORMS[platform_name]}", f"-clientconfig={configuration}"]


def preflight(root: Path, host: str, configuration: str, engine: Path) -> dict:
    if host not in PLATFORMS or native_platform() != host:
        raise common.BuildFailure(f"Requested {host} is not the native host ({native_platform()})")
    if configuration not in ("Development", "Shipping"):
        raise common.BuildFailure(f"Unsupported game configuration {configuration}")
    project = root / "game/CanopyFoundry.uproject"
    if not project.is_file():
        raise common.BuildFailure(f"Missing game project {project}; cannot package a nonexistent project")
    common.load_json(project)
    expected_engine = common.load_json(root / "config/toolchains.json")["unreal"]
    version_path = engine / "Engine/Build/Build.version"
    version = common.load_json(version_path)
    if any(version.get(k) != expected_engine["engine"][key] for k, key in (("MajorVersion", "major"), ("MinorVersion", "minor"), ("PatchVersion", "patch"))):
        raise common.BuildFailure(f"Unreal engine must be 5.8.1; found {version}")
    uat = Path(uat_command(engine, project, host, configuration, root / ".build/game")[0])
    if not uat.is_file():
        raise common.BuildFailure(f"Missing official Unreal Automation Tool {uat}")
    profile = expected_engine[host]
    bootstrap_path = root / ".build/engine-bootstrap.json"
    boot = common.load_json(bootstrap_path)
    source = expected_engine["source"]
    if (boot.get("scope") != "unreal-full-editor" or boot.get("status") != "success"
            or boot.get("engine_root") != str(engine.resolve())
            or boot.get("platform") != ("win-x64" if host == "win64" else host)
            or any(boot.get("source", {}).get(k) != source[k] for k in
                   ("tag", "commit", "archive_root", "archive_sha256"))
            or boot.get("source", {}).get("build_version_sha256") != common.sha256(version_path)
            or boot.get("inputs", {}).get("config_sha256") != common.sha256(root / "config/toolchains.json")
            or boot.get("inputs", {}).get("bootstrap_sha256") != common.sha256(root / "scripts/bootstrap_engine.py")):
        raise common.BuildFailure("Game requires this checkout's pinned full-editor bootstrap evidence")
    deps = boot.get("gitdependencies", {})
    manifest = engine / "Engine/Build/Commit.gitdeps.xml"
    working = engine / ".uedependencies"
    gitdeps = engine / deps.get("path", "")
    if (deps.get("original_manifests_sha256", {}).get("Engine/Build/Commit.gitdeps.xml") != common.sha256(manifest)
            or deps.get("working_manifest_sha256") != common.sha256(working)
            or not isinstance(deps.get("selected_file_count"), int) or deps["selected_file_count"] < 1
            or deps.get("filter_sha256") is not None or (engine / ".gitdepsignore").exists()
            or deps.get("returncode") != 0 or deps.get("sha256") != common.sha256(gitdeps)):
        raise common.BuildFailure("Official full-editor dependency manifest/client changed")
    dependency_manifest = bootstrap_engine._dependency_manifest(engine)
    if any(deps.get(key) != value for key, value in dependency_manifest.items()):
        raise common.BuildFailure("Full-editor selected dependency payloads changed")
    editor = editor_command(engine, host)
    build_script = Path(editor_build_command(engine, project, host)[0])
    if not build_script.is_file():
        raise common.BuildFailure("Official Editor build script missing")
    bundled = engine / boot.get("dotnet", {}).get("path", "")
    if (boot.get("dotnet", {}).get("sha256") != common.sha256(bundled)
            or boot.get("dotnet", {}).get("version_returncode") != 0):
        raise common.BuildFailure("Provisioned Editor .NET SDK changed")
    tool = qualified_compiler(root, host, profile)
    if boot.get("native_toolchain") != tool:
        raise common.BuildFailure("Native compiler differs from full-editor provision")
    config = "Release"
    manifest_path = root / ".build/core" / config.lower() / "canopy-core-manifest.json"
    core = common.load_json(manifest_path)
    validate_core_manifest(core, {"platform": host, "config": config, "compiler": tool["compiler"], "abi": {"crt": profile["crt_release"] if host == "win64" else profile["crt"], "rtti": profile["rtti"], "exceptions": profile["exceptions"]}})
    if not Path(core["library"]["path"]).resolve().is_relative_to((root / ".build/core").resolve()):
        raise common.BuildFailure("core manifest library path is outside the native build tree")
    sdk = core.get("host_sdk", {})
    if host == "mac-arm64":
        if core.get("vcpkg_triplet") != profile["qualified_triplet"]:
            raise common.BuildFailure("core manifest macOS dependency triplet mismatch")
        if core.get("deployment_target") != profile["deployment_target"]:
            raise common.BuildFailure("core manifest macOS deployment target mismatch")
        if any(sdk.get(key) != value for key, value in tool["qualification"].items()):
            raise common.BuildFailure("core manifest Xcode/SDK mismatch")
    if host == "win64" and core.get("vcpkg_triplet") != profile["qualified_triplet"]:
        raise common.BuildFailure("core manifest Windows dependency triplet mismatch")
    if host == "win64" and (sdk.get("visual_studio") != tool["qualification"]["visual_studio"] or sdk.get("windows_sdk", "").rstrip("\\/") != tool["qualification"]["windows_sdk"] or sdk.get("msvc") != tool["qualification"]["msvc"]):
        raise common.BuildFailure("core manifest Windows SDK/MSVC mismatch")
    if host == "linux-x64":
        if core.get("vcpkg_triplet") != profile["qualified_triplet"]:
            raise common.BuildFailure("core manifest Linux dependency triplet mismatch")
        if any(sdk.get(key) != value for key, value in tool["qualification"].items() if key != "toolchain_root"):
            raise common.BuildFailure("core manifest Linux sysroot/libc++ artifacts mismatch")
    # Bootstrap evidence and the complete core input set must still describe this checkout.
    bootstrap = build_core.preflight(root, "Release")
    if core.get("bootstrap_sha256") != bootstrap["bootstrap_sha256"] or core.get("vcpkg_commit") != bootstrap["vcpkg_commit"]:
        raise common.BuildFailure("core manifest bootstrap evidence mismatch")
    if core.get("inputs_sha256") != common.input_checksums(root):
        raise common.BuildFailure("core manifest stale or incomplete source input checksums")
    return {"project": project, "version_path": version_path, "core_path": manifest_path,
            "core": core, "boot_path": bootstrap_path, "editor": editor,
            "dependency_manifest": dependency_manifest, **tool}


def game_input_checksums(root: Path, *, include_authored_outputs: bool = True) -> dict[str, str]:
    generated = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", ".vs"}
    inputs = {}
    for directory, children, filenames in os.walk(root / "game"):
        children[:] = [name for name in children if name not in generated
                       and not name.endswith((".xcodeproj", ".xcworkspace"))]
        for name in filenames:
            path = Path(directory) / name
            relative = path.relative_to(root / "game")
            if not include_authored_outputs and (
                    relative == Path("Content/Maps/FacilityQualification.umap")
                    or relative.is_relative_to(Path("Content/B02"))):
                continue
            if path.is_file():
                inputs[str(path.relative_to(root))] = common.sha256(path)
    return inputs


def build(root: Path, host: str, configuration: str, engine: Path) -> Path:
    archive_base = root / ".build/game" / host / configuration
    evidence = preflight(root, host, configuration, engine)
    for previous in archive_base.glob("uat-*/canopy-game-manifest.json"):
        previous.unlink()
    import tempfile
    archive_base.mkdir(parents=True, exist_ok=True)
    archive = Path(tempfile.mkdtemp(prefix="uat-", dir=archive_base))
    manifest_path = archive / "canopy-game-manifest.json"
    game_sources = game_input_checksums(root, include_authored_outputs=False)
    core_inputs = common.input_checksums(root)
    core_hash = common.sha256(evidence["core_path"])
    boot_hash = common.sha256(evidence["boot_path"])
    fixture_sources = [root / "scripts/generate_cooked_fiducial.py",
                       *sorted((root / "apps/canopy-author/src").rglob("*.py"))]
    fixture_source_hashes = {str(path.relative_to(root)): common.sha256(path) for path in fixture_sources}
    environment = os.environ.copy()
    environment.pop("CANOPY_UE_SOURCE_ARCHIVE_URL", None)
    dotnet = engine / common.load_json(evidence["boot_path"])["dotnet"]["path"]
    environment["DOTNET_ROOT"] = str(dotnet.parent)
    environment["PATH"] = str(dotnet.parent) + os.pathsep + environment.get("PATH", "")
    environment["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"
    environment["DOTNET_GENERATE_ASPNET_CERTIFICATE"] = "false"
    if host == "linux-x64":
        environment["LINUX_MULTIARCH_ROOT"] = str(Path(environment["CANOPY_LINUX_TOOLCHAIN_ROOT"]).resolve().parent)
    if host == "win64":
        environment["CANOPY_UBT_MSVC_VERSION"] = evidence["qualification"]["msvc"].rstrip("\\/")
        environment["CANOPY_UBT_WINDOWS_SDK"] = evidence["qualification"]["windows_sdk"].rstrip("\\/")
    build_command = editor_build_command(engine, evidence["project"], host)
    common.run(build_command, cwd=root, env=environment, capture=False)
    editor = evidence["editor"]
    if not editor.is_file():
        raise common.BuildFailure("Editor build returned without its native executable")
    editor_hash = common.sha256(editor)
    map_path = root / "game/Content/Maps/FacilityQualification.umap"
    map_path.unlink(missing_ok=True)
    commandlet = [str(editor), str(evidence["project"]), "-run=FacilityRoom", "-unattended", "-nop4", "-nullrhi"]
    common.run(commandlet, cwd=root, env=environment, capture=False)
    if not map_path.is_file() or not map_path.stat().st_size:
        raise common.BuildFailure("FacilityRoom commandlet did not author the required native map")
    fixture_dir = archive / "fiducial-input"
    generate = ["uv", "run", "--frozen", "--package", "canopy-author", "python",
                str(root / "scripts/generate_cooked_fiducial.py"), "--output", str(fixture_dir)]
    common.run(generate, cwd=root, env=environment, capture=False)
    fixture_hashes = {str(path.relative_to(fixture_dir)): common.sha256(path)
                      for path in sorted(fixture_dir.iterdir()) if path.is_file()}
    imported = [str(editor), str(evidence["project"]), "-run=CookedFiducialImport",
                f"-FixtureDir={fixture_dir}", "-unattended", "-nop4", "-nullrhi"]
    common.run(imported, cwd=root, env=environment, capture=False)
    map_hash = common.sha256(map_path)
    game_inputs = game_input_checksums(root)
    if game_sources != game_input_checksums(root, include_authored_outputs=False):
        raise common.BuildFailure("Game source inputs changed during Editor build/map authoring")
    report_dir = archive / "automation"
    automation = [str(editor), str(evidence["project"]), "-unattended", "-nop4", "-nullrhi",
                  f"-ReportExportPath={report_dir}",
                  "-ExecCmds=Automation RunTests Canopy.F03", "-testexit=Automation Test Queue Empty"]
    common.run(automation, cwd=root, env=environment, capture=False)
    report = report_dir / "index.json"
    validate_automation_report(read_engine_report(report))
    command = uat_command(engine, evidence["project"], host, configuration, archive)
    common.run(command, cwd=root, env=environment, capture=False)
    packaged = [p for p in archive.rglob("*") if p.is_file()
                and not p.is_relative_to(report_dir) and not p.is_relative_to(fixture_dir)]
    executables = sorted((p for p in packaged if packaged_executable([p], host)), key=str)
    if len(executables) != 1:
        raise common.BuildFailure(f"UAT did not produce exactly one native CanopyFoundry executable in {archive}")
    native_runs = {}
    for case, mode in (("f03-native-room", "-FacilityQualify"),
                       ("b02-cooked-fiducial", "-CookedFiducialQualify")):
        result_path = archive / f"{case}.json"
        native = [str(executables[0]), "/Game/Maps/FacilityQualification", mode,
                  f"-QualificationReport={result_path}", "-unattended",
                  "-nullrhi" if case == "f03-native-room" else "-RenderOffscreen",
                  "-stdout", "-FullStdOutLogOutput"]
        native_output = common.run(native, cwd=executables[0].parent, env=environment)
        validate_packaged_report(read_engine_report(result_path), case)
        native_runs[case] = {"argv": native, "returncode": 0, "report_sha256": common.sha256(result_path),
                             "stdout_sha256": hashlib.sha256(native_output.encode("utf-8")).hexdigest()}
    if (game_inputs != game_input_checksums(root) or core_inputs != common.input_checksums(root)
            or core_hash != common.sha256(evidence["core_path"]) or boot_hash != common.sha256(evidence["boot_path"])
            or editor_hash != common.sha256(editor) or map_hash != common.sha256(map_path)
            or fixture_source_hashes != {str(path.relative_to(root)): common.sha256(path) for path in fixture_sources}
            or fixture_hashes != {str(path.relative_to(fixture_dir)): common.sha256(path)
                                 for path in sorted(fixture_dir.iterdir()) if path.is_file()}
            or bootstrap_engine._dependency_manifest(engine) != evidence["dependency_manifest"]):
        raise common.BuildFailure("Game, engine, or native core inputs changed during package")
    data = {"schema": 1, "scope": "native-game-package", "platform": host, "configuration": configuration,
            "engine_version_sha256": common.sha256(evidence["version_path"]),
            "engine_bootstrap_sha256": boot_hash, "editor_sha256": editor_hash,
            "uat_sha256": common.sha256(Path(command[0])), "project_sha256": common.sha256(evidence["project"]),
            "game_inputs_sha256": game_inputs, "core_manifest_sha256": core_hash,
            "config_sha256": common.sha256(root / "config/toolchains.json"), "compiler": evidence["compiler"],
            "qualification": evidence["qualification"], "editor_build_argv": build_command,
            "room_commandlet_argv": commandlet, "map_sha256": map_hash, "automation_argv": automation,
            "automation_report_sha256": common.sha256(report), "uat_argv": command,
            "fiducial_generator_argv": generate, "fiducial_import_argv": imported,
            "fiducial_sources_sha256": fixture_source_hashes, "fiducial_inputs_sha256": fixture_hashes,
            "packaged_qualifications": native_runs,
            "child_return_codes": {"editor_build": 0, "room_commandlet": 0, "automation": 0,
                                   "fiducial_generator": 0, "fiducial_import": 0, "uat": 0},
            "outputs_sha256": {str(p.relative_to(archive)): common.sha256(p) for p in packaged}}
    common.write_manifest(manifest_path, data)
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=tuple(PLATFORMS), required=True)
    parser.add_argument("--configuration", choices=("Development", "Shipping"), required=True)
    parser.add_argument("--engine-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(build(ROOT, args.platform, args.configuration, args.engine_root.resolve()))
        return 0
    except (common.BuildFailure, OSError, KeyError, ValueError) as error:
        print(f"Game build failed: {error}", file=sys.stderr)
        return error.code if isinstance(error, common.BuildFailure) else 1


if __name__ == "__main__":
    sys.exit(main())
