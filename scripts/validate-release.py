#!/usr/bin/env python3
"""Validate the preserved App Store PBW against source metadata and checksums."""

import hashlib
import json
from pathlib import Path
import sys
import zipfile


ROOT = Path(__file__).resolve().parent.parent
PACKAGE_PATH = ROOT / "package.json"
CHECKSUM_PATH = ROOT / "submission/SHA256SUMS"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


package = json.loads(PACKAGE_PATH.read_text())
expected = package["pebble"]
PBW_PATH = ROOT / f"submission/artifacts/wakka-wakka-{package['version']}.pbw"

if not PBW_PATH.is_file():
    fail(f"missing submission artifact: {PBW_PATH.relative_to(ROOT)}")

digest = hashlib.sha256(PBW_PATH.read_bytes()).hexdigest()
checksum_lines = CHECKSUM_PATH.read_text().splitlines()
artifact_entry = next(
    (line for line in checksum_lines if line.endswith(f"  {PBW_PATH.relative_to(ROOT)}")),
    None,
)
if artifact_entry is None:
    fail("submission artifact is missing from submission/SHA256SUMS")
if artifact_entry.split()[0] != digest:
    fail("submission artifact checksum does not match submission/SHA256SUMS")

with zipfile.ZipFile(PBW_PATH) as archive:
    names = set(archive.namelist())
    appinfo = json.loads(archive.read("appinfo.json"))

expected_targets = expected["targetPlatforms"]
if appinfo["targetPlatforms"] != expected_targets:
    fail("PBW targetPlatforms do not match package.json")
if appinfo["uuid"] != expected["uuid"]:
    fail("PBW UUID does not match package.json")
if appinfo["versionLabel"] != package["version"]:
    fail("PBW version does not match package.json")

for platform in expected_targets:
    for member in (f"{platform}/manifest.json", f"{platform}/pebble-app.bin"):
        if member not in names:
            fail(f"PBW is missing {member}")

print(
    "PASS: release PBW checksum and metadata match package.json "
    f"({', '.join(expected_targets)}, {digest})"
)
