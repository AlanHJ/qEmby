"""Exercise real PlayerView rendering against the Qt runtime inside the DEB."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    root = Path(__file__).resolve().parents[2]
    deb, = (root / 'dist/cloud').glob('*.deb')
    reports = []
    with tempfile.TemporaryDirectory(prefix='qemby-playback-') as temporary:
        work = Path(temporary)
        payload = work / 'payload'
        subprocess.run(['dpkg-deb', '-x', str(deb), str(payload)], check=True)
        private = payload / 'usr/lib/qemby'
        video = work / 'sample.mp4'
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-f', 'lavfi', '-i',
                        'testsrc2=size=640x360:rate=30', '-t', '20', '-c:v', 'mpeg4',
                        '-q:v', '4', str(video)], check=True)
        env = dict(os.environ, LD_LIBRARY_PATH=str(private), QT_PLUGIN_PATH=str(private / 'plugins'),
                   QT_QPA_PLATFORM_PLUGIN_PATH=str(private / 'plugins/platforms'),
                   LIBGL_ALWAYS_SOFTWARE='1')
        # Verify that the probe links to the package's private Qt, not the SDK
        # or the distribution Qt still installed as build prerequisites.
        listing = subprocess.check_output(['ldd', str(build / 'bin/qEmbyPlaybackProbe')], env=env, text=True)
        for line in listing.splitlines():
            if 'libQt6' in line and str(private) not in line:
                raise RuntimeError('Playback test escaped the deployed Qt runtime: ' + line)
        for platform in ('xcb', 'wayland'):
            for mode in ('embedded', 'independent'):
                home = work / f'{platform}-{mode}'
                home.mkdir()
                runtime = home / 'runtime'
                runtime.mkdir(mode=0o700)
                current = dict(env, HOME=str(home), XDG_RUNTIME_DIR=str(runtime),
                               XDG_CONFIG_HOME=str(home / 'config'), XDG_CACHE_HOME=str(home / 'cache'),
                               XDG_DATA_HOME=str(home / 'data'), QT_QPA_PLATFORM=platform)
                for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'QT_QPA_PLATFORMTHEME', 'QEMBY_QPA_PLATFORM'):
                    current.pop(key, None)
                command = [str(build / 'bin/qEmbyPlaybackProbe'), str(video), mode]
                server = None
                with (home / 'compositor.log').open('w') as log:
                    try:
                        if platform == 'xcb':
                            current['XDG_SESSION_TYPE'] = 'x11'
                            command = ['xvfb-run', '-a', '-s', '-screen 0 1280x720x24', *command]
                        else:
                            current.update(WAYLAND_DISPLAY='playback-test', XDG_SESSION_TYPE='wayland')
                            server = subprocess.Popen(['weston', '--backend=headless-backend.so', '--use-pixman',
                                                       '--socket=playback-test', '--idle-time=0'],
                                                      env=current, stdout=log, stderr=log)
                            for _ in range(100):
                                if (runtime / 'playback-test').exists():
                                    break
                                if server.poll() is not None:
                                    raise RuntimeError((home / 'compositor.log').read_text())
                                time.sleep(.1)
                        result = subprocess.run(command, env=current, text=True, stdout=subprocess.PIPE,
                                                stderr=subprocess.STDOUT, timeout=65)
                        print(result.stdout[-16000:], flush=True)
                        if result.returncode or '[PlaybackProbe] PASS' not in result.stdout or 'Qt: 6.9.3' not in result.stdout:
                            raise RuntimeError(f'Real video playback failed: {platform}/{mode} (exit {result.returncode})')
                        reports.append({'platform': platform, 'mode': mode, 'result': 'passed'})
                    finally:
                        if server is not None:
                            server.terminate()
                            server.wait(timeout=10)
    report_path = build / 'deb-safety-report.json'
    validation = json.loads(report_path.read_text())
    if validation.get('qt_runtime') != 'private':
        raise RuntimeError('Expected private Qt deployment')
    validation.update(qt_version='6.9.3', video_playback=reports)
    report_path.write_text(json.dumps(validation, indent=2) + '\n')


if __name__ == '__main__':
    main()
