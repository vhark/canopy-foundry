"""Preflight and package an existing game on a qualified native UE 5.8.1 host."""

import argparse
import json
import os
import platform
import re
import shutil
import sys
from pathlib import Path

try:
    from . import build_common as common, build_core
except ImportError:
    import build_common as common
    import build_core

ROOT = Path(__file__).resolve().parent.parent
PLATFORMS = {"win64": "Win64", "linux-x64": "Linux", "mac-arm64": "Mac"}


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
        if not tc or not sysroot or not Path(tc).is_absolute() or not Path(sysroot).is_absolute() or "v26" not in Path(tc).name or not Path(sysroot).is_dir():
            raise common.BuildFailure("Missing explicitly qualified v26 Linux toolchain/fixed sysroot")
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
    tool = qualified_compiler(root, host, profile)
    config = "Release"
    manifest_path = root / ".build/core" / config.lower() / "canopy-core-manifest.json"
    core = common.load_json(manifest_path)
    validate_core_manifest(core, {"platform": host, "config": config, "compiler": tool["compiler"], "abi": {"crt": profile["crt_release"] if host == "win64" else profile["crt"], "rtti": profile["rtti"], "exceptions": profile["exceptions"]}})
    if not Path(core["library"]["path"]).resolve().is_relative_to((root / ".build/core").resolve()):
        raise common.BuildFailure("core manifest library path is outside the native build tree")
    sdk = core.get("host_sdk", {})
    if host == "mac-arm64":
        if core.get("vcpkg_triplet") != "arm64-osx":
            raise common.BuildFailure("core manifest macOS dependency triplet mismatch")
        if any(sdk.get(key) != value for key, value in tool["qualification"].items()):
            raise common.BuildFailure("core manifest Xcode/SDK mismatch")
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
    return {"project": project, "version_path": version_path, "core_path": manifest_path, "core": core, **tool}


def game_input_checksums(root: Path) -> dict[str, str]:
    generated = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", ".vs"}
    inputs = {}
    for directory, children, filenames in os.walk(root / "game"):
        children[:] = [name for name in children if name not in generated
                       and not name.endswith((".xcodeproj", ".xcworkspace"))]
        for name in filenames:
            path = Path(directory) / name
            if path.is_file():
                inputs[str(path.relative_to(root))] = common.sha256(path)
    return inputs


def build(root: Path, host: str, configuration: str, engine: Path) -> Path:
    archive_base = root / ".build/game" / host / configuration
    for previous in archive_base.glob("uat-*/canopy-game-manifest.json"):
        previous.unlink()
    evidence = preflight(root, host, configuration, engine)
    import tempfile
    archive_base.mkdir(parents=True, exist_ok=True)
    archive = Path(tempfile.mkdtemp(prefix="uat-", dir=archive_base))
    manifest_path = archive / "canopy-game-manifest.json"
    game_inputs = game_input_checksums(root)
    core_inputs = common.input_checksums(root)
    core_hash = common.sha256(evidence["core_path"])
    command = uat_command(engine, evidence["project"], host, configuration, archive)
    common.run(command, cwd=root)
    packaged = [p for p in archive.rglob("*") if p.is_file() and p != manifest_path]
    if not packaged_executable(packaged, host):
        raise common.BuildFailure(f"UAT returned success without a packaged CanopyFoundry executable in {archive}")
    if game_inputs != game_input_checksums(root) or core_inputs != common.input_checksums(root) or core_hash != common.sha256(evidence["core_path"]):
        raise common.BuildFailure("Game or native core inputs changed during package")
    data = {"schema": 1, "scope": "native-game-package", "platform": host, "configuration": configuration, "engine_version_sha256": common.sha256(evidence["version_path"]), "uat_sha256": common.sha256(Path(command[0])), "project_sha256": common.sha256(evidence["project"]), "game_inputs_sha256": game_inputs, "core_manifest_sha256": common.sha256(evidence["core_path"]), "config_sha256": common.sha256(root / "config/toolchains.json"), "compiler": evidence["compiler"], "qualification": evidence["qualification"], "uat_argv": command, "uat_return_code": 0, "outputs_sha256": {str(p.relative_to(archive)): common.sha256(p) for p in packaged}}
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
