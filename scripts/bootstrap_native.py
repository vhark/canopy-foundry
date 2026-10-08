"""Bootstrap the exact vcpkg commit used by the standalone native build."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "dependencies/native-lock.json"
CHECKOUT = ROOT / ".work/tools/vcpkg"
EVIDENCE = ROOT / ".build/native-bootstrap.json"
ATTEMPT = ROOT / ".build/native-bootstrap-attempt.json"


def digest(path: Path) -> str:
    sha256 = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            sha256.update(chunk)
    return sha256.hexdigest()


def run(args: list[str], record: dict[str, object], *, cwd: Path = ROOT) -> str:
    environment = os.environ.copy()
    environment["VCPKG_DISABLE_METRICS"] = "1"
    print("+", " ".join(args), flush=True)
    completed = subprocess.run(args, cwd=cwd, env=environment, text=True, capture_output=True, check=False)
    commands = record["commands"]
    assert isinstance(commands, list)
    commands.append({
        "argv": args,
        "cwd": str(cwd),
        "returncode": completed.returncode,
        "stdout_sha256": hashlib.sha256(completed.stdout.encode()).hexdigest(),
        "stderr_sha256": hashlib.sha256(completed.stderr.encode()).hexdigest(),
    })
    if completed.stdout:
        print(completed.stdout, end="", flush=True)
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr, flush=True)
    if completed.returncode:
        raise RuntimeError(f"command failed (exit {completed.returncode}): {args}")
    return completed.stdout.strip()


def write_evidence(record: dict[str, object], destination: Path | None = None) -> None:
    destination = EVIDENCE if destination is None else destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(destination)


def bootstrap(record: dict[str, object]) -> None:
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    source = lock["vcpkg"]["repository"]
    commit = lock["vcpkg"]["commit"]
    if source != "https://github.com/microsoft/vcpkg.git" or commit != "2750401336fb7c95f6619657a46a7e798661341c":
        raise RuntimeError("native lock must match the approved official vcpkg repository and commit")
    record["inputs"] = {
        str(path.relative_to(ROOT)): digest(path)
        for path in (LOCK, ROOT / "dependencies/vcpkg.json", ROOT / "dependencies/vcpkg-configuration.json", Path(__file__).resolve())
    }
    manifest = json.loads((ROOT / "dependencies/vcpkg.json").read_text(encoding="utf-8"))
    registry = json.loads((ROOT / "dependencies/vcpkg-configuration.json").read_text(encoding="utf-8"))
    if registry["default-registry"] != {"kind": "builtin", "baseline": commit} or "builtin-baseline" in manifest:
        raise RuntimeError("registry baseline must be the sole source of the locked checkout baseline")
    locked_versions = {name: port["version"] for name, port in lock["ports"].items()}
    if {entry["name"]: entry["version"] for entry in manifest["overrides"]} != locked_versions:
        raise RuntimeError("vcpkg overrides do not match the approved native lock")
    if set(manifest["dependencies"]) != set(locked_versions):
        raise RuntimeError("vcpkg manifest dependencies do not match the approved native lock")
    git = shutil.which("git")
    if git is None:
        raise RuntimeError("git executable unavailable")
    record["git"] = {"executable": git, "sha256": digest(Path(git)), "version": run([git, "--version"], record)}
    if CHECKOUT.is_symlink():
        raise RuntimeError("refusing symlinked vcpkg checkout")
    if not CHECKOUT.exists():
        CHECKOUT.parent.mkdir(parents=True, exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix=".vcpkg-stage-", dir=CHECKOUT.parent))
        try:
            run([git, "init", str(stage)], record)
            run([git, "remote", "add", "origin", source], record, cwd=stage)
            run([git, "fetch", "--depth", "1", "origin", commit], record, cwd=stage)
            run([git, "checkout", "--detach", "FETCH_HEAD"], record, cwd=stage)
            if run([git, "rev-parse", "HEAD"], record, cwd=stage) != commit:
                raise RuntimeError("fetched vcpkg checkout does not match the approved commit")
            if CHECKOUT.exists() or CHECKOUT.is_symlink():
                raise RuntimeError("vcpkg checkout appeared while staging; refusing to overwrite it")
            stage.rename(CHECKOUT)
        finally:
            if stage.exists():
                shutil.rmtree(stage)
    if not CHECKOUT.is_dir() or not (CHECKOUT / ".git").is_dir():
        raise RuntimeError("refusing existing non-git vcpkg checkout")
    if run([git, "remote", "get-url", "origin"], record, cwd=CHECKOUT) != source:
        raise RuntimeError("vcpkg remote is not the locked official upstream")
    head = run([git, "rev-parse", "HEAD"], record, cwd=CHECKOUT)
    if head != commit:
        raise RuntimeError(f"vcpkg checkout mismatch: expected {commit}, found {head}")
    if run([git, "status", "--porcelain", "--untracked-files=all"], record, cwd=CHECKOUT):
        raise RuntimeError("refusing dirty vcpkg checkout")
    if run([git, "ls-files", "--others", "--ignored", "--exclude-standard", "--", "triplets"], record, cwd=CHECKOUT):
        raise RuntimeError("refusing unapproved custom vcpkg triplets")
    record["source"] = {"repository": source, "commit": head, "tree": run([git, "rev-parse", "HEAD^{tree}"], record, cwd=CHECKOUT)}
    ports_evidence: dict[str, dict[str, str]] = {}
    for name, pinned in lock["ports"].items():
        port_dir = CHECKOUT / "ports" / name
        portfile = port_dir / "portfile.cmake"
        metadata = port_dir / "vcpkg.json"
        port = json.loads(metadata.read_text(encoding="utf-8"))
        if (
            port.get("version", port.get("version-semver")) != pinned["version"]
            or port.get("license") != pinned["license"]
            or f"SHA512 {pinned['source_sha512']}" not in portfile.read_text(encoding="utf-8")
        ):
            raise RuntimeError(f"locked {name} version, license or source hash disagrees with pinned vcpkg port")
        ports_evidence[name] = {
            "portfile_sha256": digest(portfile),
            "metadata_sha256": digest(metadata),
            "source_sha512": pinned["source_sha512"],
        }
    record["ports"] = ports_evidence
    executable = CHECKOUT / ("vcpkg.exe" if os.name == "nt" else "vcpkg")
    script = CHECKOUT / ("bootstrap-vcpkg.bat" if os.name == "nt" else "bootstrap-vcpkg.sh")
    if not script.is_file():
        raise RuntimeError("pinned vcpkg checkout is missing its bootstrap script")
    record["source"]["bootstrap_script_sha256"] = digest(script)
    if executable.exists():
        # A reused executable is acceptable only when prior successful evidence
        # ties its bytes to these exact inputs, source tree and bootstrap script.
        if not executable.is_file() or not EVIDENCE.is_file():
            raise RuntimeError("refusing unverified pre-existing vcpkg executable")
        previous = json.loads(EVIDENCE.read_text(encoding="utf-8"))
        if (
            previous.get("status") != "success"
            or previous.get("inputs") != record["inputs"]
            or previous.get("source") != record["source"]
            or previous.get("output") != {"path": str(executable.relative_to(ROOT)), "sha256": digest(executable)}
        ):
            raise RuntimeError("refusing vcpkg executable without matching successful bootstrap evidence")
        record["reused_verified_output"] = True
    elif os.name == "nt":
        run(["cmd", "/d", "/c", str(script), "-disableMetrics"], record, cwd=CHECKOUT)
    else:
        run([str(script), "-disableMetrics"], record, cwd=CHECKOUT)
    if not executable.is_file():
        raise RuntimeError("vcpkg bootstrap exited successfully without producing the expected executable")
    record["output"] = {"path": str(executable.relative_to(ROOT)), "sha256": digest(executable)}
    record["status"] = "success"


def main() -> int:
    record: dict[str, object] = {"schema": 1, "status": "failed", "commands": []}
    try:
        bootstrap(record)
    except (OSError, ValueError, KeyError, TypeError, RuntimeError) as error:
        record["error"] = str(error)
        print(f"native bootstrap: {error}", file=sys.stderr)
        write_evidence(record, ATTEMPT)
        return 1
    write_evidence(record)
    ATTEMPT.unlink(missing_ok=True)
    print(f"native bootstrap evidence: {EVIDENCE}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
