"""Build the standalone C++20 domain library (not Unreal-qualified by default)."""

import json
import shlex
import argparse
import os
import platform
import re
import shutil
import sys
from pathlib import Path

try:
    from . import build_common as common
except ImportError:
    import build_common as common

ROOT = Path(__file__).resolve().parent.parent


def host_platform() -> str:
    return {("Darwin", "arm64"): "mac-arm64", ("Linux", "x86_64"): "linux-x64", ("Windows", "AMD64"): "win64"}.get((platform.system(), platform.machine()), "unsupported")


def preflight(root: Path, config: str) -> dict:
    if config not in ("Debug", "Release"):
        raise common.BuildFailure(f"Unsupported core configuration: {config}")
    profile = common.load_json(root / "config/toolchains.json")["standalone"]
    if host_platform() == "unsupported":
        raise common.BuildFailure("Unsupported native host")
    lock = root / "dependencies/native-lock.json"
    if not lock.is_file() or not (root / "uv.lock").is_file():
        raise common.BuildFailure("Missing native or uv lock; run pinned bootstrap")
    commit = common.load_json(lock).get("vcpkg", {}).get("commit")
    if commit != profile["vcpkg_commit"]:
        raise common.BuildFailure("vcpkg lock commit mismatch")
    checkout = root / ".work/tools/vcpkg"
    toolchain = checkout / "scripts/buildsystems/vcpkg.cmake"
    if not toolchain.is_file():
        raise common.BuildFailure("Missing pinned vcpkg bootstrap: uv run --frozen python scripts/bootstrap_native.py")
    actual = common.run(["git", "-C", str(checkout), "rev-parse", "HEAD"], cwd=root).strip()
    if actual != commit:
        raise common.BuildFailure(f"vcpkg checkout mismatch: expected {commit}, got {actual}")
    if common.run(["git", "-C", str(checkout), "status", "--porcelain", "--untracked-files=all"], cwd=root).strip():
        raise common.BuildFailure("Dirty pinned vcpkg checkout")
    if common.run(["git", "-C", str(checkout), "ls-files", "--others", "--ignored", "--exclude-standard", "--", "triplets"], cwd=root).strip():
        raise common.BuildFailure("Unapproved custom vcpkg triplets")
    bootstrap = root / ".build/native-bootstrap.json"
    if not bootstrap.is_file():
        raise common.BuildFailure("Missing vcpkg bootstrap evidence")
    boot = common.load_json(bootstrap)
    if boot.get("status") != "success" or boot.get("source", {}).get("commit") != commit:
        raise common.BuildFailure("Invalid pinned vcpkg bootstrap evidence")
    boot_inputs = {
        str(Path(relative)): common.sha256(root / relative)
        for relative in ("dependencies/native-lock.json", "dependencies/vcpkg.json",
                         "dependencies/vcpkg-configuration.json", "scripts/bootstrap_native.py")
    }
    if boot.get("inputs") != boot_inputs:
        raise common.BuildFailure("Pinned vcpkg bootstrap input checksum mismatch; rebootstrap")
    source = boot.get("source", {})
    if source.get("repository") != "https://github.com/microsoft/vcpkg.git" or source["repository"] != common.run(["git", "-C", str(checkout), "remote", "get-url", "origin"], cwd=root).strip():
        raise common.BuildFailure("Pinned vcpkg origin mismatch")
    if source.get("tree") != common.run(["git", "-C", str(checkout), "rev-parse", "HEAD^{tree}"], cwd=root).strip():
        raise common.BuildFailure("Pinned vcpkg source tree mismatch")
    boot_script = checkout / ("bootstrap-vcpkg.bat" if host_platform() == "win64" else "bootstrap-vcpkg.sh")
    if source.get("bootstrap_script_sha256") != common.sha256(boot_script):
        raise common.BuildFailure("Pinned vcpkg bootstrap script checksum mismatch")
    lock_ports = common.load_json(lock)["ports"]
    if set(boot.get("ports", {})) != set(lock_ports):
        raise common.BuildFailure("Pinned vcpkg port evidence set mismatch")
    for name, pinned in lock_ports.items():
        port = checkout / "ports" / name
        recorded = boot.get("ports", {}).get(name, {})
        if recorded.get("portfile_sha256") != common.sha256(port / "portfile.cmake") or recorded.get("metadata_sha256") != common.sha256(port / "vcpkg.json"):
            raise common.BuildFailure(f"Pinned vcpkg port checksum mismatch: {name}")
        if recorded.get("source_sha512") != pinned["source_sha512"]:
            raise common.BuildFailure(f"Pinned vcpkg port lock mismatch: {name}")
    executable = checkout / ("vcpkg.exe" if host_platform() == "win64" else "vcpkg")
    if boot.get("output") != {"path": str(executable.relative_to(root)), "sha256": common.sha256(executable)}:
        raise common.BuildFailure("vcpkg bootstrap output checksum mismatch")
    tools = {}
    for name in ("cmake", "ninja", "uv"):
        path, version = common.tool(name, profile[name], cwd=root)
        tools[name] = {"path": path, "version": version, "sha256": common.sha256(Path(path))}
    python_version = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    if python_version != profile["python"]:
        raise common.BuildFailure(f"Python version mismatch: expected {profile['python']}, got {python_version}")
    tools["python"] = {"path": sys.executable, "version": python_version, "sha256": common.sha256(Path(sys.executable))}
    host_sdk = {}
    if host_platform() == "mac-arm64":
        sdk_path = str(Path(common.run(["xcrun", "--sdk", "macosx", "--show-sdk-path"], cwd=root).strip()).resolve())
        host_sdk = {
            "xcode": common.run(["xcodebuild", "-version"], cwd=root).strip(),
            "macos_sdk": common.run(["xcrun", "--sdk", sdk_path, "--show-sdk-version"], cwd=root).strip(),
            "macos_sdk_path": sdk_path,
        }
    elif host_platform() == "win64":
        host_sdk = {"visual_studio": os.environ.get("VisualStudioVersion"), "windows_sdk": os.environ.get("WindowsSDKVersion"), "msvc": os.environ.get("VCToolsVersion")}
    else:
        host_sdk = {"libc": platform.libc_ver()}
        if os.environ.get("CANOPY_LINUX_TOOLCHAIN_ROOT"):
            host_sdk.update(common.linux_libcxx())
    return {"tools": tools, "vcpkg_commit": commit, "bootstrap_sha256": common.sha256(bootstrap), "host_sdk": host_sdk}


def validate_macos_target(commands: list[list[str]], sdk: str, triplet: str) -> None:
    if triplet != "arm64-osx":
        raise common.BuildFailure("Compiled macOS core dependency triplet mismatch")
    expected_sdk = Path(sdk).resolve()
    for argv in commands:
        architectures = []
        sysroots = []
        for index, argument in enumerate(argv):
            if argument == "-arch":
                architectures.append(argv[index + 1] if index + 1 < len(argv) else "")
            elif argument in ("-isysroot", "--sysroot"):
                sysroots.append(argv[index + 1] if index + 1 < len(argv) else "")
            elif argument.startswith("-isysroot"):
                sysroots.append(argument.removeprefix("-isysroot"))
            elif argument.startswith("--sysroot="):
                sysroots.append(argument.removeprefix("--sysroot="))
        if architectures != ["arm64"]:
            raise common.BuildFailure("Compiled macOS core architecture mismatch")
        if not sysroots or any(not root or Path(root).resolve() != expected_sdk for root in sysroots):
            raise common.BuildFailure("Compiled macOS core SDK sysroot mismatch")


def build(root: Path, config: str) -> Path:
    build_dir = root / ".build/core"
    manifest = build_dir / config.lower() / "canopy-core-manifest.json"
    manifest.unlink(missing_ok=True)
    evidence = preflight(root, config)
    original_inputs = common.input_checksums(root)
    preset = "native-debug" if config == "Debug" else "native-release"
    cmake = evidence["tools"]["cmake"]["path"]
    host = host_platform()
    configure = [cmake, "--preset", preset]
    selected_compiler = None
    # Preserve the driver name: resolving clang++ to clang changes C++ link defaults.
    if host == "mac-arm64":
        sdk_path = evidence["host_sdk"]["macos_sdk_path"]
        selected_compiler = Path(common.run(["xcrun", "--sdk", sdk_path, "--find", "clang++"], cwd=root).strip()).absolute()
        configure += ["-DCMAKE_CXX_FLAGS=-stdlib=libc++", "-DCMAKE_OSX_ARCHITECTURES=arm64",
                      f"-DCMAKE_OSX_SYSROOT={sdk_path}", "-DVCPKG_TARGET_TRIPLET=arm64-osx"]
    elif host == "win64":
        binary = shutil.which("cl.exe")
        if not binary:
            raise common.BuildFailure("MSVC cl.exe missing from the selected developer environment")
        selected_compiler = Path(binary).absolute()
        triplet = common.load_json(root / "config/toolchains.json")["unreal"]["win64"]["qualified_triplet"]
        configure += [f"-DVCPKG_OVERLAY_TRIPLETS={root / 'dependencies/triplets'}",
                      f"-DVCPKG_TARGET_TRIPLET={triplet}", f"-DVCPKG_HOST_TRIPLET={triplet}"]
    elif host == "linux-x64":
        tc = os.environ.get("CANOPY_LINUX_TOOLCHAIN_ROOT", "")
        sysroot = os.environ.get("CANOPY_LINUX_SYSROOT", "")
        if bool(tc) != bool(sysroot):
            raise common.BuildFailure("Qualified Linux compiler and sysroot must be selected together")
        if tc:
            selected_compiler = (Path(tc) / "bin/clang++").absolute()
            c_compiler = (Path(tc) / "bin/clang").resolve()
            if (not Path(tc).is_absolute() or not Path(sysroot).is_absolute()
                    or not selected_compiler.is_file() or not c_compiler.is_file()
                    or not Path(sysroot).is_dir() or not (Path(sysroot) / "include").is_dir()):
                raise common.BuildFailure("Missing explicitly selected v26 Linux compiler/fixed sysroot")
            required_clang = common.load_json(root / "config/toolchains.json")["unreal"]["linux-x64"]["clang"]
            if required_clang not in common.run([str(selected_compiler), "--version"], cwd=root).splitlines()[0]:
                raise common.BuildFailure(f"Linux compiler mismatch: required Clang {required_clang}")
            libcxx = common.linux_libcxx()
            triplet = common.load_json(root / "config/toolchains.json")["unreal"]["linux-x64"]["qualified_triplet"]
            overlay = root / "dependencies/triplets"
            if not (overlay / f"{triplet}.cmake").is_file() or not (overlay / "v26-chainload.cmake").is_file():
                raise common.BuildFailure("Missing qualified v26 libc++ vcpkg overlay triplet/chainload")
            include = libcxx["libcxx_include"]
            runtime = f'-nodefaultlibs "{libcxx["libcxx_library"]}" "{libcxx["libcxxabi_library"]}" -lm -lc -lpthread -lgcc_s -lgcc'
            configure += [f"-DCMAKE_SYSROOT={Path(sysroot).resolve()}",
                          f'-DCMAKE_CXX_FLAGS=-nostdinc++ -isystem "{Path(sysroot) / "include"}" -isystem "{include}"',
                          "-DCMAKE_EXE_LINKER_FLAGS=", f"-DCMAKE_CXX_STANDARD_LIBRARIES={runtime}",
                          f"-DCMAKE_C_COMPILER={c_compiler}",
                          f"-DVCPKG_OVERLAY_TRIPLETS={overlay}", f"-DVCPKG_TARGET_TRIPLET={triplet}"]
        else:
            binary = shutil.which("c++")
            if not binary:
                raise common.BuildFailure("Missing native Linux C++ compiler")
            selected_compiler = Path(binary).absolute()
            version = common.run([str(selected_compiler), "--version"], cwd=root).splitlines()[0]
            configure += ["-DCMAKE_SYSROOT=", "-DCMAKE_EXE_LINKER_FLAGS=", "-DCMAKE_CXX_STANDARD_LIBRARIES=", "-DVCPKG_OVERLAY_TRIPLETS=",
                          "-DVCPKG_TARGET_TRIPLET=x64-linux",
                          "-DCMAKE_CXX_FLAGS=-stdlib=libstdc++" if "clang" in version.lower() else "-DCMAKE_CXX_FLAGS="]
    cache_path = build_dir / "CMakeCache.txt"
    if selected_compiler and cache_path.exists():
        old = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache_path.read_text(encoding="utf-8"), re.MULTILINE)
        if old and Path(old.group(1)).absolute() != selected_compiler:
            shutil.rmtree(build_dir)
    if selected_compiler:
        configure.append(f"-DCMAKE_CXX_COMPILER={selected_compiler}")
    common.run(configure, cwd=root)
    common.run([cmake, "--build", "--preset", preset, "--target", "canopy_core", "canopy_core_tests"], cwd=root)
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
    match = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, flags=re.MULTILINE)
    if not match:
        raise common.BuildFailure("Missing compiler path in generated CMake cache")
    compiler = Path(match.group(1)).absolute()
    if selected_compiler and compiler != selected_compiler:
        raise common.BuildFailure("Cached compiler disagrees with selected native compiler; remove the stale CMake cache")
    triplet_match = re.search(r"^VCPKG_TARGET_TRIPLET:[^=]+=(.+)$", cache, flags=re.MULTILINE)
    if not triplet_match:
        raise common.BuildFailure("Missing resolved vcpkg target triplet in CMake cache")
    triplet = triplet_match.group(1)
    if host == "win64" and triplet != common.load_json(root / "config/toolchains.json")["unreal"]["win64"]["qualified_triplet"]:
        raise common.BuildFailure("Compiled core Windows dependency triplet mismatch")
    if host == "linux-x64":
        expected_triplet = (common.load_json(root / "config/toolchains.json")["unreal"]["linux-x64"]["qualified_triplet"]
                            if os.environ.get("CANOPY_LINUX_TOOLCHAIN_ROOT") else "x64-linux")
        if triplet != expected_triplet:
            raise common.BuildFailure("Compiled core vcpkg dependency triplet mismatch")
    compiler_version = common.run([str(compiler), "/?" if host == "win64" else "--version"], cwd=root).splitlines()[0]
    compiler_records = list((build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(compiler_records) != 1:
        raise common.BuildFailure("Missing or ambiguous CMake compiler identity record")
    identity = compiler_records[0].read_text(encoding="utf-8")
    compiler_match = re.search(r'^set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', identity, re.MULTILINE)
    if not compiler_match:
        raise common.BuildFailure("Missing CMake compiler identity")
    commands = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    core_commands = [entry for entry in commands if "/core/src/" in entry["file"].replace("\\", "/") and "canopy_core.dir" in entry["command"]]
    if not core_commands:
        raise common.BuildFailure("Missing canopy_core compiler invocation evidence")
    args = [shlex.split(entry["command"], posix=host != "win64") for entry in core_commands]
    if host == "mac-arm64":
        validate_macos_target(args, evidence["host_sdk"]["macos_sdk_path"], triplet)
    if host == "win64":
        crt = "MDd" if config == "Debug" else "MD"
        flags = [{arg[1:] for arg in argv if arg.startswith(("/", "-"))} for argv in args]
        if not all({crt, "GR-", "EHs-c-"}.issubset(options) for options in flags):
            raise common.BuildFailure("Cannot confirm core MSVC CRT/RTTI/exception compile flags")
    else:
        if not all("-fno-rtti" in argv and "-fno-exceptions" in argv for argv in args):
            raise common.BuildFailure("Cannot confirm core RTTI/exception compile flags")
        crt = "libc++" if all("-stdlib=libc++" in argv for argv in args) else "libstdc++"
        if host == "linux-x64":
            sysroots = [{arg.removeprefix("--sysroot=") for arg in argv if arg.startswith("--sysroot=")} for argv in args]
            if len(set.union(*sysroots)) > 1 or any(roots != sysroots[0] for roots in sysroots):
                raise common.BuildFailure("Inconsistent core target sysroot")
            evidence["host_sdk"]["sysroot"] = str(Path(next(iter(sysroots[0]))).resolve()) if sysroots[0] else ""
            if os.environ.get("CANOPY_LINUX_TOOLCHAIN_ROOT"):
                include = evidence["host_sdk"]["libcxx_include"]
                sdk_include = str(Path(evidence["host_sdk"]["sysroot"]) / "include")
                runtime_libraries = re.search(r"^CMAKE_CXX_STANDARD_LIBRARIES:[^=]+=(.*)$", cache, flags=re.MULTILINE)
                if (evidence["host_sdk"]["sysroot"] != str(Path(os.environ["CANOPY_LINUX_SYSROOT"]).resolve())
                        or not all("-nostdinc++" in argv and include in argv and sdk_include in argv for argv in args)
                        or not runtime_libraries or not all(item in runtime_libraries.group(1) for item in ("-nodefaultlibs", evidence["host_sdk"]["libcxx_library"], evidence["host_sdk"]["libcxxabi_library"]))
                        or any(evidence["host_sdk"][key] != value for key, value in common.linux_libcxx().items())):
                    raise common.BuildFailure("Qualified Linux compiler ABI/sysroot/libc++ evidence mismatch")
                crt = "libc++"
    candidates = [p for p in build_dir.rglob("canopy_core.lib" if host == "win64" else "libcanopy_core.a") if p.is_file()]
    if len(candidates) != 1:
        raise common.BuildFailure(f"Expected exactly one static canopy_core output, found {len(candidates)}")
    library = candidates[0]
    if host == "mac-arm64":
        architectures = common.run(["xcrun", "--sdk", evidence["host_sdk"]["macos_sdk_path"], "lipo", "-archs", str(library)], cwd=root).split()
        if architectures != ["arm64"]:
            raise common.BuildFailure("Compiled macOS core library architecture mismatch")
    if original_inputs != common.input_checksums(root):
        raise common.BuildFailure("Core inputs changed during build")
    abi = {"crt": crt, "rtti": False, "exceptions": False}
    data = {"schema": 1, "scope": "standalone-core", "platform": host, "config": config, "compiler": {"id": compiler_match.group(1), "version": compiler_version, "path": str(compiler), "binary_sha256": common.sha256(compiler)}, "abi": abi, "vcpkg_triplet": triplet, "host_sdk": evidence["host_sdk"], "tools": evidence["tools"], "vcpkg_commit": evidence["vcpkg_commit"], "bootstrap_sha256": evidence["bootstrap_sha256"], "inputs_sha256": original_inputs, "library": {"path": str(library.resolve()), "sha256": common.sha256(library)}, "child_return_codes": {"configure": 0, "build": 0}, "unreal_qualified": False}
    common.write_manifest(manifest, data)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=("Debug", "Release"), required=True)
    args = parser.parse_args()
    try:
        print(build(ROOT, args.config))
        return 0
    except (common.BuildFailure, OSError, KeyError, ValueError) as error:
        print(f"Core build failed: {error}", file=sys.stderr)
        return error.code if isinstance(error, common.BuildFailure) else 1


if __name__ == "__main__":
    sys.exit(main())
