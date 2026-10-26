"""Prepare, validate and commit a cloud-test snapshot; optionally push to run CI."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--packaging-source", type=Path, required=True)
    parser.add_argument("--push", action="store_true")
    parser.add_argument("--message", default="ci: update verified cloud package test")
    args = parser.parse_args()
    scripts = Path(__file__).resolve().parent
    destination = args.destination.resolve()

    def checked(*command):
        subprocess.run(list(map(str, command)), check=True)

    checked(sys.executable, scripts / "check_comment_removal.py")
    checked(sys.executable, scripts / "check_package_helpers.py")
    checked(sys.executable, scripts / "prepare_source.py", "--destination", destination,
            "--packaging-source", args.packaging_source, "--strip-comments")
    checked(sys.executable, destination / "scripts/release/verify_source.py")
    checked("git", "-C", destination, "add", "-u", "--", "src", "cmake", "packaging",
            "libs/qwindowkit", "scripts/release", ".github", "installer")
    checked("git", "-C", destination, "add", "-f", "--pathspec-from-file=.backup/stage-paths.txt")
    changed = subprocess.run(["git", "-C", str(destination), "diff", "--cached", "--quiet"])
    if changed.returncode == 1:
        checked("git", "-C", destination, "commit", "--quiet", "-m", args.message)
    elif changed.returncode != 0:
        changed.check_returncode()
    if args.push:
        checked("git", "-C", destination, "push", "-u", "origin", "HEAD")


if __name__ == "__main__":
    main()
