"""Export release inputs into a dedicated cloud-test worktree (never the dev tree)."""
import argparse
import contextlib
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys


def release_remover():
    from comms.core import CommentRemover

    class ReleaseCommentRemover(CommentRemover):
        def remove_c_style_comments(self, content):
            # Shield literals and complete legal notices from the upstream
            # character scanner (raw strings, digit separators, URLs in notices).
            pattern = re.compile(
                r'(?P<raw>(?:u8|u|U|L)?R"(?P<tag>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=tag)")'
                r"|(?P<number>\b[0-9][A-Za-z0-9._']*'[A-Za-z0-9._']+)"
                r'''|(?P<string>(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u8|u|U|L)?'(?:\\.|[^'\\])*')'''
                r'|(?P<comment>/\*.*?\*/|//[^\n]*)', re.DOTALL)
            saved = []
            prefix = "__QEMBY_RELEASE_LITERAL__"
            while prefix in content:
                prefix += "_"

            def shield(match):
                value = match.group()
                if match.lastgroup == "comment":
                    if not re.search(r"copyright|SPDX-License-Identifier|licensed under|permission is hereby", value, re.I):
                        return value + (" " if value.startswith("/*") else "")
                token = f'"{prefix}{len(saved)}"'
                saved.append((token, value))
                return token

            cleaned = super().remove_c_style_comments(pattern.sub(shield, content))
            for token, value in saved:
                cleaned = cleaned.replace(token, value)
            return cleaned

    remover = ReleaseCommentRemover()
    for suffix in (".ts", ".m"):
        remover.supported_extensions.pop(suffix, None)
    return remover


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--packaging-source", type=Path, required=True)
    parser.add_argument("--strip-comments", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    destination = args.destination.resolve()
    if destination == source or source in destination.parents:
        raise SystemExit("Destination must be outside the development tree")
    if not (destination / ".git").is_file():
        raise SystemExit("Destination must be a dedicated Git worktree")
    branch = subprocess.check_output(
        ["git", "-C", str(destination), "branch", "--show-current"], text=True).strip()
    if not branch.startswith("codex/cloud-build"):
        raise SystemExit("Only a codex/cloud-build* worktree may be updated")
    ignore = shutil.ignore_patterns(".git", ".github", ".backup", "__pycache__",
                                    "build", ".DS_Store", "*.user")
    for relative in ("src", "cmake", "packaging", "libs/qwindowkit", "scripts/release"):
        target = (destination / relative).resolve()
        if not target.is_relative_to(destination) or target == destination:
            raise SystemExit("Unsafe export target")
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source / relative, target, ignore=ignore)
    for name in ("CMakeLists.txt", "LICENSE", "README.md", ".gitattributes"):
        origin = source / name
        if not origin.exists():
            origin = args.packaging_source / name
        shutil.copy2(origin, destination / name)
    shutil.copytree(source / ".github", destination / ".github", dirs_exist_ok=True)
    installer = (destination / "installer").resolve()
    if not installer.is_relative_to(destination):
        raise SystemExit("Unsafe installer directory")
    if installer.exists():
        shutil.rmtree(installer)
    installer.mkdir()
    for path in (args.packaging_source / "installer").iterdir():
        if path.is_file() and path.suffix in (".iss", ".isl"):
            shutil.copy2(path, installer / path.name)
    # The local QWindowKit checkout has broken submodule metadata. Export its
    # actual source, including qmsetup, rather than fetching a different revision.
    (destination / ".gitmodules").write_text("", encoding="utf-8")
    if args.strip_comments:
        previous = Path.cwd()
        os.chdir(destination)
        try:
            (destination / ".backup").mkdir(exist_ok=True)
            remover = release_remover()
            # Qt .ts files are XML, and .m files here are Objective-C, not MATLAB.
            for suffix in (".ts", ".m"):
                remover.supported_extensions.pop(suffix, None)
            for directory in ("libs", "src"):
                with (destination / ".backup" / "comment-removal.log").open("a", encoding="utf-8") as log:
                    with contextlib.redirect_stdout(log):
                        result = remover.run(directory)
                print(directory, json.dumps({key: result.get(key) for key in
                      ("processed", "modified", "errors", "message")}, ensure_ascii=True))
                if result.get("errors", 0):
                    raise RuntimeError("Comment removal failed")
        finally:
            os.chdir(previous)
    # Backups intentionally remain local and are excluded from Git.
    with (destination / ".gitignore").open("a", encoding="utf-8") as stream:
        stream.write("\n.backup/\n__pycache__/\ndist/\nrelease/\n")
    manifest = {}
    for relative in ("src", "cmake", "packaging", "libs/qwindowkit", "scripts/release", ".github", "installer"):
        for path in sorted((destination / relative).rglob("*")):
            if path.is_file():
                data = path.read_bytes()
                try:
                    if b"\0" not in data:
                        data.decode("utf-8")
                        path.write_bytes(data.replace(b"\r\n", b"\n"))
                except UnicodeDecodeError:
                    pass
                manifest[path.relative_to(destination).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    for name in ("CMakeLists.txt", "LICENSE", "README.md", ".gitattributes"):
        path = destination / name
        path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n"))
        manifest[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    for required in ("packaging/linux/build-deb.sh", "scripts/release/package.py",
                     "libs/qwindowkit/qmsetup/cmake/scripts/configure_file.cmake"):
        if required not in manifest:
            raise RuntimeError(f"Missing required release input: {required}")
    (destination / "release-source.json").write_text(
        json.dumps({"comments_stripped": args.strip_comments, "files": manifest}, indent=2) + "\n", encoding="utf-8")
    (destination / ".backup").mkdir(exist_ok=True)
    (destination / ".backup/stage-paths.txt").write_text(
        "\n".join([*manifest, "release-source.json", ".gitignore", ".gitmodules"]) + "\n", encoding="utf-8")
    print(f"Exported {len(manifest)} files to {destination}")


if __name__ == "__main__":
    main()
