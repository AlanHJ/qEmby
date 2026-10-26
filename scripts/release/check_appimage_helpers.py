"""Regression checks for AppImage host exclusions and artifact validation."""
import json
from pathlib import Path
import tempfile
import unittest
from bundle_appimage import HOST_LIBRARY
from release_contract import expected_packages, hash_file, validate_artifact


class AppImagePolicy(unittest.TestCase):
    def test_only_host_glibc_and_vendor_drivers_are_excluded(self):
        for name in ('libc.so.6', 'libm.so.6', 'ld-linux-x86-64.so.2', 'libGLX_mesa.so.0'):
            self.assertIsNotNone(HOST_LIBRARY.fullmatch(name), name)
        for name in ('libstdc++.so.6', 'libgcc_s.so.1', 'libwayland-client.so.0',
                     'libxkbcommon.so.0', 'libxcb.so.1', 'libpulse.so.0',
                     'libEGL.so.1', 'libGLdispatch.so.0', 'libQt6Core.so.6', 'libmpv.so.1'):
            self.assertIsNone(HOST_LIBRARY.fullmatch(name), name)

    def test_reports_must_match_package_hash_and_native_wayland(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary) / 'package-linux-appimage-x86_64'
            folder.mkdir()
            name, = expected_packages(folder.name, '0.0.9')
            (folder / name).write_bytes(b'AppImage fixture')
            digest = hash_file(folder / name)
            reports = [{'distro': d, 'sha256': digest, 'x11': 'passed',
                        'native_wayland': 'passed', 'static_runtime': 'passed'}
                       for d in ('jammy', 'noble', 'bookworm', 'trixie')]
            info = {'schema_version': 1, 'group': folder.name, 'version': '0.0.9',
                    'run': '42', 'commit': 'a' * 40, 'platform': 'appimage', 'packages': [name],
                    'validation': {'dependency_closure': 'passed', 'glibc_maximum': '2.35',
                                   'distro_smoke': reports}}
            (folder / 'SHA256SUMS.txt').write_text(f'{digest}  {name}\n')
            def save():
                (folder / 'build-info.json').write_text(json.dumps(info))
            save()
            validate_artifact(folder, 42, 'a' * 40)
            reports[0]['native_wayland'] = 'failed'
            save()
            with self.assertRaises(ValueError):
                validate_artifact(folder, 42, 'a' * 40)
            reports[0]['native_wayland'] = 'passed'
            reports[0]['sha256'] = 'b' * 64
            save()
            with self.assertRaises(ValueError):
                validate_artifact(folder, 42, 'a' * 40)


if __name__ == '__main__':
    unittest.main()
