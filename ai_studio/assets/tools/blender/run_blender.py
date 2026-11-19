"""Run a Python script inside headless Blender with the Studio library importable.

    python run_blender.py my_build.py -- --out tmp/model.glb
    python run_blender.py --which            # print the Blender that would run, and exit

Resolution order: --blender, $BLENDER, $BLENDER_EXECUTABLE, `blender` on PATH, then installed
versions newest first. The script runs with --factory-startup, so user add-ons and preferences
never change the result, and with the Studio root on sys.path, so it can
`from ai_studio.assets.tools.blender.lib import bpy_scene, glb`.

A warm server (`blender_server.py start`) takes the job when one is running for the same Blender
and Studio root and is free; otherwise Blender starts cold, with the same result. `--cold` or
NT_BLENDER_SERVER=0 skips the server, `--server` starts one first.

The Codex/Claude sandbox blocks launching .exe files; run this command outside the sandbox.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender.lib.studio_root import find_studio_root  # noqa: E402

INSTALL_ROOTS = [Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Blender Foundation",
                 Path("/Applications"), Path("/usr/share"), Path("/opt")]
PREFERRED = (5, 2)


def _version_of(path):
    match = re.search(r"(\d+)\.(\d+)", str(path))
    return tuple(int(v) for v in match.groups()) if match else (0, 0)


def installed_blenders():
    """Executables under the usual install folders, preferred version first, then newest."""
    found = []
    for root in INSTALL_ROOTS:
        if not root.is_dir():
            continue
        for folder in root.iterdir():
            if not folder.name.lower().startswith("blender"):
                continue
            for name in ("blender.exe", "blender", "Contents/MacOS/Blender"):
                exe = folder / name
                if exe.is_file():
                    found.append(exe)
    return sorted(found, key=lambda p: (_version_of(p) != PREFERRED, tuple(-v for v in _version_of(p))))


def find_blender(explicit=None):
    for candidate in (explicit, os.environ.get("BLENDER"), os.environ.get("BLENDER_EXECUTABLE")):
        if candidate:
            if Path(candidate).is_file():
                return Path(candidate)
            raise FileNotFoundError(f"Blender not found at {candidate}")
    on_path = shutil.which("blender")
    if on_path:
        return Path(on_path)
    installed = installed_blenders()
    if installed:
        return installed[0]
    raise FileNotFoundError("Blender not found: pass --blender or set BLENDER")


def blender_command(blender, script, script_args=(), studio_root=None, factory_startup=True):
    root = Path(studio_root or find_studio_root(Path(script).parent))
    command = [str(blender), "--background"]
    if factory_startup:
        command.append("--factory-startup")
    # Blender ignores PYTHONPATH unless told otherwise, so the import path goes in as code.
    command += ["--python-expr", f"import sys; sys.path.insert(0, {str(root)!r})",
                "--python-exit-code", "1", "--python", str(script), "--", *map(str, script_args)]
    return command


def run(script, script_args=(), blender=None, log=None, timeout=3600, factory_startup=True, quiet=False,
        server="auto"):
    """Run `script` in Blender; returns {"code", "seconds", "blender", "version", "log", "via"}.

    Output goes to the terminal unless `log` is given (then only to the file); a server job
    prints it when the job ends. A hung Blender is killed after `timeout` seconds (code -9).
    `server`: "auto" uses a live warm server, True starts one first, False always runs cold.
    """
    exe = find_blender(blender)
    if server and factory_startup and os.environ.get("NT_BLENDER_SERVER", "").strip() != "0":
        from ai_studio.assets.tools.blender import blender_server
        result = blender_server.try_run(exe, script, script_args, log, timeout, quiet, start=server is True)
        if result:
            return result
    command = blender_command(exe, script, script_args, factory_startup=factory_startup)
    started = time.monotonic()
    log_path = Path(log) if log else None
    if log_path:
        log_path.parent.mkdir(parents=True, exist_ok=True)
    sink = open(log_path, "w", encoding="utf-8", errors="replace") if log_path else None
    try:
        process = subprocess.Popen(command, stdout=sink or (subprocess.DEVNULL if quiet else None),
                                   stderr=subprocess.STDOUT if sink or quiet else None)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            code = -9
    finally:
        if sink:
            sink.close()
    version = None
    if log_path and log_path.is_file():
        match = re.search(r"Blender (\d+\.\d+\.\d+)", log_path.read_text(encoding="utf-8", errors="replace")[:4000])
        version = match.group(1) if match else None
    return {"code": code, "seconds": round(time.monotonic() - started, 2), "blender": str(exe),
            "version": version, "log": str(log_path) if log_path else None, "via": "cold"}


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    script_args = []
    if "--" in argv:
        split = argv.index("--")
        argv, script_args = argv[:split], argv[split + 1:]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("script", nargs="?")
    parser.add_argument("--blender")
    parser.add_argument("--log", help="write Blender output here instead of the terminal")
    parser.add_argument("--timeout", type=float, default=3600)
    parser.add_argument("--keep-user-prefs", action="store_true", help="skip --factory-startup")
    parser.add_argument("--which", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--cold", action="store_true", help="never use a warm server")
    mode.add_argument("--server", action="store_true", help="start a warm server if none runs")
    args = parser.parse_args(argv)
    if args.which:
        print(find_blender(args.blender))
        return 0
    if not args.script:
        parser.error("script is required")
    result = run(args.script, script_args, args.blender, args.log, args.timeout,
                 not args.keep_user_prefs, server=False if args.cold else (True if args.server else "auto"))
    if result["code"] != 0:
        where = f", see {result['log']}" if result["log"] else ""
        print(f"blender exited {result['code']} after {result['seconds']} s{where}", file=sys.stderr)
    return 0 if result["code"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
