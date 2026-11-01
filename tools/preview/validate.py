#!/usr/bin/env python3
"""Render and drive the shared native ImGui UI with sample data, without a game."""

import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time
import zlib


def valid_png(path):
    """Check complete PNG chunks/CRCs, including IEND, before publishing evidence."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    offset = 8
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        end = offset + 12 + size
        if end > len(data):
            return False
        kind = data[offset + 4:offset + 8]
        expected = struct.unpack_from(">I", data, offset + 8 + size)[0]
        if zlib.crc32(data[offset + 4:offset + 8 + size]) & 0xffffffff != expected:
            return False
        if kind == b"IEND":
            return size == 0 and end == len(data)
        offset = end
    return False


@contextlib.contextmanager
def display_environment():
    env = os.environ.copy()
    with tempfile.TemporaryDirectory(prefix="s8-ui-preview-") as temporary:
        os.chmod(temporary, 0o700)
        env["XDG_RUNTIME_DIR"] = temporary
        env["MESA_SHADER_CACHE_DISABLE"] = "true"
        process = None
        if os.name != "nt" and not env.get("DISPLAY"):
            if not shutil.which("Xvfb"):
                raise RuntimeError("Xvfb is required when DISPLAY is unset")
            display = next(number for number in range(120, 220)
                           if not Path(f"/tmp/.X11-unix/X{number}").exists()
                           and not Path(f"/tmp/.X{number}-lock").exists())
            # Local disposable display contains only sample UI; TCP is disabled.
            log = open(Path(temporary) / "xvfb.log", "w+")
            process = subprocess.Popen(["Xvfb", f":{display}", "-screen", "0", "1920x1200x24",
                                        "-nolisten", "tcp", "-ac"], stdout=log, stderr=log)
            for _ in range(100):
                if process.poll() is not None:
                    log.seek(0)
                    raise RuntimeError("Xvfb failed: " + log.read())
                if Path(f"/tmp/.X11-unix/X{display}").exists():
                    break
                time.sleep(0.05)
            else:
                raise RuntimeError("Xvfb startup timed out")
            env["DISPLAY"] = f":{display}"
            env["SDL_VIDEODRIVER"] = "x11"
        try:
            yield env, Path(temporary)
        finally:
            if process is not None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path, default=Path("out/ui-preview"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    executable = args.executable.resolve()
    output = args.output.resolve()
    screenshots = output / "screenshots"
    screenshots.mkdir(parents=True, exist_ok=True)
    results = []

    with display_environment() as (env, temporary):
        cases = [
            ("navigation", "navigation", []),
            ("title", "title", ["--scenario", "title"]),
            ("title-traits", "title-traits", ["--scenario", "title"]),
            ("title-city", "title-city", ["--scenario", "title"]),
            ("pending", "pending", ["--delay-frames", "25"]),
            ("loading", "loading", ["--scenario", "loading"]),
            ("settings", "settings", []),
            ("host-scenarios", "host-scenarios", []),
            ("status-links", "status-links", ["--scenario", "errors"]),
            ("minimize", "minimize", []),
            ("minimize-150", "minimize", ["--scale", "1.5"]),
            ("autoload", "autoload", ["--auto-load", "--scenario", "title"]),
            ("minimize-fallback", "minimize", ["--no-atlas"]),
            ("fallback", "navigation", ["--no-atlas"]),
        ]
        for name, script_name, options in cases:
            text = (root / "tools/preview/tests" / (script_name + ".txt")).read_text(encoding="utf-8")
            destination = output / name if name != script_name else output
            (destination / "screenshots").mkdir(parents=True, exist_ok=True)
            text = text.replace("out/ui-preview", destination.as_posix())
            script = temporary / (name + ".txt")
            script.write_text(text, encoding="utf-8")
            command = [str(executable), "--script", str(script), "--frames", "500", *options]
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=60)
            (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            match = re.search(r"RESULT frames=(\d+) checks=(\d+) acknowledged_sample_commands=(\d+) unsupported_game_commands=(\d+)", result.stdout)
            passed = result.returncode == 0 and match is not None
            values = dict(zip(("frames", "checks", "sample_commands", "unsupported_commands"),
                              map(int, match.groups()))) if match else {}
            results.append({"case": name, "passed": passed, "exit_code": result.returncode, **values})
            print(("PASS " if passed else "FAIL ") + name + " " + str(values), flush=True)
            if not passed:
                print(result.stdout + result.stderr, flush=True)

        for scenario in ("ready", "title", "errors", "external"):
            path = screenshots / ("overview-" + scenario + ".png")
            command = [str(executable), "--scenario", scenario, "--frames", "8", "--screenshot", str(path)]
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=30)
            passed = result.returncode == 0 and path.exists()
            results.append({"case": "render-" + scenario, "passed": passed, "exit_code": result.returncode})
            print(("PASS " if passed else "FAIL ") + "render-" + scenario, flush=True)
            if not passed:
                print(result.stdout + result.stderr, flush=True)

    pngs = sorted(output.rglob("*.png"))
    corrupt_pngs = [str(path.relative_to(output)) for path in pngs if not valid_png(path)]
    report = {
        "scope": "Actual shared ImGui UI, SDL input, sample UiBridge. No game memory, injection or save/config files.",
        "results": results,
        "checks": sum(row.get("checks", 0) for row in results),
        "passed": all(row["passed"] for row in results) and not corrupt_pngs,
        "screenshots": [str(path.relative_to(output)) for path in pngs],
        "invalid_screenshots": corrupt_pngs,
    }
    (output / "validation.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("PNG verification:", len(pngs), "files,", len(corrupt_pngs), "invalid", flush=True)
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
