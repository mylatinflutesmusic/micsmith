#!/usr/bin/env python3
"""Open a REAPER project with a selected Quell preset, save it, reopen it,
and verify both the sound parameters and the persisted preset name."""

import base64
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

QUEL_ROOT = Path(__file__).resolve().parents[2]
FIXTURE = QUEL_ROOT / "tests" / "daw" / "preset_reopen.RPP"
SCRIPT = QUEL_ROOT / "tests" / "daw" / "preset_reopen.lua"
OUTPUT_DIR = QUEL_ROOT / "build" / "daw-tests"
SAVED = OUTPUT_DIR / "preset_reopen_roundtrip.RPP"
LOG = OUTPUT_DIR / "preset_reopen.log"

EXPECTED = {
    "depth": 55.0,
    "sensitivity": 60.0,
    "selectivity": 60.0,
    "speed": 55.0,
    "lowtame": 20.0,
    "hightame": 15.0,
    "mix": 100.0,
    "output": 0.0,
    "link": 1.0,
}


def find_reaper():
    override = os.environ.get("REAPER_EXE")
    candidates = [override] if override else []
    candidates += [
        r"C:\Program Files\REAPER (x64)\reaper.exe",
        r"C:\Program Files\REAPER\reaper.exe",
        shutil.which("reaper"),
    ]

    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)

    raise FileNotFoundError(
        "REAPER not found. Set REAPER_EXE to the reaper.exe path."
    )


def extract_vst_xml(path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    start = next(i for i, line in enumerate(lines) if "<VST " in line)
    state_lines = []
    i = start + 1

    while i < len(lines) and lines[i].strip() != ">":
        if lines[i].strip():
            state_lines.append(lines[i].strip())
        i += 1

    raw = b"".join(base64.b64decode(line) for line in state_lines)
    xml_start = raw.find(b"<?xml")
    xml_end = raw.find(b"</PARAMS>") + len(b"</PARAMS>")

    if xml_start < 0 or xml_end < len(b"</PARAMS>"):
        raise ValueError("Quell VST state contains no PARAMS XML")

    return raw[xml_start:xml_end].decode("utf-8")


def check_persisted_state():
    if not SAVED.is_file():
        raise FileNotFoundError(f"REAPER did not save {SAVED}")

    xml = extract_vst_xml(SAVED)
    errors = []

    if 'presetName="Vocal Cleanup"' not in xml:
        errors.append("presetName was not preserved in the saved project")

    for key, expected in EXPECTED.items():
        match = re.search(rf'<PARAM id="{key}" value="([^"]*)"/>', xml)
        if match is None:
            errors.append(f"{key} is missing from the saved state")
        elif abs(float(match.group(1)) - expected) > 0.01:
            errors.append(f"{key}={match.group(1)}, expected {expected}")

    return errors


def main():
    reaper = find_reaper()

    if not FIXTURE.is_file():
        print(f"Missing fixture: {FIXTURE}", file=sys.stderr)
        return 1

    if not SCRIPT.is_file():
        print(f"Missing REAPER script: {SCRIPT}", file=sys.stderr)
        return 1

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    for path in (SAVED, LOG):
        if path.exists():
            path.unlink()

    env = os.environ.copy()
    env.update(
        {
            "QUELL_DAW_SOURCE": str(FIXTURE),
            "QUELL_DAW_SAVED": str(SAVED),
            "QUELL_DAW_LOG": str(LOG),
        }
    )

    process = subprocess.Popen(
        [str(reaper), "-newinst", "-new", "-nosplash", str(SCRIPT)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    deadline = time.time() + 30
    log_text = ""

    try:
        while time.time() < deadline:
            if LOG.is_file():
                log_text = LOG.read_text(encoding="utf-8", errors="replace")
                if "RESULT" in log_text:
                    break

            if process.poll() is not None and not LOG.is_file():
                break

            time.sleep(0.25)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()

    print(log_text.rstrip())

    if "RESULT PASS" not in log_text:
        print("\nDAW PRESET REOPEN: FAIL", file=sys.stderr)
        return 1

    try:
        errors = check_persisted_state()
    except Exception as error:
        print(f"\nDAW PRESET REOPEN: FAIL: {error}", file=sys.stderr)
        return 1

    if errors:
        print("\nDAW PRESET REOPEN: FAIL", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(f"\nSaved project: {SAVED}")
    print("DAW PRESET REOPEN: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
