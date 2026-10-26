"""Regression tests for destructive-path guards and release artifact provenance."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from prepare_source import EXPORT_DIRS, ROOT_FILES, export_ignore, validate_export_paths, validate_worktree
from release_contract import expected_packages, hash_file, validate_artifact


class ExportSafety(unittest.TestCase):
    def test_build_scripts_are_kept_but_build_directories_and_agent_files_are_excluded(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "build-release").mkdir()
            (root / "build-deb.sh").write_text("source", encoding="utf-8")
            (root / "build-appimage.sh").write_text("source", encoding="utf-8")
            ignored = export_ignore(root, ["build-release", "build-deb.sh", "build-appimage.sh", "AGENTS.md", ".env"])
            self.assertEqual(ignored, {"build-release", "AGENTS.md", ".env"})

    def test_overlapping_or_linked_destinations_are_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            source, target, packaging = (root / name for name in ("source", "target", "packaging"))
            target.mkdir()
            for name in EXPORT_DIRS:
                (source / name).mkdir(parents=True, exist_ok=True)
            for name in ROOT_FILES:
                (source / name).write_text("source", encoding="utf-8")
            (packaging / "installer").mkdir(parents=True)
            for name in ("qEmby.iss", "ChineseSimplified.isl"):
                (packaging / "installer" / name).write_text("fixture", encoding="utf-8")
            validate_export_paths(source, target, packaging)
            with self.assertRaises(ValueError):
                validate_export_paths(source, source, packaging)
            with self.assertRaises(ValueError):
                validate_export_paths(source, root, packaging)
            with self.assertRaises(ValueError):
                validate_export_paths(source, target, target)
            sentinel = root / "personal-config"
            sentinel.write_text("preserve", encoding="utf-8")
            try:
                (target / ".gitignore").symlink_to(sentinel)
            except OSError:
                self.skipTest("Symbolic links require additional Windows privileges; checked in Linux CI")
            with self.assertRaises(ValueError):
                validate_export_paths(source, target, packaging)
            self.assertEqual(sentinel.read_text(), "preserve")

    def test_real_worktree_rejects_untracked_and_staged_user_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            repo, worktree = root / "repo", root / "worktree"
            env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1",
                       GIT_AUTHOR_NAME="qEmby tests", GIT_AUTHOR_EMAIL="qemby-ci@example.invalid",
                       GIT_COMMITTER_NAME="qEmby tests", GIT_COMMITTER_EMAIL="qemby-ci@example.invalid")

            def git(*args):
                subprocess.run(["git", *map(str, args)], check=True, env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

            git("init", repo)
            git("-C", repo, "commit", "--allow-empty", "-m", "fixture")
            git("-C", repo, "worktree", "add", "-b", "codex/cloud-build-guard-test", worktree)
            validate_worktree(worktree)
            user_file = worktree / "personal.txt"
            user_file.write_text("must survive", encoding="utf-8")
            with self.assertRaises(ValueError):
                validate_worktree(worktree)
            git("-C", worktree, "add", "personal.txt")
            with self.assertRaises(ValueError):
                validate_worktree(worktree)
            self.assertEqual(user_file.read_text(), "must survive")


class ArtifactSafety(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name) / "package-linux-jammy-amd64"
        self.folder.mkdir()
        self.name = next(iter(expected_packages(self.folder.name, "0.0.9")))
        (self.folder / self.name).write_bytes(b"test package")
        self.info = {"schema_version": 1, "group": self.folder.name, "version": "0.0.9",
                     "run": "42", "commit": "a" * 40, "platform": "linux", "packages": [self.name],
                     "validation": {"deb_lifecycle": "passed", "qt_preserved": True, "global_probes_preserved": True}}
        self.save()
        self.sum_line = f"{hash_file(self.folder / self.name)}  {self.name}\n"
        (self.folder / "SHA256SUMS.txt").write_text(self.sum_line, encoding="utf-8")

    def save(self):
        (self.folder / "build-info.json").write_text(json.dumps(self.info), encoding="utf-8")

    def test_valid_artifact(self):
        self.assertEqual(validate_artifact(self.folder, 42, "a" * 40)["version"], "0.0.9")

    def test_mixed_run_and_commit_are_rejected(self):
        for run, commit in ((43, "a" * 40), (42, "b" * 40)):
            with self.assertRaises(ValueError):
                validate_artifact(self.folder, run, commit)

    def test_duplicate_hashes_and_unlisted_files_are_rejected(self):
        (self.folder / "SHA256SUMS.txt").write_text(self.sum_line * 2, encoding="utf-8")
        with self.assertRaises(ValueError):
            validate_artifact(self.folder, 42, "a" * 40)
        (self.folder / "SHA256SUMS.txt").write_text(self.sum_line, encoding="utf-8")
        (self.folder / "stale.deb").write_bytes(b"old")
        with self.assertRaises(ValueError):
            validate_artifact(self.folder, 42, "a" * 40)

    def test_traversal_missing_validation_and_corruption_are_rejected(self):
        self.info["packages"] = ["../other.deb"]
        self.save()
        with self.assertRaises(ValueError):
            validate_artifact(self.folder, 42, "a" * 40)
        self.info["packages"] = [self.name]
        self.info["validation"] = {}
        self.save()
        with self.assertRaises(ValueError):
            validate_artifact(self.folder, 42, "a" * 40)
        self.info["validation"] = {"deb_lifecycle": "passed", "qt_preserved": True, "global_probes_preserved": True}
        self.save()
        (self.folder / self.name).write_bytes(b"corrupt")
        with self.assertRaises(ValueError):
            validate_artifact(self.folder, 42, "a" * 40)


if __name__ == "__main__":
    unittest.main()
