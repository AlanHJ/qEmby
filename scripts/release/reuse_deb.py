"""Reuse a CI binary only when every compiled input matches; repackage privately."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request


def compiled_inputs(manifest):
    return {name: digest for name, digest in manifest['files'].items()
            if name.startswith(('src/', 'libs/qwindowkit/')) or name in (
                'CMakeLists.txt', 'cmake/QEmbyPlatformDeps.cmake', 'scripts/release/playback-probe.cpp')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--source-run', type=int)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    reuse = root / 'build/reuse'
    build = root / 'build/cloud'
    if args.check:
        old = reuse / 'release-source.json'
        ready = (old.is_file() and (reuse / 'build/cloud/bin/qEmbyPlaybackProbe').is_file()
                 and len(list((reuse / 'dist/cloud').glob('*.deb'))) == 1)
        if ready:
            current = json.loads((root / 'release-source.json').read_text())
            previous = json.loads(old.read_text())
            ready = compiled_inputs(current) == compiled_inputs(previous)
        with Path(os.environ['GITHUB_OUTPUT']).open('a') as stream:
            stream.write(f'reusable={str(ready).lower()}\n')
        if not ready:
            print('No complete matching compilation; build this distro normally')
            return
        request = urllib.request.Request(
            f'https://api.github.com/repos/AlanHJ/qEmby/actions/runs/{args.source_run}',
            headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'], 'Accept': 'application/vnd.github+json'})
        with urllib.request.urlopen(request, timeout=30) as response:
            run = json.load(response)
        if run.get('id') != args.source_run or not re.fullmatch('[0-9a-f]{40}', run.get('head_sha', '')):
            raise RuntimeError('Invalid compilation run identity')
        build.mkdir(parents=True, exist_ok=True)
        shutil.copytree(reuse / 'build/cloud/bin', build / 'bin')
        for binary in (build / 'bin').iterdir():
            binary.chmod(0o755)
        (build / 'compiled-provenance.json').write_text(json.dumps({
            'source_run': args.source_run, 'source_commit': run['head_sha'],
            'matching_compiled_inputs': len(compiled_inputs(current)),
        }, indent=2) + '\n')
        print('Verified unchanged application/probe sources; reusing compiled binaries')
        return
    if not (build / 'compiled-provenance.json').is_file():
        raise RuntimeError('Compilation source verification is required before repackaging')
    package, = (reuse / 'dist/cloud').glob('*.deb')
    version = re.search(r'project\(qEmby VERSION ([\d.]+)', (root / 'CMakeLists.txt').read_text())[1]
    actual = subprocess.check_output(['dpkg-deb', '-f', str(package), 'Version'], text=True).strip()
    if actual != version + '-' + os.environ['QEMBY_DEB_DIST_TAG']:
        raise RuntimeError('Compiled package distro/version mismatch')
    destination = root / 'dist/cloud'
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise RuntimeError('Use an empty package output directory')
    with tempfile.TemporaryDirectory(dir=build, prefix='repackage-') as temporary:
        tree = Path(temporary)
        subprocess.run(['dpkg-deb', '--raw-extract', str(package), str(tree)], check=True)
        sdk = Path(os.environ['QT_ROOT_DIR'])
        subprocess.run(['python3', str(root / 'scripts/release/deploy_qt_wayland.py'),
                        '--qt-lib-dir', str(sdk / 'lib'), '--qt-plugin-dir', str(sdk / 'plugins'),
                        '--private-dir', str(tree / 'usr/lib/qemby')], check=True)
        wrapper = (root / 'packaging/linux/qemby-wrapper.in').read_text().replace(
            '@QEMBY_PRIVATE_APP_RELATIVE_PATH@', 'lib/qemby/qEmbyApp')
        (tree / 'usr/bin/qEmbyApp').write_text(wrapper)
        (tree / 'usr/bin/qEmbyApp').chmod(0o755)
        for name in ('postinst', 'postrm'):
            (tree / 'DEBIAN' / name).write_text((root / f'packaging/linux/debian/{name}.in').read_text())
            (tree / 'DEBIAN' / name).chmod(0o755)
        control = tree / 'DEBIAN/control'
        text = control.read_text()
        match = re.search(r'(?m)^Depends: (.*(?:\n[ \t].*)*)', text)
        if match is None:
            raise RuntimeError('Missing package dependency declaration')
        dependencies = match[1]
        for name in ('libwayland-client0', 'libwayland-cursor0', 'libwayland-egl1'):
            if name not in dependencies:
                dependencies += ', ' + name
        control.write_text(text[:match.start(1)] + dependencies + text[match.end(1):])
        checksums = []
        for path in sorted(tree.rglob('*')):
            if path.is_file() and not path.is_symlink() and not path.is_relative_to(tree / 'DEBIAN'):
                checksums.append(f'{hashlib.md5(path.read_bytes()).hexdigest()}  {path.relative_to(tree)}\n')
        (tree / 'DEBIAN/md5sums').write_text(''.join(checksums))
        subprocess.run(['dpkg-deb', '--build', '--root-owner-group', str(tree), str(destination / package.name)], check=True)


if __name__ == '__main__':
    main()
