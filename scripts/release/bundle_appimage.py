"""Bundle the application and Qt plugin ELF closure; retain only host glibc/drivers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


# These are supplied by the host glibc or GPU driver installation. GLVND,
# Wayland, XCB, audio libraries and libstdc++ are deliberately bundled.
HOST_LIBRARY = re.compile(
    r"^(?:ld-linux.*|lib(?:c|m|pthread|dl|rt|resolv|util|anl|nss_[^.]+)\.so(?:\..*)?"
    r"|lib(?:GLX_mesa|EGL_mesa|nvidia[^.]*)\.so(?:\..*)?)$")


def output(*args, env=None):
    return subprocess.check_output(list(map(str, args)), text=True, env=env, stderr=subprocess.STDOUT)


def elf(path):
    if not path.is_file():
        return False
    with path.open('rb') as stream:
        return stream.read(4) == b'\x7fELF'


def owners(path):
    for candidate in (path, path.resolve()):
        result = subprocess.run(['dpkg-query', '-S', str(candidate)], text=True, capture_output=True)
        if result.returncode == 0:
            return {line.split(': ')[0].split(':')[0] for line in result.stdout.splitlines()}
    raise RuntimeError(f'Cannot identify bundled library license owner: {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--appdir', type=Path, required=True)
    args = parser.parse_args()
    appdir = args.appdir.resolve()
    libdir = appdir / 'usr/lib/qemby'
    if not (libdir / 'qEmbyApp').is_file():
        raise SystemExit('AppDir is missing the private installed executable')
    qtpaths = shutil.which('qtpaths6') or '/usr/lib/qt6/bin/qtpaths'
    plugins = Path(output(qtpaths, '--plugin-dir').strip())
    copyright_packages = set()
    for category in ('platforms', 'imageformats', 'iconengines', 'platforminputcontexts',
                     'xcbglintegrations', 'wayland-shell-integration',
                     'wayland-decoration-client', 'wayland-graphics-integration-client', 'tls'):
        source = plugins / category
        if source.is_dir():
            shutil.copytree(source, libdir / 'plugins' / category, dirs_exist_ok=True)
            for plugin in source.rglob('*.so'):
                copyright_packages.update(owners(plugin))
    for name in ('platforms/libqxcb.so', 'platforms/libqwayland-generic.so'):
        if not (libdir / 'plugins' / name).is_file():
            raise RuntimeError(f'Required Qt plugin not installed: {name}')
    (libdir / 'qt.conf').write_text('[Paths]\nPrefix=.\nLibraries=.\nPlugins=plugins\n', encoding='utf-8')
    shutil.copytree('/usr/share/X11/xkb', appdir / 'usr/share/X11/xkb', symlinks=False)
    root = Path(__file__).resolve().parents[2]
    docs = appdir / 'usr/share/doc/qemby-bundled'
    docs.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'LICENSE', docs / 'qEmby-LICENSE')
    env = dict(os.environ, LD_LIBRARY_PATH=f'{libdir}:{root}/build/cloud/bin')
    queue = [p for p in libdir.rglob('*') if elf(p)]
    origins = {}
    visited = set()
    while queue:
        binary = queue.pop()
        if binary in visited:
            continue
        visited.add(binary)
        listing = output('ldd', binary, env=env)
        if 'not found' in listing:
            raise RuntimeError(f'Unresolved ELF dependency: {binary}\n{listing}')
        for name, original in re.findall(r'^\s*(\S+) => (/\S+) \(', listing, re.M):
            if HOST_LIBRARY.fullmatch(name):
                continue
            source = Path(original).resolve()
            if source.is_relative_to(appdir):
                continue
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            if name in origins and origins[name] != digest:
                raise RuntimeError(f'Conflicting libraries with the same SONAME: {name}')
            origins[name] = digest
            target = libdir / name
            if not target.exists():
                shutil.copy2(source, target)
                queue.append(target)
            elif hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise RuntimeError(f'Bundled library collision: {name}')
            copyright_packages.update(owners(Path(original)))
    # Preserve distribution copyright/license notices for every collected library.
    for package in sorted(copyright_packages | {'qt6-wayland', 'xkb-data'}):
        notice = Path('/usr/share/doc') / package / 'copyright'
        if notice.is_file():
            shutil.copy2(notice, docs / f'{package}.copyright')
        else:
            raise RuntimeError(f'Missing bundled dependency copyright: {package}')
    maximum = (2, 35)
    for binary in visited:
        relative = os.path.relpath(libdir, binary.parent)
        rpath = '$ORIGIN' if relative == '.' else '$ORIGIN/' + relative
        subprocess.run(['patchelf', '--force-rpath', '--set-rpath', rpath, str(binary)], check=True)
        versions = re.findall(r'\bGLIBC_(\d+)\.(\d+)\b', output('readelf', '--version-info', binary))
        if any(tuple(map(int, version)) > maximum for version in versions):
            raise RuntimeError(f'AppImage exceeds the glibc 2.35 baseline: {binary.name}')
        listing = output('ldd', binary, env=dict(os.environ, LD_LIBRARY_PATH=str(libdir)))
        if 'not found' in listing:
            raise RuntimeError(f'Broken deployed ELF: {binary.name}\n{listing}')
        for name, location in re.findall(r'^\s*(\S+) => (/\S+) \(', listing, re.M):
            if not HOST_LIBRARY.fullmatch(name) and not Path(location).resolve().is_relative_to(appdir):
                raise RuntimeError(f'Non-system dependency escaped bundle: {name}')
    (docs / 'bundle-info.json').write_text(json.dumps({
        'glibc_maximum': '2.35', 'elf_files': len(visited),
        'bundled_libraries': sorted(origins), 'qt_plugins': sorted(
            str(p.relative_to(libdir / 'plugins')) for p in (libdir / 'plugins').rglob('*.so')),
        'host_requirements': ['glibc >= 2.35', 'GPU drivers', 'desktop session', 'fonts'],
    }, indent=2) + '\n', encoding='utf-8')
    print(f'AppImage dependency closure verified: {len(visited)} ELF files')


if __name__ == '__main__':
    main()
