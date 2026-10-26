"""Verify the exact exported source snapshot before building."""
import hashlib
import json
from pathlib import Path


root = Path(__file__).resolve().parents[2]
manifest = json.loads((root / "release-source.json").read_text(encoding="utf-8"))
invalid = []
for name, expected in manifest["files"].items():
    path = (root / name).resolve()
    if not path.is_relative_to(root) or not path.is_file():
        invalid.append(name)
    elif hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        invalid.append(name)
if invalid:
    raise SystemExit("Source snapshot mismatch: " + ", ".join(invalid))
print(f"Verified {len(manifest['files'])} source files")
