"""Strict process and provenance helpers for local source builds."""

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path


class BuildFailure(Exception):
    def __init__(self, message: str, code: int = 1):
        super().__init__(message)
        self.code = code


def sha256(path: Path) -> str:
    if not path.is_file():
        raise BuildFailure(f"Missing build input/output: {path}")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(argv: list[str], *, cwd: Path, env: dict[str, str] | None = None, capture: bool = True) -> str:
    if not argv or not all(isinstance(arg, str) and arg for arg in argv):
        raise BuildFailure("Invalid process argument vector")
    print("argv: " + json.dumps(argv), flush=True)
    try:
        result = subprocess.run(argv, cwd=cwd, env=env, text=True,
                                stdout=subprocess.PIPE if capture else None,
                                stderr=subprocess.STDOUT, check=False)
    except OSError as error:
        raise BuildFailure(f"Cannot run {argv[0]}: {error}") from error
    if result.stdout:
        print(result.stdout, end="", flush=True)
    if result.returncode:
        raise BuildFailure(f"Command failed ({result.returncode}): {argv[0]}", result.returncode)
    return result.stdout or ""


def tool(name: str, expected: str, *, cwd: Path) -> tuple[str, str]:
    path = shutil.which(name)
    if not path:
        raise BuildFailure(f"Missing required tool {name} == {expected}; use uv run --frozen")
    actual = run([path, "--version"], cwd=cwd).splitlines()[0]
    tokens = actual.split()
    observed = tokens[-1] if name == "cmake" else tokens[1] if name == "uv" and len(tokens) > 1 else tokens[0]
    if observed != expected and not (name == "ninja" and observed.startswith(expected + ".g")):
        raise BuildFailure(f"{name} version mismatch: required {expected}, got {actual}")
    return str(Path(path).resolve()), actual


def load_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise BuildFailure(f"Invalid or missing JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise BuildFailure(f"Expected JSON object: {path}")
    return value


def write_manifest(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def core_inputs(root: Path) -> list[Path]:
    paths = [root / name for name in (
        "pyproject.toml", "uv.lock", "config/toolchains.json", "CMakeLists.txt",
        "CMakePresets.json", "dependencies/native-lock.json", "dependencies/vcpkg.json",
        "dependencies/vcpkg-configuration.json", "scripts/bootstrap_native.py",
        "scripts/build_common.py", "scripts/build_core.py", "scripts/build_game.py",
    )]
    paths += sorted((root / "dependencies/upstream").glob("*.whl"))
    paths += sorted((root / "dependencies/triplets").glob("*.cmake"))
    paths += sorted(path for path in (root / "core").rglob("*") if path.is_file())
    return paths


def input_checksums(root: Path) -> dict[str, str]:
    return {str(path.relative_to(root)): sha256(path) for path in core_inputs(root)}


def linux_libcxx() -> dict[str, str]:
    raw_include = os.environ.get("CANOPY_LINUX_LIBCXX_INCLUDE", "")
    raw_libdir = os.environ.get("CANOPY_LINUX_LIBCXX_LIBDIR", "")
    if not raw_include or not raw_libdir or not Path(raw_include).is_absolute() or not Path(raw_libdir).is_absolute():
        raise BuildFailure("Qualified Linux requires absolute CANOPY_LINUX_LIBCXX_INCLUDE and CANOPY_LINUX_LIBCXX_LIBDIR")
    include = Path(raw_include).resolve()
    libdir = Path(raw_libdir).resolve()
    raw_sysroot = os.environ.get("CANOPY_LINUX_SYSROOT", "")
    if not raw_sysroot or not Path(raw_sysroot).is_absolute():
        raise BuildFailure("Qualified Linux requires an absolute CANOPY_LINUX_SYSROOT")
    sdk = Path(raw_sysroot).resolve()
    if include != sdk / "include/c++/v1" or libdir != sdk / "lib64":
        raise BuildFailure("Qualified Linux libc++ headers/libraries must belong to the selected fixed sysroot")
    if not all((include / name).is_file() for name in ("__config", "vector", "string")):
        raise BuildFailure("Qualified Linux libc++ headers are missing from the selected include directory")
    library = libdir / "libc++.a"
    abi_library = libdir / "libc++abi.a"
    if not library.is_file() or not abi_library.is_file():
        raise BuildFailure("Qualified Linux SDK must provide lib64/libc++.a and libc++abi.a")
    headers = sorted(p for p in include.rglob("*") if p.is_file())
    digest = hashlib.sha256()
    for header in headers:
        digest.update(str(header.relative_to(include)).encode("utf-8"))
        digest.update(bytes.fromhex(sha256(header)))
    return {"libcxx_include": str(include), "libcxx_headers_sha256": digest.hexdigest(),
            "libcxx_library": str(library), "libcxx_library_sha256": sha256(library),
            "libcxxabi_library": str(abi_library), "libcxxabi_library_sha256": sha256(abi_library)}
