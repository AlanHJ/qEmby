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


EXPORT_DIRS = ("src", "cmake", "packaging", "libs/qwindowkit", "scripts/release", ".github")
ROOT_FILES = ("CMakeLists.txt", "LICENSE", "README.md", ".gitattributes")


def is_link(path):
    return path.is_symlink() or getattr(path, "is_junction", lambda: False)()


def export_ignore(directory, names):
    ignored = set(shutil.ignore_patterns(
        ".git", ".github", ".backup", "__pycache__", ".cache", ".vs", ".vscode",
        "build", ".DS_Store", "*.user", "*.log", "AGENTS.md", "CLAUDE.md", ".env", ".env.*"
    )(directory, names))
    for name in names:
        if name in ignored:
            continue
        path = Path(directory) / name
        if is_link(path):
            raise ValueError(f"Linked release input is not allowed: {path}")
        if path.is_dir() and name.startswith("build-"):
            ignored.add(name)
    return ignored


def validate_worktree(destination):
    if is_link(destination) or not (destination / ".git").is_file():
        raise ValueError("Destination must be a dedicated, unlinked Git worktree")
    root = Path(subprocess.check_output(
        ["git", "-C", str(destination), "rev-parse", "--show-toplevel"], text=True).strip()).resolve()
    branch = subprocess.check_output(
        ["git", "-C", str(destination), "branch", "--show-current"], text=True).strip()
    if root != destination.resolve() or not branch.startswith("codex/cloud-build"):
        raise ValueError("Only the root of a codex/cloud-build* worktree may be updated")
    dirty = subprocess.check_output(
        ["git", "-C", str(destination), "status", "--porcelain", "--untracked-files=all"], text=True)
    if dirty.strip():
        raise ValueError("Test worktree has local or staged changes; preserve/review them before exporting")


def validate_export_paths(source, destination, packaging_source):
    roots = [path.resolve() for path in (source, destination, packaging_source)]
    for origin in (roots[0], roots[2]):
        if origin == roots[1] or origin in roots[1].parents or roots[1] in origin.parents:
            raise ValueError("Source, packaging inputs and destination must not overlap")
    for relative in (*EXPORT_DIRS, "installer", ".backup", *ROOT_FILES,
                     ".gitignore", ".gitmodules", "release-source.json"):
        path = destination / relative
        if is_link(path) or path.resolve() != path.absolute():
            raise ValueError(f"Linked export destination is not allowed: {relative}")
    for relative in EXPORT_DIRS:
        path = source / relative
        if not path.is_dir() or is_link(path):
            raise ValueError(f"Missing or linked source directory: {relative}")
        target = destination / relative
        if is_link(target) or not target.resolve().is_relative_to(destination.resolve()):
            raise ValueError(f"Unsafe export target: {relative}")
    for name in ROOT_FILES:
        path = source / name if (source / name).exists() else packaging_source / name
        if not path.is_file() or is_link(path):
            raise ValueError(f"Missing or linked input: {name}")
    for name in ("qEmby.iss", "ChineseSimplified.isl"):
        path = packaging_source / "installer" / name
        if not path.is_file() or is_link(path):
            raise ValueError(f"Missing or linked installer input: {name}")
    if is_link(destination / "installer") or is_link(packaging_source / "installer"):
        raise ValueError("Linked installer directory is not allowed")


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
    if is_link(args.destination):
        raise SystemExit("Linked destination is not allowed")
    destination = args.destination.resolve()
    validate_worktree(destination)
    validate_export_paths(source, destination, args.packaging_source)
    # Validate all source entries before deleting any previous export directory.
    for relative in EXPORT_DIRS:
        for directory, names, files in os.walk(source / relative):
            excluded = export_ignore(directory, names + files)
            names[:] = [name for name in names if name not in excluded]
    for relative in EXPORT_DIRS:
        target = (destination / relative).resolve()
        if not target.is_relative_to(destination) or target == destination:
            raise SystemExit("Unsafe export target")
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source / relative, target, ignore=export_ignore)
    for name in ROOT_FILES:
        origin = source / name
        if not origin.exists():
            origin = args.packaging_source / name
        shutil.copy2(origin, destination / name)
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
    # Existing publication .gitignore comments may use a legacy Windows encoding.
    # Preserve its bytes; only the ASCII exclusion entries need modification.
    ignores = (destination / ".gitignore").read_bytes().splitlines()
    local_only = [b".backup/", b"__pycache__/", b"dist/", b"release/"]
    original = b"\n".join(line for line in ignores if line not in local_only).rstrip()
    (destination / ".gitignore").write_bytes(original + b"\n\n" + b"\n".join(local_only) + b"\n")
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
    for name in (*ROOT_FILES, ".gitignore", ".gitmodules"):
        path = destination / name
        path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n"))
        manifest[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    for required in ("packaging/linux/build-deb.sh", "scripts/release/package.py",
                     "libs/qwindowkit/qmsetup/cmake/scripts/configure_file.cmake"):
        if required not in manifest:
            raise RuntimeError(f"Missing required release input: {required}")
    (destination / "release-source.json").write_text(
        json.dumps({"comments_stripped": args.strip_comments, "files": manifest}, indent=2) + "\n", encoding="utf-8", newline="\n")
    (destination / ".backup").mkdir(exist_ok=True)
    (destination / ".backup/stage-paths.txt").write_text(
        "\n".join([*manifest, "release-source.json"]) + "\n", encoding="utf-8")
    print(f"Exported {len(manifest)} files to {destination}")


if __name__ == "__main__":
    main()
