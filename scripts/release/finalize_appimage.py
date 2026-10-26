"""Publish metadata only after every distro tested the exact same AppImage."""
import json
import os
from pathlib import Path
import re
import shutil
from release_contract import hash_file


root = Path(__file__).resolve().parents[2]
image, = (root / 'build/appimage').glob('*.AppImage')
digest = hash_file(image)
reports = [json.loads(p.read_text()) for p in (root / 'build/appimage-reports').rglob('*.json')]
expected = {'jammy', 'noble', 'bookworm', 'trixie'}
if len(reports) != len(expected) or {r['distro'] for r in reports} != expected:
    raise RuntimeError('Missing distribution smoke reports')
for report in reports:
    if report['sha256'] != digest or any(report.get(key) != 'passed' for key in
                                       ('x11', 'native_wayland', 'static_runtime')):
        raise RuntimeError('AppImage smoke tests failed or used a different file')
dist = root / 'dist/cloud'
if dist.exists() and any(dist.iterdir()):
    raise RuntimeError('Use an empty dist/cloud directory')
dist.mkdir(parents=True, exist_ok=True)
shutil.copy2(image, dist / image.name)
version = re.search(r'project\(qEmby VERSION ([\d.]+)', (root / 'CMakeLists.txt').read_text())[1]
info = {'schema_version': 1, 'group': 'package-linux-appimage-x86_64', 'version': version,
        'commit': os.environ['GITHUB_SHA'], 'run': os.environ['GITHUB_RUN_ID'],
        'platform': 'appimage', 'packages': [image.name],
        'validation': {'dependency_closure': 'passed', 'glibc_maximum': '2.35',
                       'distro_smoke': reports}}
(dist / 'SHA256SUMS.txt').write_text(f'{digest}  {image.name}\n')
(dist / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
