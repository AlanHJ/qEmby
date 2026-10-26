"""Download a test run's artifacts and verify packages and commit consistency."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from release_contract import GROUPS, validate_artifact


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_id", type=int)
    parser.add_argument("--repository", default="AlanHJ/qEmby")
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--allow-partial", action="store_true")
    parser.add_argument("--platform", choices=['all', 'windows', 'macos', 'linux', 'appimage'], default='all')
    parser.add_argument("--artifact-pattern", default="package-*")
    args = parser.parse_args()
    run = json.loads(subprocess.check_output([
        "gh", "run", "view", str(args.run_id), "--repo", args.repository,
        "--json", "status,conclusion,headSha,databaseId"], text=True))
    if not re.fullmatch(r"[0-9a-f]{40}", run.get("headSha", "")) or run.get("databaseId") != args.run_id:
        raise SystemExit("Invalid GitHub run identity")
    if not args.allow_partial and (run["status"] != "completed" or run["conclusion"] != "success"):
        raise SystemExit("A complete download requires a successful completed workflow")
    destination = args.destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        raise SystemExit("Use an empty destination to avoid mixing runs")
    destination.mkdir(parents=True, exist_ok=True)
    pattern = args.artifact_pattern
    if pattern == 'package-*' and args.platform != 'all':
        pattern = {'windows': 'package-windows-*', 'macos': 'package-macos*',
                   'linux': 'package-linux-*', 'appimage': 'package-linux-appimage-*'}[args.platform]
    subprocess.run(["gh", "run", "download", str(args.run_id), "--repo", args.repository,
                    "--pattern", pattern, "--dir", str(destination)], check=True)
    commits = set()
    versions = set()
    verified = 0
    groups = list(destination.iterdir())
    for folder in groups:
        metadata = validate_artifact(folder, args.run_id, run["headSha"])
        commits.add(metadata["commit"])
        versions.add(metadata["version"])
        verified += len(metadata["packages"])
    if len(commits) != 1:
        raise SystemExit("Artifacts do not refer to one source commit")
    if len(versions) != 1:
        raise SystemExit("Artifacts do not refer to one release version")
    expected = {name for name, (platform, _) in GROUPS.items()
                if args.platform == 'all' or platform == args.platform
                or (args.platform == 'linux' and platform == 'appimage')}
    if not args.allow_partial and {p.name for p in groups} != expected:
        raise SystemExit(f"Incomplete run: expected {sorted(expected)}, got {sorted(p.name for p in groups)}")
    print(f"Verified {verified} packages from commit {next(iter(commits))}")
    print(destination)


if __name__ == "__main__":
    main()
