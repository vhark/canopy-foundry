import hashlib
import json
import sys

import pytest

from scripts import build_common, build_core, build_game


def test_child_failure_preserves_exit_code(tmp_path):
    child = tmp_path / "tool with spaces.py"
    child.write_text("import sys; sys.exit(37)\n")
    with pytest.raises(build_common.BuildFailure) as error:
        build_common.run([sys.executable, str(child)], cwd=tmp_path)
    assert error.value.code == 37


def test_argument_vector_keeps_paths_with_spaces(tmp_path):
    child = tmp_path / "tool with spaces.py"
    output = tmp_path / "output with spaces.json"
    child.write_text("import json,sys; open(sys.argv[1], 'w').write(json.dumps(sys.argv[2:]))\n")
    build_common.run([sys.executable, str(child), str(output), "a b", "-project=/a b/project.uproject"], cwd=tmp_path)
    assert json.loads(output.read_text()) == ["a b", "-project=/a b/project.uproject"]


def test_native_platform_and_project_preflight_reject_before_uat(tmp_path, monkeypatch):
    monkeypatch.setattr(build_game, "native_platform", lambda: "mac-arm64")
    with pytest.raises(build_common.BuildFailure, match="native host"):
        build_game.preflight(tmp_path, "win64", "Development", tmp_path / "UE")
    with pytest.raises(build_common.BuildFailure, match="project"):
        build_game.preflight(tmp_path, "mac-arm64", "Development", tmp_path / "UE")


def test_engine_patch_mismatch_is_rejected(tmp_path, monkeypatch):
    monkeypatch.setattr(build_game, "native_platform", lambda: "mac-arm64")
    project = tmp_path / "game" / "CanopyFoundry.uproject"
    project.parent.mkdir()
    project.write_text("{}")
    config = tmp_path / "config" / "toolchains.json"
    config.parent.mkdir()
    config.write_text(json.dumps({"unreal": {"engine": {"major": 5, "minor": 8, "patch": 1}}}))
    engine = tmp_path / "Unreal Engine 5.8"
    version = engine / "Engine" / "Build" / "Build.version"
    version.parent.mkdir(parents=True)
    version.write_text(json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 0}))
    with pytest.raises(build_common.BuildFailure, match="5.8.1"):
        build_game.preflight(tmp_path, "mac-arm64", "Development", engine)


def test_core_manifest_rejects_wrong_configuration_host_or_compiler(tmp_path):
    lib = tmp_path / "libcanopy_core.a"
    lib.write_bytes(b"archive bytes")
    correct = dict(schema=1, scope="standalone-core", platform="mac-arm64", config="Debug", compiler={"id": "AppleClang", "version": "26.1.1", "binary_sha256": "a" * 64}, abi={"crt": "libc++", "rtti": False, "exceptions": False}, library={"path": str(lib), "sha256": hashlib.sha256(lib.read_bytes()).hexdigest()})
    expected = {"platform": "mac-arm64", "config": "Debug", "compiler": {"id": "AppleClang", "version": "26.1.1"}, "abi": {"crt": "libc++", "rtti": False, "exceptions": False}}
    build_game.validate_core_manifest(correct, expected)
    for field, value in (("config", "Release"), ("platform", "win64"), ("compiler", {"id": "AppleClang", "version": "27", "binary_sha256": "a" * 64}), ("abi", {"crt": "libc++", "rtti": True, "exceptions": False})):
        wrong = dict(correct, **{field: value})
        with pytest.raises(build_common.BuildFailure, match=field):
            build_game.validate_core_manifest(wrong, expected)
    lib.write_bytes(b"changed")
    with pytest.raises(build_common.BuildFailure, match="checksum"):
        build_game.validate_core_manifest(correct, expected)


def test_core_requires_pinned_bootstrap(tmp_path):
    config = tmp_path / "config" / "toolchains.json"
    config.parent.mkdir()
    config.write_text(json.dumps({"standalone": {"vcpkg_commit": "2750401336fb7c95f6619657a46a7e798661341c"}}))
    (tmp_path / "dependencies").mkdir()
    (tmp_path / "dependencies/native-lock.json").write_text(json.dumps({"vcpkg": {"commit": "2750401336fb7c95f6619657a46a7e798661341c"}}))
    (tmp_path / "uv.lock").write_text("")
    with pytest.raises(build_common.BuildFailure, match="Missing pinned vcpkg bootstrap"):
        build_core.preflight(tmp_path, "Debug")


def test_failed_core_preflight_removes_prior_success_manifest(tmp_path):
    manifest = tmp_path / ".build/core/debug/canopy-core-manifest.json"
    manifest.parent.mkdir(parents=True)
    manifest.write_text('{"status":"success"}')
    with pytest.raises(build_common.BuildFailure):
        build_core.build(tmp_path, "Debug")
    assert not manifest.exists()


def test_core_input_set_includes_new_source_files(tmp_path):
    for relative in (
        "pyproject.toml", "uv.lock", "config/toolchains.json", "CMakeLists.txt",
        "CMakePresets.json", "dependencies/native-lock.json", "dependencies/vcpkg.json",
        "dependencies/vcpkg-configuration.json", "scripts/bootstrap_native.py",
        "scripts/build_common.py", "scripts/build_core.py", "scripts/build_game.py",
    ):
        path = tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}")
    before = build_common.input_checksums(tmp_path)
    added = tmp_path / "core/src/new_module.cpp"
    added.parent.mkdir(parents=True)
    added.write_text("int new_module = 1;\n")
    assert str(added.relative_to(tmp_path)) in build_common.input_checksums(tmp_path)
    assert before != build_common.input_checksums(tmp_path)


def test_qualified_libcxx_evidence_changes_with_headers_and_library(tmp_path, monkeypatch):
    include = tmp_path / "sysroot/include/c++/v1"
    libdir = tmp_path / "sysroot/lib64"
    include.mkdir(parents=True)
    libdir.mkdir()
    for name in ("__config", "vector", "string"):
        (include / name).write_text(name)
    library = libdir / "libc++.a"
    library.write_bytes(b"library")
    (libdir / "libc++abi.a").write_bytes(b"abi library")
    monkeypatch.setenv("CANOPY_LINUX_SYSROOT", str(tmp_path / "sysroot"))
    monkeypatch.setenv("CANOPY_LINUX_LIBCXX_INCLUDE", str(include))
    monkeypatch.setenv("CANOPY_LINUX_LIBCXX_LIBDIR", str(libdir))
    first = build_common.linux_libcxx()
    (include / "vector").write_text("modified vector")
    assert first["libcxx_headers_sha256"] != build_common.linux_libcxx()["libcxx_headers_sha256"]
    library.write_bytes(b"changed library")
    assert first["libcxx_library_sha256"] != build_common.linux_libcxx()["libcxx_library_sha256"]




@pytest.mark.parametrize(
    ("host", "relative"),
    [("mac-arm64", "CanopyFoundry.app/Contents/MacOS/CanopyFoundry"),
     ("linux-x64", "CanopyFoundry/Binaries/Linux/CanopyFoundry"),
     ("linux-x64", "CanopyFoundry/Binaries/Linux/CanopyFoundry-Linux-Shipping"),
     ("win64", "CanopyFoundry/Binaries/Win64/CanopyFoundry.exe"),
     ("win64", "CanopyFoundry/Binaries/Win64/CanopyFoundry-Win64-Shipping.exe")],
)
def test_package_rejects_text_and_accepts_binary_header(tmp_path, host, relative):
    if sys.platform == "win32" and host != "win64":
        pytest.skip("POSIX executable permissions require a POSIX filesystem")
    binary = tmp_path / relative
    binary.parent.mkdir(parents=True)
    binary.write_bytes(b"not an executable")
    assert not build_game.packaged_executable([binary], host)
    if host == "win64":
        binary.write_bytes(b"MZ" + b"\0" * 58 + (256).to_bytes(4, "little") + b"\0" * 192 +
                           b"PE\0\0" + b"\x64\x86" + b"\0" * 16 + b"\x02\0" +
                           b"\x0b\x02" + b"\0" * 38)
    elif host == "mac-arm64":
        binary.write_bytes(bytes.fromhex("cffaedfe0c000001") + b"\0" * 4 + b"\x02\0\0\0" + b"\0" * 16)
    else:
        binary.write_bytes(b"\x7fELF" + bytes([2, 1]) + b"\0" * 10 + b"\x02\0\x3e\0" + b"\0" * 44)
    binary.chmod(0o755)
    assert build_game.packaged_executable([binary], host)


@pytest.mark.skipif(sys.platform == "win32", reason="POSIX executable permissions require a POSIX filesystem")
def test_universal_macos_package_contains_arm64_executable(tmp_path):
    binary = tmp_path / "CanopyFoundry.app/Contents/MacOS/CanopyFoundry"
    binary.parent.mkdir(parents=True)
    arm64 = bytes.fromhex("cffaedfe0c000001") + b"\0" * 4 + b"\x02\0\0\0" + b"\0" * 16
    fat = bytes.fromhex("cafebabe000000010100000c") + b"\0" * 4 + (64).to_bytes(4, "big") + (32).to_bytes(4, "big") + b"\0" * 4
    binary.write_bytes(fat + b"\0" * (64 - len(fat)) + arm64)
    binary.chmod(0o755)
    assert build_game.packaged_executable([binary], "mac-arm64")


def test_game_input_snapshot_ignores_cook_outputs_but_detects_authored_changes(tmp_path):
    authored = [
        tmp_path / "game/CanopyFoundry.uproject",
        tmp_path / "game/Config/DefaultEngine.ini",
        tmp_path / "game/Content/Maps/Facility.umap",
        tmp_path / "game/Plugins/CanopyRuntime/Source/Runtime.cpp",
    ]
    for path in authored:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"authored input")
    before = build_game.game_input_checksums(tmp_path)
    for relative in (
        "game/Binaries/Mac/CanopyFoundry",
        "game/Intermediate/Build/generated.cpp",
        "game/Saved/Cooked/Facility.uasset",
        "game/Plugins/CanopyRuntime/Binaries/Mac/Runtime.dylib",
        "game/Plugins/CanopyRuntime/Intermediate/generated.cpp",
    ):
        generated = tmp_path / relative
        generated.parent.mkdir(parents=True, exist_ok=True)
        generated.write_bytes(b"build output")
    assert build_game.game_input_checksums(tmp_path) == before
    authored[-1].write_bytes(b"changed runtime input")
    after = build_game.game_input_checksums(tmp_path)
    key = str(authored[-1].relative_to(tmp_path))
    assert after[key] != before[key]


@pytest.mark.parametrize("version", ["Xcode 27.0\nBuild version 27A266a", "Xcode 26.4", "Xcode 26.1", ""])
def test_unapproved_xcode_fails_preflight(version):
    with pytest.raises(build_common.BuildFailure, match="26.1.1"):
        build_game.validate_xcode_version(version)
    build_game.validate_xcode_version("Xcode 26.1.1\nBuild version 17B100")


@pytest.mark.parametrize(
    ("vs", "msvc", "sdk"),
    [("17.0", "14.50.1", "10.0.26100.0"), ("18.0", "14.49.1", "10.0.26100.0"), ("18.0", "14.50.1", "10.0.22621.0")],
)
def test_unapproved_windows_toolchain_fails_preflight(vs, msvc, sdk):
    profile = {"visual_studio": "18.0", "msvc": "14.50", "windows_sdk": "10.0.26100"}
    with pytest.raises(build_common.BuildFailure, match="Windows SDK"):
        build_game.validate_windows_sdk(vs, msvc, sdk, profile)
    build_game.validate_windows_sdk("18.0", "14.50.1", "10.0.26100.0", profile)


@pytest.mark.parametrize(
    ("architecture", "sdk", "triplet", "error"),
    [
        ("x86_64", "/SDK/selected", "arm64-osx", "architecture"),
        (None, "/SDK/selected", "arm64-osx", "architecture"),
        ("arm64", "/SDK/other", "arm64-osx", "sysroot"),
        ("arm64", None, "arm64-osx", "sysroot"),
        ("arm64", "/SDK/selected", "x64-osx", "triplet"),
    ],
)
def test_macos_core_rejects_mismatched_effective_target(architecture, sdk, triplet, error):
    command = ["clang++", "-fno-rtti", "-fno-exceptions"]
    if architecture:
        command += ["-arch", architecture]
    if sdk:
        command += ["-isysroot", sdk]
    with pytest.raises(build_common.BuildFailure, match=error):
        build_core.validate_macos_target([command], "/SDK/selected", triplet)
