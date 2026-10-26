"""Shared package names and integrity checks for release targets."""
import hashlib
import json
from pathlib import Path
import re


GROUPS = {
    "package-linux-appimage-x86_64": ("appimage", None),
    "package-windows-x64": ("windows", None),
    "package-macos26-arm64": ("macos", None),
    **{f"package-linux-{name}-amd64": ("linux", name)
       for name in ("jammy", "noble", "bookworm", "trixie")},
}


def hash_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_packages(group, version):
    if group not in GROUPS or not isinstance(version, str) or not re.fullmatch(r"\d+\.\d+\.\d+(?:\.\d+)?", version):
        raise ValueError("Unknown target or invalid release version")
    platform, distro = GROUPS[group]
    if platform == "windows":
        return {f"qEmby-v{version}-Win-x64.zip", f"qEmby-v{version}-Win-x64-Setup.exe"}
    if platform == "macos":
        return {f"qEmby-v{version}-macOS26-arm64.dmg"}
    if platform == "appimage":
        return {f"qemby-{version}-x86_64.AppImage"}
    return {f"qemby_{version}-{distro}_amd64.deb"}


def validate_artifact(folder, run_id, commit):
    folder = Path(folder)
    if folder.is_symlink() or folder.name not in GROUPS:
        raise ValueError("Unexpected artifact group")
    for path in folder.iterdir():
        if path.is_symlink() or not path.is_file() or path.resolve().parent != folder.resolve():
            raise ValueError("Artifact contains nested or linked paths")
    info = json.loads((folder / "build-info.json").read_text(encoding="utf-8"))
    if info.get("schema_version") != 1 or info.get("group") != folder.name:
        raise ValueError("Unsupported or mismatched artifact metadata")
    if str(info.get("run")) != str(run_id) or info.get("commit") != commit:
        raise ValueError("Artifact does not belong to the requested GitHub run and commit")
    if info.get("platform") != GROUPS[folder.name][0]:
        raise ValueError("Artifact platform mismatch")
    expected = expected_packages(folder.name, info.get("version"))
    packages = info.get("packages", [])
    if not isinstance(packages, list) or len(packages) != len(expected) or set(packages) != expected:
        raise ValueError("Package names or count do not match the target")
    validation = info.get("validation", {})
    if info["platform"] == "appimage":
        reports = validation.get("distro_smoke", [])
        if (validation.get("dependency_closure") != "passed" or validation.get("glibc_maximum") != "2.35"
                or len(reports) != 4 or {r.get("distro") for r in reports} != {'jammy', 'noble', 'bookworm', 'trixie'}
                or any(any(r.get(key) != 'passed' for key in ('x11', 'native_wayland', 'static_runtime'))
                       for r in reports)):
            raise ValueError("AppImage compatibility validation is missing")
    if info["platform"] == "linux":
        if (validation.get("deb_lifecycle") != "passed" or validation.get("qt_preserved") is not True
                or validation.get("global_probes_preserved") is not True):
            raise ValueError("DEB safety validation is missing")
    elif info["platform"] == "windows" and validation.get("native_dll_loading") != "passed":
        raise ValueError("Windows dependency validation is missing")
    elif info["platform"] == "macos" and (
            validation.get("codesign") != "passed" or validation.get("dmg_verify") != "passed"):
        raise ValueError("macOS package validation is missing")
    checksums = {}
    for line in (folder / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([a-f0-9]{64})  ([^/\\]+)", line)
        if not match or match[2] in checksums:
            raise ValueError("Invalid or duplicate checksum entry")
        checksums[match[2]] = match[1]
    if set(checksums) != expected:
        raise ValueError("Checksum manifest does not match the expected packages")
    if {p.name for p in folder.iterdir()} != expected | {"SHA256SUMS.txt", "build-info.json"}:
        raise ValueError("Unexpected files in artifact")
    for name, digest in checksums.items():
        if hash_file(folder / name) != digest:
            raise ValueError(f"Package checksum mismatch: {name}")
        if info["platform"] == "appimage" and any(r.get('sha256') != digest for r in reports):
            raise ValueError("AppImage smoke reports refer to a different binary")
    return info
