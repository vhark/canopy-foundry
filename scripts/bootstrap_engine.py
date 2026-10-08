"""Install pinned Unreal source and original GitDependencies payloads.

Default installs only host build tools. --full-editor installs the original
unfiltered Epic dependency manifest for Editor, runtime and cooker builds.
Never run Setup: it changes git hooks in the enclosing worktree.
"""

from __future__ import annotations

import argparse
import hashlib
import plistlib
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import sys
import tarfile
import tempfile
from urllib.error import URLError
from urllib.parse import urlsplit
import xml.etree.ElementTree as ET
from urllib.request import HTTPRedirectHandler, build_opener

try:
    from scripts.build_common import BuildFailure, load_json, run, sha256, write_manifest
except ModuleNotFoundError:
    from build_common import BuildFailure, load_json, run, sha256, write_manifest


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config/toolchains.json"
EVIDENCE = ROOT / ".build/engine-bootstrap.json"
LAUNCHER_REGISTRY = Path.home() / "Library/Application Support/Epic/UnrealEngineLauncher/LauncherInstalled.dat"
LAUNCHER_MANIFESTS = Path.home() / "Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests"
EDITOR_BINARY = "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
UBT_BINARY = "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
GENERATED_DIRECTORIES = frozenset({"Saved", "DerivedDataCache", ".vs"})
GENERATED_INSTALLED_FILES = frozenset({
    "Engine/Intermediate/ProjectFiles/PrimaryProjectName.txt",
    "Engine/Intermediate/ProjectFiles/PrimaryProjectPath.txt",
    "Engine/Intermediate/Build/Mac/Resources/Info-Editor.Template.plist",
    "Engine/Intermediate/Build/Mac/Resources/Info.Template.plist",
    "Engine/Intermediate/Build/BuildCookRun/StagedBuild_CanopyFoundry.ini",
    "Engine/Intermediate/TargetInfo.json",
    "Engine/Intermediate/Build/BuildRulesProjects/UE5Rules/UE5Rules.csproj",
    "Engine/Intermediate/Build/BuildRulesProjects/UE5ProgramRules/UE5ProgramRules.csproj",
    "Engine/Plugins/ScriptPlugin/Source/ScriptGeneratorUbtPlugin/ScriptGeneratorUbtPlugin.ubtplugin.csproj.props",
})
HOSTS = {
    ("Darwin", "arm64"): ("mac-arm64", "osx-arm64"),
    ("Windows", "AMD64"): ("win-x64", "win-x64"),
    ("Linux", "x86_64"): ("linux-x64", "linux-x64"),
}


def _source_requirements(requirements: dict) -> tuple[dict, dict]:
    try:
        source = requirements["source"]
        version = requirements["engine"]
        if not isinstance(source, dict) or not isinstance(version, dict):
            raise TypeError("invalid source/version")
        if not re.fullmatch(r"[0-9a-f]{40}", source["commit"]):
            raise ValueError("invalid commit")
        if not re.fullmatch(r"[0-9a-f]{64}", source["archive_sha256"]):
            raise ValueError("invalid digest")
        if source["archive_root"] != "EpicGames-UnrealEngine-" + source["commit"]:
            raise ValueError("root/commit mismatch")
        if any(type(version[key]) is not int or version[key] < 0 for key in ("major", "minor", "patch")):
            raise ValueError("invalid engine version")
    except (KeyError, TypeError, ValueError) as error:
        raise BuildFailure("Invalid pinned Unreal source configuration") from error
    return source, version


def _safe_member(name: str, root: str) -> tuple[str, ...]:
    # No tarfile.extract(): independently constrain every member before writing.
    if not name or "\\" in name or "\x00" in name or name.startswith("/"):
        raise BuildFailure("Unsafe Unreal archive path")
    parts = name.rstrip("/").split("/")
    if parts[0] != root or any(part in ("", ".", "..") or ":" in part for part in parts):
        raise BuildFailure("Unreal archive has an unexpected root or unsafe path")
    return tuple(parts[1:])


def extract_source(archive: Path, destination: Path, requirements: dict) -> Path:
    """Digest-check and atomically admit a new, exact-version source tree.

    Files are written only to private staging, then the verified tree is installed.
    Only regular files and directories are admitted; links and special files are forbidden.
    """
    source, version = _source_requirements(requirements)
    archive = Path(archive)
    destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        raise BuildFailure(f"Refusing to overwrite Unreal installation: {destination}")
    if sha256(archive) != source["archive_sha256"]:
        raise BuildFailure("Unreal source archive SHA-256 does not match the approved commit")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".unreal-source-", dir=destination.parent))
    try:
        with tarfile.open(archive, "r:gz") as tar:
            seen: set[str] = set()
            found_version = False
            for member in tar:
                parts = _safe_member(member.name, source["archive_root"])
                if not member.isdir() and not member.isfile():
                    raise BuildFailure("Unreal archive contains a link or special file")
                if not parts:
                    if not member.isdir():
                        raise BuildFailure("Unreal archive root is not a directory")
                    continue
                relative = PurePosixPath(*parts)
                key = str(relative).casefold()
                if key in seen:
                    raise BuildFailure("Unreal archive contains duplicate/case-colliding entries")
                seen.add(key)
                output = stage.joinpath(*parts)
                if member.isdir():
                    output.mkdir(parents=True, exist_ok=True)
                else:
                    output.parent.mkdir(parents=True, exist_ok=True)
                    with tar.extractfile(member) as input_stream, output.open("xb") as output_stream:
                        if input_stream is None:
                            raise BuildFailure("Missing Unreal archive file contents")
                        shutil.copyfileobj(input_stream, output_stream, 1024 * 1024)
                    if os.name != "nt":
                        output.chmod(0o755 if member.mode & 0o111 else 0o644)
                if parts == ("Engine", "Build", "Build.version") and member.isfile():
                    found_version = True
            if not found_version:
                raise BuildFailure("Unreal archive is missing Engine/Build/Build.version")
        build_version = load_json(stage / "Engine/Build/Build.version")
        observed = tuple(build_version.get(key) for key in ("MajorVersion", "MinorVersion", "PatchVersion"))
        expected = tuple(version[key] for key in ("major", "minor", "patch"))
        if any(type(value) is not int for value in observed) or observed != expected:
            raise BuildFailure(f"Unreal archive Build.version mismatch: required {expected}, got {observed}")
        if destination.exists() or destination.is_symlink():
            raise BuildFailure(f"Unreal installation appeared during extraction: {destination}")
        stage.rename(destination)
        return destination
    except (tarfile.TarError, EOFError, OSError) as error:
        raise BuildFailure(f"Unable to extract pinned Unreal source: {type(error).__name__}") from error
    finally:
        if stage.exists():
            shutil.rmtree(stage)


class _HttpsRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        if urlsplit(newurl).scheme.lower() != "https":
            raise BuildFailure("Unreal archive redirect must use HTTPS")
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def _download_url(commit: str) -> str:
    url = os.environ.get("CANOPY_UE_SOURCE_ARCHIVE_URL", "")
    parsed = urlsplit(url)
    if (parsed.scheme != "https" or parsed.netloc != "codeload.github.com" or parsed.fragment
            or parsed.path not in (f"/EpicGames/UnrealEngine/tar.gz/{commit}",
                                   f"/EpicGames/UnrealEngine/legacy.tar.gz/{commit}")):
        raise BuildFailure("Missing or invalid CANOPY_UE_SOURCE_ARCHIVE_URL for the pinned private commit")
    return url


def _download_archive(commit: str, parent: Path) -> Path:
    url = _download_url(commit)
    fd, filename = tempfile.mkstemp(prefix=".unreal-archive-", suffix=".tar.gz", dir=parent)
    archive = Path(filename)
    try:
        with os.fdopen(fd, "wb") as target:
            # urllib exceptions can contain the signed URL: never print or persist them.
            with build_opener(_HttpsRedirects).open(url, timeout=120) as response:
                if urlsplit(response.url).scheme.lower() != "https":
                    raise BuildFailure("Unreal archive response was not HTTPS")
                shutil.copyfileobj(response, target, 1024 * 1024)
        return archive
    except (OSError, URLError, ValueError) as error:
        archive.unlink(missing_ok=True)
        raise BuildFailure(f"Unable to download pinned Unreal source archive ({type(error).__name__}); signed URL may have expired") from None
    except BaseException:
        archive.unlink(missing_ok=True)
        raise


def _host() -> tuple[str, str]:
    key = (platform.system(), platform.machine())
    if key not in HOSTS:
        raise BuildFailure(f"Unsupported native Unreal bootstrap host: {key}")
    return HOSTS[key]




def _publish_source(installed: Path, destination: Path, record: dict) -> None:
    if destination.exists() or destination.is_symlink():
        raise BuildFailure("Refusing to overwrite an Unreal installation that appeared during bootstrap")
    installed.rename(destination)
    try:
        write_manifest(EVIDENCE, record)
    except BaseException:
        destination.rename(installed)
        if EVIDENCE.exists():
            EVIDENCE.unlink()
        raise


def _dependency_manifest(installed: Path) -> dict:
    manifest = installed / "Engine/Build/Commit.gitdeps.xml"
    if not manifest.is_file() or manifest.is_symlink():
        raise BuildFailure("Pinned source lacks the original Epic dependency manifest")
    # GitDependencies writes the selected files to this working manifest. Hash
    # the recorded output too; a successful exit alone is not proof of payload.
    working = installed / ".uedependencies"
    if not working.is_file() or working.is_symlink():
        raise BuildFailure("GitDependencies did not publish its working manifest")
    count = 0
    payloads = hashlib.sha256()
    for _, node in ET.iterparse(working, events=("end",)):
        if node.tag == "File":
            name = node.attrib.get("Name", "")
            path = (installed / name).resolve()
            if (not name or not re.fullmatch(r"[0-9a-f]{40}", node.attrib.get("ExpectedHash", ""))
                    or node.attrib.get("Hash") != node.attrib["ExpectedHash"]
                    or not path.is_relative_to(installed.resolve()) or not path.is_file()):
                raise BuildFailure("GitDependencies reported an incomplete selected dependency")
            actual = hashlib.sha1()
            content = hashlib.sha256()
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    actual.update(block)
                    content.update(block)
            if actual.hexdigest() != node.attrib["ExpectedHash"]:
                raise BuildFailure(f"GitDependencies selected payload changed: {name}")
            payloads.update(name.encode("utf-8") + b"\0" + content.digest())
            count += 1
        node.clear()
    if count == 0:
        raise BuildFailure("GitDependencies published an empty dependency manifest")
    return {"original_manifests_sha256": {"Engine/Build/Commit.gitdeps.xml": sha256(manifest)},
            "working_manifest_sha256": sha256(working), "selected_file_count": count,
            "payloads_sha256": payloads.hexdigest()}


def _engine_version(engine: Path, requirements: dict) -> dict:
    version = load_json(engine / "Engine/Build/Build.version")
    expected = requirements["engine"]
    if any(type(version.get(field)) is not int or version[field] != expected[key]
           for field, key in (("MajorVersion", "major"), ("MinorVersion", "minor"), ("PatchVersion", "patch"))):
        raise BuildFailure(f"Unreal engine release differs from configured {expected}: {version}")
    return version


def _launcher_identity(engine: Path, expected: dict) -> dict:
    if not engine.is_absolute() or engine.is_symlink() or not engine.is_dir():
        raise BuildFailure("Installed Editor root must be an existing absolute non-symlinked directory")
    registry = load_json(LAUNCHER_REGISTRY)
    candidates = [entry for entry in registry.get("InstallationList", [])
                  if isinstance(entry, dict) and entry.get("InstallLocation") == str(engine.resolve())
                  and entry.get("AppName") == expected["app_name"] and entry.get("NamespaceId") == "ue"]
    if len(candidates) != 1 or candidates[0].get("AppVersion") != expected["app_version"]:
        raise BuildFailure("Epic Launcher installation registry does not match the pinned Editor release and root")
    matching = []
    for path in sorted(LAUNCHER_MANIFESTS.glob("*.item")):
        receipt = load_json(path)
        if receipt.get("InstallLocation") == str(engine.resolve()) and receipt.get("AppName") == expected["app_name"]:
            matching.append((path, receipt))
    if len(matching) != 1:
        raise BuildFailure("Epic Launcher Editor installation receipt is missing or ambiguous")
    item_path, receipt = matching[0]
    guid = receipt.get("InstallationGuid")
    if (receipt.get("AppVersionString") != expected["app_version"]
            or receipt.get("bIsIncompleteInstall") is not False
            or receipt.get("CatalogNamespace") not in (None, "ue")
            or receipt.get("ManifestLocation") != str(engine / ".egstore")
            or not isinstance(guid, str) or not re.fullmatch(r"[A-Za-z0-9]+", guid)
            or not (engine / ".egstore" / f"{guid}.manifest").is_file()):
        raise BuildFailure("Epic Launcher Editor receipt or installed manifest differs from the pinned release")
    return {"registry_entry": candidates[0], "receipt_path": str(item_path),
            "receipt_sha256": sha256(item_path)}


def _installed_inventory(engine: Path) -> dict:
    """Hash installation inputs, excluding runtime caches and generated project-selection metadata."""
    digest = hashlib.sha256()
    count = 0
    for directory, dirs, files in os.walk(engine, followlinks=False):
        base = Path(directory)
        # Do not omit Intermediate: it also contains shipped rules and generated headers.
        dirs[:] = sorted(name for name in dirs if name not in GENERATED_DIRECTORIES
                         and not (base == engine / ".egstore" and name == "Pending")
                         and not (base == engine / "Engine/Intermediate" and name == "UbtRuns"))
        for name in (*dirs, *files):
            path = base / name
            if path.is_symlink():
                raise BuildFailure(f"Installed engine inventory contains a symlink: {path}")
        for name in sorted(files):
            path = base / name
            if not path.is_file():
                raise BuildFailure(f"Installed engine inventory contains a non-file: {path}")
            relative_path = path.relative_to(engine)
            relative = relative_path.as_posix()
            # Xcode metadata export writes shared-PCH wrappers and response files.
            # Keep actual .gch binaries, module definitions and shipped UHT headers.
            generated_pch = (
                relative_path.parts[:5] == ("Engine", "Intermediate", "Build", "Mac", "arm64")
                and name.startswith(("SharedPCH.", "SharedDefinitions."))
                and (name.endswith(".h") or name.endswith(".h.gch.rsp")))
            if relative in GENERATED_INSTALLED_FILES or generated_pch:
                continue
            digest.update(relative.encode("utf-8") + b"\0" + bytes.fromhex(sha256(path)))
            count += 1
    if count == 0:
        raise BuildFailure("Installed engine inventory is empty")
    return {"selected_file_count": count, "payloads_sha256": digest.hexdigest()}


def _installed_payload(engine: Path, requirements: dict, host: str) -> tuple[dict, dict]:
    if host != "mac-arm64":
        raise BuildFailure(f"No approved installed Editor distribution for {host}")
    expected = requirements["installed"][host]
    version = _engine_version(engine, requirements)
    if (sha256(engine / "Engine/Build/Build.version") != expected["build_version_sha256"]
            or sha256(engine / EDITOR_BINARY) != expected["editor_sha256"]
            or sha256(engine / UBT_BINARY) != expected["ubt_sha256"]):
        raise BuildFailure("Installed UnrealEditor/UnrealBuildTool/Build.version differs from pinned release")
    with (engine / "Engine/Binaries/Mac/UnrealEditor.app/Contents/Info.plist").open("rb") as stream:
        plist = plistlib.load(stream)
    dotted = ".".join(str(requirements["engine"][key]) for key in ("major", "minor", "patch"))
    if (plist.get("CFBundleShortVersionString") != dotted
            or plist.get("CFBundleExecutable") != "UnrealEditor"
            or expected["app_version"] != f"{dotted}-{version.get('Changelist')}+{version.get('BranchName')}-Mac"):
        raise BuildFailure("Installed Editor bundle/release identity does not match pinned Epic build")
    return _launcher_identity(engine, expected), _installed_inventory(engine)


def register_installed_editor(root: Path, engine: Path, host: str, tool: dict) -> dict:
    """Admit an existing Epic Launcher Editor without modifying its installed SDK."""
    root, engine = Path(root).resolve(), Path(engine)
    if not engine.is_absolute() or engine.is_symlink():
        raise BuildFailure("--engine-root must be an absolute non-symlinked installed Editor path")
    engine = engine.resolve()
    requirements = load_json(root / "config/toolchains.json")["unreal"]
    launcher, inventory = _installed_payload(engine, requirements, host)
    dotnet_path = f"Engine/Binaries/ThirdParty/DotNet/{requirements['source']['dotnet_directory']}/{host}/dotnet"
    dotnet = engine / dotnet_path
    if not dotnet.is_file() or dotnet.is_symlink():
        raise BuildFailure("Installed Editor bundled .NET SDK is missing")
    version = run([str(dotnet), "--version"], cwd=engine).strip()
    info = run([str(dotnet), "--info"], cwd=engine)
    if not re.fullmatch(r"10\.0\.\d+", version) or not info.strip():
        raise BuildFailure("Installed Editor bundled .NET SDK is not the approved 10.0 SDK")
    record = {"schema": 1, "status": "success", "scope": "unreal-installed-editor",
              "engine_root": str(engine), "platform": host,
              "installed": {"distribution": "epic-launcher", "launcher": launcher, "inventory": inventory},
              "dotnet": {"path": dotnet_path, "sha256": sha256(dotnet), "version": version,
                         "info_sha256": hashlib.sha256(info.encode()).hexdigest(),
                         "version_returncode": 0, "info_returncode": 0},
              "inputs": {"config_sha256": sha256(root / "config/toolchains.json"),
                         "bootstrap_sha256": sha256(root / "scripts/bootstrap_engine.py")},
              "native_toolchain": tool}
    # Re-evaluate admitted files and checkout hashes before atomically replacing evidence.
    if (_installed_payload(engine, requirements, host) != (launcher, inventory)
            or record["dotnet"]["sha256"] != sha256(dotnet)
            or record["inputs"] != {"config_sha256": sha256(root / "config/toolchains.json"),
                                    "bootstrap_sha256": sha256(root / "scripts/bootstrap_engine.py")}):
        raise BuildFailure("Installed Editor or checkout inputs changed during admission")
    write_manifest(root / ".build/engine-bootstrap.json", record)
    return record


def validate_editor(root: Path, engine: Path, host: str, tool: dict) -> dict:
    """Validate source or installed Editor admission against this checkout and the live engine."""
    root, engine = Path(root).resolve(), Path(engine).resolve()
    boot_path = root / ".build/engine-bootstrap.json"
    boot = load_json(boot_path)
    requirements = load_json(root / "config/toolchains.json")["unreal"]
    if (boot.get("schema") != 1 or boot.get("status") != "success"
            or boot.get("engine_root") != str(engine)
            or boot.get("platform") != ("win-x64" if host == "win64" else host)
            or boot.get("inputs") != {"config_sha256": sha256(root / "config/toolchains.json"),
                                      "bootstrap_sha256": sha256(root / "scripts/bootstrap_engine.py")}):
        raise BuildFailure("Editor admission differs from this checkout's pinned bootstrap evidence")
    if boot.get("native_toolchain") != tool:
        raise BuildFailure("Native compiler differs from Editor admission")
    _engine_version(engine, requirements)
    dotnet = boot.get("dotnet")
    if not isinstance(dotnet, dict) or not isinstance(dotnet.get("path"), str):
        raise BuildFailure("Provisioned Editor .NET SDK evidence is missing")
    relative = dotnet["path"]
    expected_dotnet = (f"Engine/Binaries/ThirdParty/DotNet/"
                       f"{requirements['source']['dotnet_directory']}/"
                       f"{'win-x64' if host == 'win64' else host}/"
                       f"{'dotnet.exe' if host == 'win64' else 'dotnet'}")
    path = (engine / relative).resolve()
    if (relative != expected_dotnet or not path.is_relative_to(engine)
            or (engine / relative).is_symlink() or dotnet.get("sha256") != sha256(path)
            or dotnet.get("version_returncode") != 0):
        raise BuildFailure("Provisioned Editor .NET SDK changed")
    build_script = {"mac-arm64": "Mac/Build.sh", "linux-x64": "Linux/Build.sh",
                    "win64": "Build.bat"}.get(host)
    uat_script = "RunUAT.bat" if host == "win64" else "RunUAT.sh"
    if (build_script is None
            or not (engine / "Engine/Build/BatchFiles" / build_script).is_file()
            or not (engine / "Engine/Build/BatchFiles" / uat_script).is_file()):
        raise BuildFailure("Official Editor build or Automation Tool script missing")
    if boot.get("scope") == "unreal-installed-editor":
        launcher, inventory = _installed_payload(engine, requirements, host)
        installed = boot.get("installed")
        if (not isinstance(installed, dict) or installed.get("distribution") != "epic-launcher"
                or installed.get("launcher") != launcher
                or installed.get("inventory") != inventory
                or boot.get("source") is not None or boot.get("gitdependencies") is not None):
            raise BuildFailure("Installed Editor inventory or Epic Launcher provenance changed")
        return {"boot_path": boot_path, "installed": True, "dependency_manifest": inventory}
    if boot.get("scope") != "unreal-full-editor":
        raise BuildFailure("Game requires a pinned full-editor bootstrap admission")
    source = requirements["source"]
    version_path = engine / "Engine/Build/Build.version"
    admission_source = boot.get("source")
    if (not isinstance(admission_source, dict)
            or any(admission_source.get(key) != source[key]
                   for key in ("tag", "commit", "archive_root", "archive_sha256"))
            or admission_source.get("build_version_sha256") != sha256(version_path)
            or admission_source.get("build_version") != load_json(version_path)):
        raise BuildFailure("Pinned full-editor source archive identity changed")
    deps = boot.get("gitdependencies")
    if not isinstance(deps, dict) or not isinstance(deps.get("path"), str):
        raise BuildFailure("Official full-editor dependency client evidence is missing")
    manifest = engine / "Engine/Build/Commit.gitdeps.xml"
    working = engine / ".uedependencies"
    gitdeps = (engine / deps["path"]).resolve()
    if (deps.get("original_manifests_sha256", {}).get("Engine/Build/Commit.gitdeps.xml") != sha256(manifest)
            or deps.get("working_manifest_sha256") != sha256(working)
            or not isinstance(deps.get("selected_file_count"), int) or deps["selected_file_count"] < 1
            or deps.get("filter_sha256") is not None or (engine / ".gitdepsignore").exists()
            or deps.get("returncode") != 0 or not gitdeps.is_relative_to(engine)
            or deps.get("sha256") != sha256(gitdeps)):
        raise BuildFailure("Official full-editor dependency manifest/client changed")
    dependency_manifest = _dependency_manifest(engine)
    if any(deps.get(key) != value for key, value in dependency_manifest.items()):
        raise BuildFailure("Full-editor selected dependency payloads changed")
    if not isinstance(boot.get("build_inputs"), dict) or not boot["build_inputs"]:
        raise BuildFailure("Pinned full-editor source build inputs are missing")
    for name, expected_hash in boot["build_inputs"].items():
        if not isinstance(name, str):
            raise BuildFailure("Pinned full-editor source build input path is invalid")
        path = (engine / name).resolve()
        if not path.is_relative_to(engine) or sha256(path) != expected_hash:
            raise BuildFailure(f"Full-editor source build input changed: {name}")
    return {"boot_path": boot_path, "installed": False, "dependency_manifest": dependency_manifest}


def bootstrap(destination: Path, archive: Path | None = None, *, full_editor: bool = False) -> dict:
    requirements = load_json(CONFIG)["unreal"]
    source, version = _source_requirements(requirements)
    if (version["major"], version["minor"], version["patch"]) != (5, 8, 3):
        raise BuildFailure("Unsupported Unreal source version")
    if source.get("tag") != "5.8.3-release" or source.get("dotnet_directory") != "10.0":
        raise BuildFailure("Unreal tag or bundled .NET directory differs from the approved source")
    host, rid = _host()
    if full_editor:
        try:
            from scripts import build_game
        except ImportError:
            import build_game
        compiler = build_game.qualified_compiler(ROOT, "win64" if host == "win-x64" else host,
                                                 requirements["win64" if host == "win-x64" else host])
    if not destination.is_absolute() or destination.exists() or destination.is_symlink():
        raise BuildFailure("--engine-root must be an absolute, new, non-symlinked directory")
    if archive is not None and not archive.is_absolute():
        raise BuildFailure("--archive must be an absolute path")
    if EVIDENCE.exists() or EVIDENCE.is_symlink():
        raise BuildFailure("Refusing to overwrite existing engine bootstrap evidence")
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    if full_editor:
        free_bytes = shutil.disk_usage(destination.parent).free
        minimum_bytes = 100 * 1024 ** 3
        if free_bytes < minimum_bytes:
            raise BuildFailure(f"Full Editor source build requires at least 100 GiB free on {destination.parent}; found {free_bytes / 1024 ** 3:.1f} GiB. Use a trusted build volume/runner with sufficient capacity.")
    downloaded = None
    staged = Path(tempfile.mkdtemp(prefix=".unreal-bootstrap-", dir=destination.parent))
    installed = staged / "source"
    try:
        if archive is None:
            downloaded = _download_archive(source["commit"], staged)
            archive = downloaded
        extract_source(archive, installed, requirements)
        gitdeps = installed / "Engine/Binaries/DotNET/GitDependencies" / rid / ("GitDependencies.exe" if os.name == "nt" else "GitDependencies")
        if not gitdeps.is_file() or gitdeps.is_symlink():
            raise BuildFailure("Approved source does not include the native host GitDependencies executable")
        gitdeps_hash = sha256(gitdeps)
        ignore = installed / ".gitdepsignore"
        if ignore.exists() or ignore.is_symlink():
            raise BuildFailure("Refusing existing Unreal dependency filter in source archive")
        managed_inputs = (
            "Directory.Build.props",
            "Directory.Build.targets",
            "Engine/Source/Programs/Shared/Directory.Build.props",
            "Engine/Source/Programs/Shared/EpicGames.Horde/Protos/horde/log_rpc.proto",
            "Engine/Source/Programs/Shared/EpicGames.Horde/Protos/horde/log_rpc_messages.proto",
            "Engine/Source/Programs/Shared/EpicGames.UBA/Library.props",
            "Engine/Source/Programs/Shared/UnrealEngine.CSharp.targets",
            "Engine/Source/Programs/Shared/UnrealEngine.csproj.props",
        )
        uba_directory, uba_names = {
            "mac-arm64": ("Mac", ("libUbaHost.dylib", "libUbaDetours.dylib")),
            "linux-x64": ("Linux", ("libUbaHost.so", "libUbaDetours.so", "UbaStaticStub.bin")),
            "win-x64": ("Win64", ("x64/UbaHost.dll", "x64/UbaDetours.dll")),
        }[host]
        build_inputs = managed_inputs + tuple(
            f"Engine/Binaries/{uba_directory}/UnrealBuildAccelerator/{name}" for name in uba_names
        )
        if host == "win-x64":
            build_inputs += ("Engine/Build/Windows/Resources/Default.ico",)
        elif host == "linux-x64":
            build_inputs += ("Engine/Binaries/Linux/dump_syms", "Engine/Binaries/Linux/BreakpadSymbolEncoder")
        if not full_editor:
            ignore.write_text(
                "# Host .NET, native UBA and official UBT inputs; not an editor installation.\n"
                "**\n"
                f"!/Engine/Binaries/ThirdParty/DotNet/10.0/{host}/**\n"
                + "".join(f"!/{name}\n" for name in build_inputs),
                encoding="utf-8",
            )
        environment = os.environ.copy()
        environment.pop("CANOPY_UE_SOURCE_ARCHIVE_URL", None)
        # Prevent ambient GitDependencies options or caches from changing the selection.
        for key in ("UE_GITDEPS_ARGS", "UE_GITDEPS", "UE4_GITDEPS"):
            environment.pop(key, None)
        environment["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"
        environment["DOTNET_GENERATE_ASPNET_CERTIFICATE"] = "false"
        arguments = [f"--root={installed}", "--force", "--no-cache"]
        run([str(gitdeps), *arguments], cwd=installed, env=environment)
        if any(not (installed / name).is_file() for name in build_inputs):
            raise BuildFailure("GitDependencies did not install the official UBT/UBA build inputs")
        dependencies = _dependency_manifest(installed) if full_editor else {}
        dotnet = installed / "Engine/Binaries/ThirdParty/DotNet/10.0" / host / ("dotnet.exe" if os.name == "nt" else "dotnet")
        if not dotnet.is_file() or dotnet.is_symlink():
            raise BuildFailure("GitDependencies did not install the host's bundled .NET executable")
        dotnet_version = run([str(dotnet), "--version"], cwd=installed, env=environment).strip()
        dotnet_info = run([str(dotnet), "--info"], cwd=installed, env=environment)
        if not re.fullmatch(r"10\.0\.\d+", dotnet_version) or not dotnet_info.strip():
            raise BuildFailure("Bundled .NET SDK is missing or not version 10.0")
        version_file = installed / "Engine/Build/Build.version"
        record = {
            "schema": 1, "status": "success",
            "scope": "unreal-full-editor" if full_editor else "unreal-build-tools",
            "engine_root": str(destination), "platform": host,
            "source": {"tag": source["tag"], "commit": source["commit"],
                       "archive_root": source["archive_root"], "archive_sha256": source["archive_sha256"],
                       "build_version": load_json(version_file), "build_version_sha256": sha256(version_file)},
            "gitdependencies": {"path": str(gitdeps.relative_to(installed)), "sha256": gitdeps_hash,
                                "filter_sha256": sha256(ignore) if not full_editor else None,
                                "host_rid": rid, "arguments": arguments, "returncode": 0,
                                **dependencies},
            "build_inputs": {name: sha256(installed / name) for name in build_inputs},
            "dotnet": {"path": dotnet.relative_to(installed).as_posix(), "sha256": sha256(dotnet),
                       "version": dotnet_version, "info_sha256": hashlib.sha256(dotnet_info.encode("utf-8")).hexdigest(),
                       "version_returncode": 0, "info_returncode": 0},
            "inputs": {"config_sha256": sha256(CONFIG), "bootstrap_sha256": sha256(Path(__file__).resolve())},
            **({"native_toolchain": compiler} if full_editor else {}),
        }
        _publish_source(installed, destination, record)
        return record
    finally:
        if downloaded is not None:
            downloaded.unlink(missing_ok=True)
        shutil.rmtree(staged)


def main() -> int:
    parser = argparse.ArgumentParser(description="Install pinned UE 5.8.3 source or register an existing Epic Editor")
    parser.add_argument("--engine-root", type=Path, required=True, help="absolute new source destination or existing installed Editor")
    parser.add_argument("--archive", type=Path, help="absolute local source tar.gz; otherwise use CANOPY_UE_SOURCE_ARCHIVE_URL")
    parser.add_argument("--full-editor", action="store_true", help="install original complete Epic dependencies (requires qualified native compiler)")
    parser.add_argument("--installed-editor", action="store_true", help="register an existing Epic Launcher Editor without changing its SDK")
    args = parser.parse_args()
    try:
        if args.installed_editor:
            if args.archive is not None or args.full_editor:
                raise BuildFailure("--installed-editor cannot be combined with source --archive or --full-editor")
            host, _ = _host()
            if host != "mac-arm64":
                raise BuildFailure(f"No approved installed Editor distribution for {host}")
            try:
                from scripts import build_game
            except ImportError:
                import build_game
            requirements = load_json(CONFIG)["unreal"]
            compiler = build_game.qualified_compiler(ROOT, host, requirements[host])
            result = register_installed_editor(ROOT, args.engine_root, host, compiler)
        else:
            result = bootstrap(args.engine_root, args.archive, full_editor=args.full_editor)
    except (BuildFailure, OSError, KeyError, ValueError, TypeError) as error:
        print(f"engine bootstrap: {error}", file=sys.stderr)
        return 1
    print(f"engine bootstrap: {result['engine_root']} (provenance: {EVIDENCE})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
