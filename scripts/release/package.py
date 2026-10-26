"""Package an already-built Release. Does not configure or compile the project."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys


def run(*args):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def package_windows(root, build, dist, version):
    app = dist / "qEmby"
    app.mkdir()
    binaries = build / "bin"
    shutil.copy2(binaries / "qEmbyApp.exe", app)
    for path in binaries.glob("*.dll"):
        shutil.copy2(path, app)
    for required in ("qEmbyCore.dll", "libmpv-2.dll"):
        if not (app / required).is_file():
            raise RuntimeError(f"Missing runtime: {required}")
    run("windeployqt", "--release", "--compiler-runtime", app / "qEmbyApp.exe")
    shutil.copy2(root / "LICENSE", app)
    shutil.make_archive(str(dist / f"qEmby-v{version}-Win-x64"), "zip", dist, "qEmby")
    candidates = [shutil.which("ISCC"), r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
                  r"C:\Program Files\Inno Setup 6\ISCC.exe"]
    iscc = next((p for p in candidates if p and Path(p).is_file()), None)
    if not iscc:
        raise RuntimeError("Inno Setup ISCC not found")
    run(iscc, f"/DAppVersion={version}", f"/DSourceDir={app}", f"/O{dist}", root / "installer/qEmby.iss")
    if not (dist / f"qEmby-v{version}-Win-x64-Setup.exe").is_file():
        raise RuntimeError("Installer not generated")


def mac_dependencies(binary):
    return [line.strip().split(" (compatibility", 1)[0]
            for line in output("otool", "-L", binary).splitlines()[1:]]


def package_macos(root, build, dist, version):
    if output("uname", "-m") != "arm64":
        raise RuntimeError("Apple Silicon runner required")
    app = dist / "qEmby.app"
    shutil.copytree(build / "bin/qEmbyApp.app", app, symlinks=True)
    frameworks = app / "Contents/Frameworks"
    frameworks.mkdir(exist_ok=True)
    executable = app / "Contents/MacOS/qEmbyApp"
    info = app / "Contents/Info.plist"
    with info.open("rb") as stream:
        values = plistlib.load(stream)
    values.update(CFBundleIdentifier="io.github.AlanHJ.qEmby", CFBundleName="qEmby",
                  CFBundleShortVersionString=version)
    with info.open("wb") as stream:
        plistlib.dump(values, stream)
    brew = Path(output("brew", "--prefix"))
    run("macdeployqt", app, "-no-codesign", f"-libpath={build / 'bin'}", f"-libpath={brew / 'lib'}")
    # Resolve app libraries and the full Homebrew libmpv closure, including
    # libraries referenced by Qt plugins. No build-machine paths may remain.
    queue = []
    for path in app.rglob("*"):
        if path.is_file() and not path.is_symlink():
            if "Mach-O" in output("file", "-b", path):
                queue.append(path)
    seen = set()
    origins = {}
    while queue:
        binary = queue.pop(0)
        if binary in seen:
            continue
        seen.add(binary)
        if "arm64" not in output("lipo", "-archs", binary):
            raise RuntimeError(f"Non-arm64 binary: {binary.name}")
        for dependency in mac_dependencies(binary):
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            candidates = []
            if dependency.startswith("@executable_path/"):
                candidates.append(executable.parent / dependency.removeprefix("@executable_path/"))
            elif dependency.startswith("@loader_path/"):
                candidates.append(binary.parent / dependency.removeprefix("@loader_path/"))
            elif dependency.startswith("/"):
                candidates.append(Path(dependency))
            elif dependency.startswith("@rpath/"):
                tail = dependency.removeprefix("@rpath/")
                candidates.extend([frameworks / tail, build / "bin" / tail, brew / "lib" / tail])
            resolved = next((p.resolve() for p in candidates if p.exists()), None)
            if resolved is None:
                raise RuntimeError(f"Unresolved dependency in {binary.name}: {dependency}")
            if resolved.is_relative_to(app):
                continue
            if ".framework/" in str(resolved):
                raise RuntimeError(f"Framework not deployed by macdeployqt: {resolved.name}")
            target = frameworks / resolved.name
            if target.name in origins and origins[target.name] != resolved:
                raise RuntimeError(f"Library name collision: {target.name}")
            origins[target.name] = resolved
            if not target.exists():
                shutil.copy2(resolved, target)
                target.chmod(0o755)
                run("install_name_tool", "-id", f"@rpath/{target.name}", target)
                queue.append(target)
            relative = os.path.relpath(target, binary.parent)
            run("install_name_tool", "-change", dependency, f"@loader_path/{relative}", binary)
    for binary in seen:
        for dependency in mac_dependencies(binary):
            if dependency.startswith("/") and not dependency.startswith(("/System/Library/", "/usr/lib/")):
                raise RuntimeError(f"External dependency remains: {binary.name}")
    shutil.copy2(root / "LICENSE", app / "Contents/Resources/LICENSE")
    # Test artifacts use ad-hoc signing; Developer ID/notarization is a later step.
    for binary in sorted(seen, key=lambda p: len(p.parts), reverse=True):
        run("codesign", "--force", "--sign", "-", binary)
    for framework in frameworks.glob("*.framework"):
        run("codesign", "--force", "--sign", "-", framework)
    run("codesign", "--force", "--sign", "-", app)
    run("codesign", "--verify", "--deep", "--strict", app)
    image = dist / "dmg-root"
    image.mkdir()
    shutil.copytree(app, image / app.name, symlinks=True)
    (image / "Applications").symlink_to("/Applications")
    dmg = dist / f"qEmby-v{version}-macOS26-arm64.dmg"
    run("hdiutil", "create", "-volname", "qEmby", "-srcfolder", image, "-format", "UDZO", dmg)
    run("hdiutil", "verify", dmg)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/cloud"))
    parser.add_argument("--platform", choices=["windows", "macos", "linux"], required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    version = re.search(r"project\(qEmby VERSION ([\d.]+)", (root / "CMakeLists.txt").read_text())[1]
    dist = root / "dist/cloud"
    dist.mkdir(parents=True, exist_ok=True)
    build = args.build_dir.resolve()
    if args.platform == "windows":
        package_windows(root, build, dist, version)
    elif args.platform == "macos":
        package_macos(root, build, dist, version)
    artifacts = [p for p in dist.iterdir() if p.suffix in (".exe", ".zip", ".dmg", ".deb")]
    if not artifacts:
        raise RuntimeError("No packages generated")
    hashes = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in sorted(artifacts))
    (dist / "SHA256SUMS.txt").write_text(hashes, encoding="utf-8")
    metadata = {"version": version, "commit": os.environ.get("GITHUB_SHA", "local"),
                "platform": args.platform, "run": os.environ.get("GITHUB_RUN_ID"),
                "packages": [p.name for p in sorted(artifacts)]}
    (dist / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
