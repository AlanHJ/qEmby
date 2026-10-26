"""Download a test run's artifacts and verify packages and commit consistency."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_id", type=int)
    parser.add_argument("--repository", default="AlanHJ/qEmby")
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--allow-partial", action="store_true")
    args = parser.parse_args()
    destination = args.destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        raise SystemExit("Use an empty destination to avoid mixing runs")
    destination.mkdir(parents=True, exist_ok=True)
    subprocess.run(["gh", "run", "download", str(args.run_id), "--repo", args.repository,
                    "--pattern", "package-*", "--dir", str(destination)], check=True)
    commits = set()
    verified = 0
    groups = list(destination.glob("*/build-info.json"))
    for info in groups:
        metadata = json.loads(info.read_text(encoding="utf-8"))
        if str(metadata["run"]) != str(args.run_id):
            raise SystemExit("Artifact run ID mismatch")
        commits.add(metadata["commit"])
        checksums = {}
        for line in (info.parent / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"([a-f0-9]{64})  ([^/\\]+)", line)
            if not match:
                raise SystemExit("Invalid checksum entry")
            checksums[match[2]] = match[1]
        if set(checksums) != set(metadata["packages"]):
            raise SystemExit("Package manifest mismatch")
        for name, expected in checksums.items():
            package = info.parent / name
            if not package.is_file() or hashlib.sha256(package.read_bytes()).hexdigest() != expected:
                raise SystemExit(f"Package checksum mismatch: {name}")
            verified += 1
    if len(commits) != 1:
        raise SystemExit("Artifacts do not refer to one source commit")
    if not args.allow_partial and (len(groups) != 6 or verified != 7):
        raise SystemExit(f"Incomplete run: {len(groups)}/6 platform groups, {verified}/7 packages")
    print(f"Verified {verified} packages from commit {next(iter(commits))}")
    print(destination)


if __name__ == "__main__":
    main()
