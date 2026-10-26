"""Exercise DEB lifecycle and Qt preservation in a disposable CI container only."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tarfile
import tempfile


def run(*command):
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), check=True)


def capture(*command):
    return subprocess.check_output(list(map(str, command)))


def validate_package(package, root):
    files = []
    with tarfile.open(fileobj=io.BytesIO(capture("dpkg-deb", "--fsys-tarfile", package)), mode="r:") as archive:
        for member in archive.getmembers():
            if member.isdir():
                continue
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts:
                raise RuntimeError("Unsafe package path")
            name = path.as_posix().removeprefix("./")
            permitted = name == "usr/bin/qEmbyApp" or name.startswith((
                "usr/lib/qemby/", "usr/share/doc/qEmby/")) or name in {
                "usr/share/applications/qemby.desktop", "usr/share/metainfo/qemby.metainfo.xml",
                "usr/share/icons/hicolor/scalable/apps/qemby.svg"}
            if not permitted or path.name.startswith(("libQt6", "libmpv")) or path.name == "qt.conf":
                raise RuntimeError(f"Native package writes outside its private payload: {name}")
            if not member.isfile():
                raise RuntimeError(f"Unexpected linked or special package entry: {name}")
            files.append("/" + name)
    controls = {}
    with tarfile.open(fileobj=io.BytesIO(capture("dpkg-deb", "--ctrl-tarfile", package)), mode="r:") as archive:
        for member in archive.getmembers():
            if member.isfile():
                name = PurePosixPath(member.name).name
                if name in controls:
                    raise RuntimeError("Duplicate DEB control entry")
                controls[name] = archive.extractfile(member).read()
    for name in ("preinst", "prerm", "triggers"):
        if name in controls:
            raise RuntimeError(f"Unexpected maintainer hook: {name}")
    for name in ("postinst", "postrm"):
        expected = (root / f"packaging/linux/debian/{name}.in").read_bytes().replace(b"\r\n", b"\n")
        if controls.get(name) != expected:
            raise RuntimeError(f"Generated {name} differs from the reviewed non-destructive script")
        run("sh", "-n", root / f"packaging/linux/debian/{name}.in")
    if capture("dpkg-deb", "--field", package, "Package").strip() != b"qemby":
        raise RuntimeError("Unexpected package identity")
    depends = capture("dpkg-deb", "--field", package, "Depends").decode()
    if "libqt6core6" not in depends or "libmpv" not in depends:
        raise RuntimeError("Missing native Qt/mpv dependencies")
    return files


def fingerprint(path):
    info = path.lstat()
    if path.is_symlink():
        return ("link", os.readlink(path), stat.S_IMODE(info.st_mode))
    return ("file", hashlib.sha256(path.read_bytes()).hexdigest(), stat.S_IMODE(info.st_mode))


def shared_snapshot(probes):
    paths = set(probes)
    for pattern in ("*-linux-gnu/libQt6*.so*", "*-linux-gnu/qt6/plugins/**/*"):
        paths.update(p for p in Path("/usr/lib").glob(pattern) if p.is_file() or p.is_symlink())
    for base in ("/etc/ld.so.conf", "/etc/ld.so.cache", "/etc/environment"):
        if Path(base).exists():
            paths.add(Path(base))
    for pattern in ("ld.so.conf.d/*", "profile.d/*"):
        paths.update(p for p in Path("/etc").glob(pattern) if p.is_file() or p.is_symlink())
    return {str(path): fingerprint(path) for path in sorted(paths)}


def assert_preserved(baseline, probes, phase):
    current = shared_snapshot(probes)
    if current != baseline:
        changed = sorted(name for name in baseline.keys() | current.keys() if baseline.get(name) != current.get(name))
        raise RuntimeError(f"Shared Qt/configuration changed during {phase}: {changed}")
    print(f"PASS {phase}: {len(baseline)} shared Qt/configuration entries preserved", flush=True)


def fixture(directory, name, version, entries):
    tree = directory / name
    (tree / "DEBIAN").mkdir(parents=True)
    (tree / "DEBIAN/control").write_text(
        f"Package: {name}\nVersion: {version}\nArchitecture: amd64\nMaintainer: qEmby CI\n"
        "Description: disposable packaging safety fixture\n", encoding="utf-8")
    for relative, data in entries.items():
        target = tree / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    package = directory / f"{name}.deb"
    run("dpkg-deb", "--build", tree, package)
    return package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deb", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    # This test deliberately creates system-path probes. Never run it on a host
    # desktop or a reused environment containing a real qemby installation.
    if (os.name != "posix" or os.environ.get("GITHUB_ACTIONS") != "true"
            or os.environ.get("QEMBY_DEB_SAFETY_TEST") != "1"
            or not Path("/.dockerenv").is_file() or os.geteuid() != 0):
        raise SystemExit("DEB lifecycle tests require an explicitly enabled disposable GitHub container")
    root = Path(__file__).resolve().parents[2]
    package = args.deb.resolve()
    report = args.report.resolve()
    if not package.is_relative_to(root / "dist/cloud") or not report.is_relative_to(root / "build"):
        raise SystemExit("Test inputs and report must stay inside this CI workspace")
    for name in ("qemby", "qemby-qt-safety-fixture"):
        if subprocess.run(["dpkg-query", "--status", name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
            raise SystemExit("Safety test requires a clean container without fixture or qemby packages")
    payload = validate_package(package, root)
    probes = {
        Path("/usr/lib/libQt6Core.so.6.987654"): b"unowned Qt library sentinel\n",
        Path("/usr/plugins/imageformats/libqsvg.so"): b"unowned SVG plugin sentinel\n",
        Path("/usr/translations/qt_qemby_safety_probe.qm"): b"unowned translation sentinel\n",
        Path("/usr/bin/qt.conf"): b"[Paths]\nLibraries = lib/qemby\nPlugins = plugins\n",
    }
    external_plugin = Path("/usr/plugins/platforms/libqxcb.so")
    obsolete = Path("/usr/lib/libQt6Core.so.6.987653-qemby-owned-fixture")
    obsolete_private = Path("/usr/lib/qemby/obsolete-safety-fixture")
    for path in [*probes, external_plugin, obsolete, obsolete_private]:
        if path.exists() or path.is_symlink():
            raise SystemExit(f"Refusing to overwrite an existing probe path: {path}")
    with tempfile.TemporaryDirectory(prefix="qemby-deb-safety-") as temporary:
        directory = Path(temporary)
        other = fixture(directory, "qemby-qt-safety-fixture", "1.0", {
            "usr/plugins/platforms/libqxcb.so": b"Qt plugin owned by a different package\n"})
        old = fixture(directory, "qemby", "0.0.0~safety", {
            str(obsolete).lstrip("/"): b"obsolete package-owned Qt library\n",
            str(obsolete_private).lstrip("/"): b"obsolete private payload\n"})
        run("dpkg", "--install", other)
        run("dpkg", "--install", old)
        for path, data in probes.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as stream:
                stream.write(data)
        protected = [*probes, external_plugin]
        baseline = shared_snapshot(protected)
        run("apt-get", "install", "-y", "--no-install-recommends", package)
        assert_preserved(baseline, protected, "upgrade")
        if obsolete.exists() or obsolete_private.exists():
            raise RuntimeError("dpkg did not remove obsolete files owned by the previous package")
        for name in ("qEmbyApp", "libqEmbyCore.so", "libQWKCore.so", "libQWKWidgets.so"):
            dynamic = capture("readelf", "-d", f"/usr/lib/qemby/{name}").decode()
            entries = [line for line in dynamic.splitlines() if "RPATH" in line or "RUNPATH" in line]
            if not entries or any(line.split("[", 1)[1].split("]", 1)[0] != "$ORIGIN" for line in entries):
                raise RuntimeError(f"Non-private runtime search path in {name}")
        run("apt-get", "install", "-y", "--no-install-recommends", "--reinstall", package)
        assert_preserved(baseline, protected, "reinstall")
        run("dpkg", "--remove", "qemby")
        assert_preserved(baseline, protected, "remove")
        if any(Path(name).exists() or Path(name).is_symlink() for name in payload):
            raise RuntimeError("Package payload remains after remove")
        run("apt-get", "install", "-y", "--no-install-recommends", package)
        assert_preserved(baseline, protected, "install")
        run("dpkg", "--purge", "qemby")
        assert_preserved(baseline, protected, "purge")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({
        "deb_lifecycle": "passed", "qt_preserved": True, "global_probes_preserved": True,
        "cases": ["upgrade", "reinstall", "remove", "install", "purge"],
        "protected_entries": len(baseline), "private_runpath": "$ORIGIN",
        "obsolete_owned_files_removed": True,
    }, indent=2) + "\n", encoding="utf-8")
    print("DEB lifecycle and Qt isolation checks passed", flush=True)


if __name__ == "__main__":
    main()
