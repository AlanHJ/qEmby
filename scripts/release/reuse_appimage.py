"""Replay checks only when the application and all AppImage packaging inputs match."""
import argparse
import json
import os
from pathlib import Path
import re
import urllib.request


def fetch(url):
    request = urllib.request.Request(url, headers={
        'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
        'Accept': 'application/vnd.github+json',
    })
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def payload_inputs(manifest):
    return {name: digest for name, digest in manifest['files'].items()
            if name.startswith(('src/', 'libs/', 'cmake/', 'packaging/')) or name in {
                'CMakeLists.txt', 'LICENSE', 'scripts/release/appimage-build.sh',
                'scripts/release/bundle_appimage.py', 'scripts/release/playback-probe.cpp'}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-run', type=int, required=True)
    args = parser.parse_args()
    run = fetch(f'https://api.github.com/repos/AlanHJ/qEmby/actions/runs/{args.source_run}')
    commit = run.get('head_sha', '')
    if run.get('id') != args.source_run or not re.fullmatch('[0-9a-f]{40}', commit):
        raise RuntimeError('Invalid AppImage source run')
    root = Path(__file__).resolve().parents[2]
    current = json.loads((root / 'release-source.json').read_text(encoding='utf-8'))
    blob = fetch(f'https://api.github.com/repos/AlanHJ/qEmby/contents/release-source.json?ref={commit}')
    import base64
    previous = json.loads(base64.b64decode(blob['content']))
    if payload_inputs(current) != payload_inputs(previous):
        raise RuntimeError('AppImage application or packaging changed; compile a new image')
    print(f'Verified unchanged AppImage/probe inputs from {args.source_run}, {commit}')


if __name__ == '__main__':
    main()
