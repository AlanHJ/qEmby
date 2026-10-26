"""Check Linux packaging directory guards without compiling anything."""
from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
common = root / "packaging/linux/common.sh"
for path in (root / "packaging/linux").rglob("*.sh"):
    subprocess.run(["bash", "-n", str(path)], check=True)
for path in (root / "packaging/linux/debian").glob("*.in"):
    subprocess.run(["sh", "-n", str(path)], check=True)
for name in ("AppRun", "qemby-wrapper.in"):
    subprocess.run(["sh", "-n", str(root / "packaging/linux" / name)], check=True)
with tempfile.TemporaryDirectory(prefix="qemby-path-tests-") as temporary:
    base = Path(temporary)
    source = base / "source"
    source.mkdir()
    build = source / "build"
    build.mkdir()
    sentinel = build / "personal.txt"
    sentinel.write_text("keep")
    (build / "CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/another/source\n")

    def call(function, target):
        return subprocess.run(["bash", "-c", 'set -euo pipefail; source "$1"; "$2" "$3" "$4"',
                               "guard-test", str(common), function, str(source), str(target)],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    assert call("qemby_check_build_cache", build).returncode != 0
    assert sentinel.read_text() == "keep"
    for unsafe in (source, source.parent, Path("/")):
        assert call("qemby_validate_work_dir", unsafe).returncode != 0
    unowned = base / "unowned-appdir"
    unowned.mkdir()
    (unowned / "personal.txt").write_text("keep")
    assert call("qemby_reset_owned_appdir", unowned).returncode != 0
    assert (unowned / "personal.txt").read_text() == "keep"
    owned = base / "owned-appdir"
    assert call("qemby_reset_owned_appdir", owned).returncode == 0
    (owned / "generated.txt").write_text("generated")
    assert call("qemby_reset_owned_appdir", owned).returncode == 0
    assert not (owned / "generated.txt").exists()
print("Linux packaging path guards passed")
