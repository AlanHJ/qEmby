"""Deploy client-side Wayland plugins not selected by Qt's Widgets-only deploy."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import os


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-lib-dir', type=Path, required=True)
    parser.add_argument('--qt-plugin-dir', type=Path, required=True)
    parser.add_argument('--private-dir', type=Path, required=True)
    args = parser.parse_args()
    private = args.private_dir.resolve()
    libraries = args.qt_lib_dir.resolve()
    if not (private / 'qEmbyApp').is_file() or private == libraries or libraries.is_relative_to(private):
        raise RuntimeError('Expected an installed qEmby private directory separate from the Qt SDK')
    if not shutil.which('patchelf'):
        raise RuntimeError('Private Qt Wayland deployment requires patchelf')
    queue = []
    def copy_library(name):
        destination = private / name
        if destination.exists():
            return
        source = (libraries / name).resolve(strict=True)
        target = private / source.name
        if not target.exists():
            shutil.copy2(source, target)
            queue.append(target)
        if destination != target:
            destination.symlink_to(target.name)
    plugins = ['platforms/libqwayland-generic.so', 'platforms/libqwayland-egl.so',
               'wayland-graphics-integration-client/libqt-plugin-wayland-egl.so']
    for category in ('wayland-shell-integration', 'wayland-decoration-client'):
        plugins.extend(str(p.relative_to(args.qt_plugin_dir)) for p in (args.qt_plugin_dir / category).glob('*.so'))
    for name in plugins:
        source = args.qt_plugin_dir / name
        if not source.is_file():
            raise RuntimeError(f'Missing SDK Wayland plugin: {name}')
        target = private / 'plugins' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        queue.append(target)
    visited = set()
    while queue:
        binary = queue.pop()
        if binary in visited:
            continue
        visited.add(binary)
        relative = os.path.relpath(private, binary.parent)
        rpath = '$ORIGIN' if relative == '.' else '$ORIGIN/' + relative
        subprocess.run(['patchelf', '--set-rpath', rpath, str(binary)], check=True)
        dynamic = subprocess.check_output(['readelf', '-d', str(binary)], text=True)
        for name in re.findall(r'Shared library: \[(libQt6[^]]+)\]', dynamic):
            copy_library(name)
    print(f'Deployed {len(plugins)} client Wayland plugins and their private Qt dependencies')


if __name__ == '__main__':
    main()
