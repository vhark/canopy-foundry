using UnrealBuildTool;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

public class CanopySimExternal : ModuleRules
{
    static string Digest(string file)
    {
        using FileStream stream = File.OpenRead(file);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }

    static string Run(string executable, params string[] args)
    {
        var start = new ProcessStartInfo(executable) { RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false };
        foreach (string arg in args) start.ArgumentList.Add(arg);
        using Process process = Process.Start(start) ?? throw new BuildException("Missing qualified native tool: " + executable);
        string output = process.StandardOutput.ReadToEnd();
        string error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0) throw new BuildException("Native tool failed: " + executable + " " + error);
        return output.Trim();
    }

    static string EnvironmentValue(string name) => Environment.GetEnvironmentVariable(name) ?? "";
    static void Require(bool condition, string reason)
    {
        if (!condition) throw new BuildException("Canopy core admission rejected: " + reason);
    }

    static void CheckInputs(string root, JsonElement manifest, List<string> external)
    {
        string[] fixedInputs = {
            "pyproject.toml", "uv.lock", "config/toolchains.json", "CMakeLists.txt",
            "CMakePresets.json", "dependencies/native-lock.json", "dependencies/vcpkg.json",
            "dependencies/vcpkg-configuration.json", "scripts/bootstrap_native.py",
            "scripts/build_common.py", "scripts/build_core.py", "scripts/build_game.py"
        };
        var paths = new List<string>(fixedInputs);
        void Gather(string directory, string pattern, SearchOption option)
        {
            string absolute = Path.Combine(root, directory);
            Require(Directory.Exists(absolute), "missing input directory " + directory);
            external.Add(absolute); // Detect additions as well as changes to listed files.
            if (option == SearchOption.AllDirectories)
                external.AddRange(Directory.GetDirectories(absolute, "*", SearchOption.AllDirectories));
            paths.AddRange(Directory.GetFiles(absolute, pattern, option)
                .Select(path => Path.GetRelativePath(root, path).Replace('\\', '/')));
        }
        Gather("dependencies/upstream", "*.whl", SearchOption.TopDirectoryOnly);
        Gather("dependencies/triplets", "*.cmake", SearchOption.TopDirectoryOnly);
        Gather("core", "*", SearchOption.AllDirectories);
        JsonElement hashes = manifest.GetProperty("inputs_sha256");
        Require(hashes.ValueKind == JsonValueKind.Object && hashes.EnumerateObject().Count() == paths.Count,
            "core input set differs from Release manifest");
        foreach (string relative in paths)
        {
            string absolute = Path.GetFullPath(Path.Combine(root, relative));
            Require(File.Exists(absolute) && hashes.TryGetProperty(relative, out JsonElement expected) &&
                expected.GetString() == Digest(absolute), "stale or missing core input " + relative);
            external.Add(absolute);
        }
    }

    static void CheckToolchain(string root, string platform, JsonElement manifest, JsonElement profile, List<string> external)
    {
        JsonElement compiler = manifest.GetProperty("compiler");
        JsonElement sdk = manifest.GetProperty("host_sdk");
        string triplet = profile.GetProperty("qualified_triplet").GetString() ?? "";
        Require(manifest.GetProperty("vcpkg_triplet").GetString() == triplet, "qualified triplet mismatch");
        string executable;
        if (platform == "mac-arm64")
        {
            string xcode = Run("xcodebuild", "-version");
            Require(xcode.Split('\n')[0].Trim() == "Xcode " + profile.GetProperty("xcode").GetString() &&
                sdk.GetProperty("xcode").GetString() == xcode, "approved Xcode/manifest mismatch");
            string sdkPath = Path.GetFullPath(Run("xcrun", "--sdk", "macosx", "--show-sdk-path"));
            sdkPath = new DirectoryInfo(sdkPath).ResolveLinkTarget(true)?.FullName ?? sdkPath;
            Require(sdk.GetProperty("macos_sdk_path").GetString() == sdkPath &&
                sdk.GetProperty("macos_sdk").GetString() == Run("xcrun", "--sdk", sdkPath, "--show-sdk-version") &&
                manifest.GetProperty("deployment_target").GetString() == profile.GetProperty("deployment_target").GetString(),
                "approved macOS SDK/deployment target mismatch");
            executable = Path.GetFullPath(Run("xcrun", "--sdk", sdkPath, "--find", "clang++"));
            Require(compiler.GetProperty("id").GetString() == "AppleClang", "compiler family mismatch");
        }
        else if (platform == "win64")
        {
            string vs = EnvironmentValue("VisualStudioVersion");
            string msvc = EnvironmentValue("VCToolsVersion");
            string winSdk = EnvironmentValue("WindowsSDKVersion").TrimEnd('\\', '/');
            Require(vs == profile.GetProperty("visual_studio").GetString() &&
                msvc.StartsWith(profile.GetProperty("msvc").GetString() + ".", StringComparison.Ordinal) &&
                winSdk.StartsWith(profile.GetProperty("windows_sdk").GetString() + ".", StringComparison.Ordinal) &&
                sdk.GetProperty("visual_studio").GetString() == vs && sdk.GetProperty("msvc").GetString() == msvc &&
                (sdk.GetProperty("windows_sdk").GetString() ?? "").TrimEnd('\\', '/') == winSdk,
                "approved Windows VS/MSVC/SDK mismatch");
            executable = EnvironmentValue("PATH").Split(Path.PathSeparator)
                .Select(dir => Path.Combine(dir, "cl.exe")).FirstOrDefault(File.Exists) ?? "";
            Require(compiler.GetProperty("id").GetString() == "MSVC", "compiler family mismatch");
        }
        else
        {
            string tool = EnvironmentValue("CANOPY_LINUX_TOOLCHAIN_ROOT");
            string sysroot = EnvironmentValue("CANOPY_LINUX_SYSROOT");
            Require(Path.IsPathFullyQualified(tool) && Path.IsPathFullyQualified(sysroot) &&
                Path.GetFullPath(tool) == Path.GetFullPath(sysroot) &&
                Path.GetFileName(Path.GetDirectoryName(tool)) == profile.GetProperty("archive").GetProperty("root").GetString(),
                "fixed v26 Linux toolchain/sysroot mismatch");
            executable = Path.Combine(tool, "bin/clang++");
            Require(compiler.GetProperty("id").GetString() == "Clang" &&
                compiler.GetProperty("version").GetString()!.Contains(profile.GetProperty("clang").GetString()!, StringComparison.Ordinal),
                "approved Linux compiler mismatch");
            string include = Path.GetFullPath(EnvironmentValue("CANOPY_LINUX_LIBCXX_INCLUDE"));
            string libdir = Path.GetFullPath(EnvironmentValue("CANOPY_LINUX_LIBCXX_LIBDIR"));
            Require(include == Path.Combine(tool, "include/c++/v1") && libdir == Path.Combine(tool, "lib64") &&
                sdk.GetProperty("libcxx_include").GetString() == include, "Linux libc++ sysroot mismatch");
            foreach (string library in new[] { "libc++", "libc++abi" })
            {
                string file = Path.Combine(libdir, library + ".a");
                Require(sdk.GetProperty(library == "libc++" ? "libcxx_library_sha256" : "libcxxabi_library_sha256").GetString() == Digest(file),
                    "Linux " + library + " artifact mismatch");
                external.Add(file);
            }
            // The qualified libc++ headers are part of the ABI, not render-only data.
            using var aggregate = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            foreach (string header in Directory.GetFiles(include, "*", SearchOption.AllDirectories).OrderBy(p => p, StringComparer.Ordinal))
            {
                string relative = Path.GetRelativePath(include, header).Replace('\\', '/');
                aggregate.AppendData(Encoding.UTF8.GetBytes(relative));
                aggregate.AppendData(Convert.FromHexString(Digest(header)));
                external.Add(header);
            }
            Require(Convert.ToHexString(aggregate.GetHashAndReset()).ToLowerInvariant() ==
                sdk.GetProperty("libcxx_headers_sha256").GetString(), "Linux libc++ header mismatch");
        }
        Require(File.Exists(executable) && Digest(executable) == compiler.GetProperty("binary_sha256").GetString(),
            "native compiler binary differs from core manifest");
        external.Add(executable);
        string identity = Run(executable, platform == "win64" ? "/?" : "--version").Split('\n')[0].Trim();
        Require(identity == compiler.GetProperty("version").GetString(), "native compiler version differs from core manifest");
    }

    public CanopySimExternal(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        string root = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../../../"));
        string manifestPath = Path.Combine(root, ".build/core/release/canopy-core-manifest.json");
        Require(File.Exists(manifestPath), "build the qualified Release core first");
        ExternalDependencies.Add(manifestPath);
        string configPath = Path.Combine(root, "config/toolchains.json");
        ExternalDependencies.Add(configPath);
        // Select the string overload: UBT's rules compiler does not reference System.Memory.
        using JsonDocument configDocument = JsonDocument.Parse(json: File.ReadAllText(configPath));
        using JsonDocument document = JsonDocument.Parse(json: File.ReadAllText(manifestPath));
        JsonElement manifest = document.RootElement;
        Require(manifest.GetProperty("schema").GetInt32() == 1 &&
            manifest.GetProperty("scope").GetString() == "standalone-core" &&
            manifest.GetProperty("config").GetString() == "Release", "core manifest schema/config mismatch");
        JsonElement engine = configDocument.RootElement.GetProperty("unreal").GetProperty("engine");
        Require(Target.Version.MajorVersion == engine.GetProperty("major").GetInt32() &&
            Target.Version.MinorVersion == engine.GetProperty("minor").GetInt32() &&
            Target.Version.PatchVersion == engine.GetProperty("patch").GetInt32(), "approved engine version mismatch");
        string platform = Target.Platform == UnrealTargetPlatform.Win64 ? "win64" :
            Target.Platform == UnrealTargetPlatform.Mac ? "mac-arm64" :
            Target.Platform == UnrealTargetPlatform.Linux ? "linux-x64" : "unsupported";
        Require(platform != "unsupported" && manifest.GetProperty("platform").GetString() == platform, "core platform mismatch");
        JsonElement profile = configDocument.RootElement.GetProperty("unreal").GetProperty(platform);
        JsonElement abi = manifest.GetProperty("abi");
        bool mac = platform == "mac-arm64";
        Require(abi.GetProperty("crt").GetString() == (mac || platform == "linux-x64" ? "libc++" : "MD") &&
            !abi.GetProperty("rtti").GetBoolean() && abi.GetProperty("exceptions").GetBoolean() == mac,
            "core CRT/RTTI/exception ABI mismatch");
        CheckInputs(root, manifest, ExternalDependencies);
        CheckToolchain(root, platform, manifest, profile, ExternalDependencies);
        JsonElement library = manifest.GetProperty("library");
        string path = Path.GetFullPath(library.GetProperty("path").GetString() ?? "");
        string buildRoot = Path.GetFullPath(Path.Combine(root, ".build/core")) + Path.DirectorySeparatorChar;
        Require(path.StartsWith(buildRoot, StringComparison.OrdinalIgnoreCase) && File.Exists(path) &&
            Digest(path) == library.GetProperty("sha256").GetString(), "core archive outside build tree or digest mismatch");
        ExternalDependencies.Add(path);
        PublicIncludePaths.Add(Path.Combine(root, "core/include"));
        PublicAdditionalLibraries.Add(path);
    }
}
