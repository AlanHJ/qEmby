"""Start a real AppImage under isolated X11 and native Wayland test displays."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time


def stop(process):
    if process.poll() is None:
        # The AppImage runtime can fork: terminate its whole disposable session.
        import signal
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()


def smoke(image, platform, base):
    work = base / platform
    work.mkdir()
    runtime = work / 'runtime'
    runtime.mkdir(mode=0o700)
    env = dict(os.environ, HOME=str(work), XDG_CONFIG_HOME=str(work / 'config'),
               XDG_CACHE_HOME=str(work / 'cache'), XDG_DATA_HOME=str(work / 'data'),
               XDG_RUNTIME_DIR=str(runtime), QT_DEBUG_PLUGINS='1',
               LIBGL_ALWAYS_SOFTWARE='1', APPIMAGE_EXTRACT_AND_RUN='1')
    for key in ('QT_QPA_PLATFORM', 'QEMBY_QPA_PLATFORM', 'WAYLAND_DISPLAY', 'DISPLAY',
                'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORMTHEME'):
        env.pop(key, None)
    # Invalid host plugin roots must be overridden by AppRun.
    env['QT_PLUGIN_PATH'] = '/nonexistent/foreign-qt/plugins'
    if platform == 'xcb':
        env.update(DISPLAY=':97', XDG_SESSION_TYPE='x11')
        command = ['Xvfb', ':97', '-screen', '0', '1280x720x24', '-nolisten', 'tcp']
        ready = lambda: Path('/tmp/.X11-unix/X97').exists()
    else:
        env.update(WAYLAND_DISPLAY='qemby-test', XDG_SESSION_TYPE='wayland', WAYLAND_DEBUG='1')
        command = ['weston', '--backend=headless-backend.so', '--use-pixman',
                   '--socket=qemby-test', '--idle-time=0', '--width=1280', '--height=720']
        ready = lambda: (runtime / 'qemby-test').exists()
    with (work / 'display.log').open('w') as displaylog, (work / 'application.log').open('w') as applog:
        server = subprocess.Popen(command, env=env, stdout=displaylog, stderr=displaylog, start_new_session=True)
        app = None
        try:
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError((work / 'display.log').read_text(errors='replace'))
                if ready():
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('Test display did not become ready')
            app = subprocess.Popen([str(image)], env=env, stdout=applog, stderr=applog, start_new_session=True)
            shown = False
            for _ in range(200):
                if app.poll() is not None:
                    raise RuntimeError(f'AppImage exited early: {app.returncode}')
                logs = (work / 'application.log').read_text(errors='replace')
                if platform == 'wayland':
                    # A native surface configure proves this did not use XWayland.
                    shown = bool(re.search(r'xdg_toplevel@\d+\.configure', logs))
                else:
                    window = subprocess.run(['xdotool', 'search', '--onlyvisible', '--class', '.'],
                                            env=env, capture_output=True)
                    # Login windows can have translated titles. Check the owning
                    # executable rather than depending on a particular title.
                    for window_id in window.stdout.decode().split():
                        owner = subprocess.run(['xdotool', 'getwindowpid', window_id],
                                               env=env, text=True, capture_output=True)
                        if owner.returncode == 0 and owner.stdout.strip().isdigit():
                            executable = Path('/proc') / owner.stdout.strip() / 'exe'
                            if executable.resolve().name == 'qEmbyApp':
                                shown = True
                                break
                if shown:
                    break
                time.sleep(.2)
            if not shown:
                raise RuntimeError(f'No {platform} application window appeared')
            time.sleep(3)
            if app.poll() is not None:
                raise RuntimeError(f'AppImage crashed after mapping the {platform} window')
            print(f'PASS AppImage visible window: {platform}', flush=True)
        except Exception:
            print((work / 'application.log').read_text(errors='replace')[-18000:], flush=True)
            raise
        finally:
            if app is not None:
                stop(app)
            stop(server)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--distro', required=True)
    args = parser.parse_args()
    image = args.image.resolve()
    image.chmod(0o755)
    # Static AppImage runtime must not require a host FHS ELF interpreter.
    header = subprocess.check_output(['readelf', '-l', str(image)], text=True)
    if 'INTERP' in header:
        raise RuntimeError('AppImage runtime unexpectedly needs a dynamic ELF interpreter')
    with tempfile.TemporaryDirectory(prefix='qemby-appimage-smoke-') as temporary:
        for platform in ('xcb', 'wayland'):
            smoke(image, platform, Path(temporary))
    from release_contract import hash_file
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'distro': args.distro, 'sha256': hash_file(image),
                                      'x11': 'passed', 'native_wayland': 'passed',
                                      'static_runtime': 'passed'}, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
