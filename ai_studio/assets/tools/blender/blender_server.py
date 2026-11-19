"""Keep one Blender warm so tool scripts skip its start-up and first-frame cost.

    python blender_server.py start [--blender PATH] [--idle-minutes 30]
    python blender_server.py status
    python blender_server.py stop

`run_blender.run()` uses a live server by itself and falls back to a cold Blender whenever the
server is missing, busy, stale, for another Blender or Studio root, or dies mid-job; it never
starts one unless asked (`server=True`, or `ensure_server()`). `NT_BLENDER_SERVER=0` disables it.

Each job runs a script file through runpy with the same sys.argv a cold run sees, its output in
the job log, then the server answers and resets to factory settings. Pure-Python modules the job
imported (Studio library, game helpers) are dropped after every job, so edits apply on the next
job; an edit to this file makes the server exit. One job at a time: a second caller runs cold
instead of waiting. The server listens on 127.0.0.1 only, requires the token from
`<studio root>/tmp/blender_server.json`, and exits after `--idle-minutes` without a job.

The Codex/Claude sandbox blocks launching .exe files; start it outside the sandbox.
"""
import argparse
import hmac
import json
import os
import re
import secrets
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# Inside Blender the server gets its Studio root from the command line, like a cold job does.
if __package__ in (None, "") and "bpy" not in sys.modules:
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

SOURCE = Path(__file__).resolve()
STATE = Path("tmp") / "blender_server.json"
LOCK = Path("tmp") / "blender_server.lock"
SERVER_LOG = Path("tmp") / "blender_server.log"
# An idle server answers in a millisecond, but it resets the scene right after replying to the
# previous job (30-60 ms), so a tighter ping would send back-to-back jobs to a cold Blender.
PING_SECONDS = 0.5
CONNECT_SECONDS = 0.2


# ---------------------------------------------------------------- shared wire protocol

def _send(conn, message):
    conn.sendall(json.dumps(message).encode("utf-8") + b"\n")


def _receive(conn, limit=1 << 20):
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(65536)
        if not chunk:
            raise ConnectionError("connection closed")
        data += chunk
        if len(data) > limit:
            raise ValueError("message too large")
    return json.loads(data.decode("utf-8"))


# ---------------------------------------------------------------- client side (plain Python)

def studio_root(start=None):
    from ai_studio.assets.tools.blender.lib.studio_root import find_studio_root
    return Path(find_studio_root(start or SOURCE.parent))


def read_state(root):
    path = Path(root) / STATE
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def _alive(pid):
    if os.name == "nt":
        import ctypes
        kernel = ctypes.windll.kernel32
        handle = kernel.OpenProcess(0x1000, False, int(pid))  # PROCESS_QUERY_LIMITED_INFORMATION
        if not handle:
            return False
        code = ctypes.c_ulong()
        ok = kernel.GetExitCodeProcess(handle, ctypes.byref(code))
        kernel.CloseHandle(handle)
        return bool(ok) and code.value == 259  # STILL_ACTIVE
    try:
        os.kill(int(pid), 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def _image(pid):
    """Executable of `pid`, or None when unknown; guards kills against a recycled pid."""
    if os.name == "nt":
        import ctypes
        kernel = ctypes.windll.kernel32
        handle = kernel.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return None
        buffer, size = ctypes.create_unicode_buffer(1024), ctypes.c_ulong(1024)
        ok = kernel.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size))
        kernel.CloseHandle(handle)
        return buffer.value if ok else None
    try:
        return os.readlink(f"/proc/{int(pid)}/exe")
    except OSError:
        return None


def _kill(state, root):
    pid = state.get("pid")
    image = _image(pid) if pid else None
    if pid and _alive(pid) and (image is None or "blender" in Path(image).name.lower()):
        try:
            os.kill(int(pid), 9 if os.name != "nt" else 15)
        except OSError:
            pass
        for _ in range(50):
            if not _alive(pid):
                break
            time.sleep(0.1)
    _forget(root, state)


def _forget(root, state=None):
    """Remove the state file, but only if it still describes `state` (a newer server may own it)."""
    path = Path(root) / STATE
    current = read_state(root)
    if current is not None and state is not None and current.get("pid") != state.get("pid"):
        return
    try:
        path.unlink()
    except OSError:
        pass


def _request(state, message, timeout):
    with socket.create_connection(("127.0.0.1", int(state["port"])), timeout=CONNECT_SECONDS) as conn:
        conn.settimeout(timeout)
        _send(conn, {**message, "token": state["token"]})
        return _receive(conn)


def live_state(root):
    """State of a server that is alive for `root`, else None; a dead server's file is removed."""
    state = read_state(root)
    if not state:
        return None
    if not _alive(state.get("pid", 0)):
        _forget(root, state)
        return None
    return state


def ping(root, timeout=PING_SECONDS):
    state = live_state(root)
    if not state:
        return None
    try:
        reply = _request(state, {"op": "ping"}, timeout)
    except (OSError, ValueError):
        return None
    return {**state, **reply} if reply.get("ok") else None


class _Lock:
    """Non-blocking per-root lock: the holder owns the server for one job; others run cold."""

    def __init__(self, root):
        self.path = Path(root) / LOCK
        self.file = None

    def acquire(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.file = open(self.path, "a+b")
        try:
            if os.name == "nt":
                import msvcrt
                self.file.seek(0)
                msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            return True
        except OSError:
            self.file.close()
            self.file = None
            return False

    def release(self):
        if self.file:
            if os.name == "nt":
                import msvcrt
                self.file.seek(0)
                msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
            self.file.close()
            self.file = None


def _discard(path):
    if path:
        try:
            os.unlink(path)
        except OSError:  # a just-killed Blender may still hold it for a moment
            pass


def _same_file(a, b):
    return os.path.normcase(str(Path(a).resolve())) == os.path.normcase(str(Path(b).resolve()))


def try_run(blender, script, script_args=(), log=None, timeout=3600, quiet=False, start=False,
            studio=None):
    """Run `script` on a warm server; returns run_blender's result dict, or None to run cold."""
    script = Path(script).resolve()
    root = Path(studio or studio_root(script.parent))
    if start:
        ensure_server(blender, root=root)
    state = live_state(root)
    if not state or not _same_file(state.get("blender", ""), blender):
        return None
    lock = _Lock(root)
    if not lock.acquire():
        return None
    try:
        return _run_locked(state, root, blender, script, script_args, log, timeout, quiet)
    finally:
        lock.release()


def _run_locked(state, root, blender, script, script_args, log, timeout, quiet):
    started = time.monotonic()
    try:
        if not _request(state, {"op": "ping"}, PING_SECONDS).get("ok"):
            return None
    except (OSError, ValueError):
        return None
    temp_log = None
    if log:
        log_path = Path(log).resolve()
    else:
        handle, temp_log = tempfile.mkstemp(prefix="blender_job_", suffix=".log")
        os.close(handle)
        log_path = Path(temp_log)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    job = {"op": "job", "script": str(script), "argv": [str(a) for a in script_args],
           "log": str(log_path), "cwd": os.getcwd()}
    try:
        with socket.create_connection(("127.0.0.1", int(state["port"])), timeout=CONNECT_SECONDS) as conn:
            _send(conn, {**job, "token": state["token"]})
            conn.settimeout(max(1.0, timeout - (time.monotonic() - started)))
            try:
                reply = _receive(conn)
            except socket.timeout:
                # The server is stuck in this job; a cold run would hang the same way.
                _kill(state, root)
                reply = {"code": -9}
    except (OSError, ValueError) as error:
        # The server died mid-job (or refused it): kill what is left so no one reuses it, and let
        # the caller rerun the job cold, which is what it would have done without a server.
        _kill(state, root)
        _discard(temp_log)
        print(f"blender_server: job failed on the server ({error}); running cold", file=sys.stderr)
        return None
    if "code" not in reply:
        _discard(temp_log)
        return None
    if temp_log:
        if not quiet:
            sys.stdout.write(Path(temp_log).read_text(encoding="utf-8", errors="replace"))
            sys.stdout.flush()
        _discard(temp_log)
    version = re.search(r"\d+\.\d+\.\d+", state.get("version") or "")
    return {"code": reply["code"], "seconds": round(time.monotonic() - started, 2),
            "blender": str(blender), "version": version.group(0) if version else None,
            "log": str(log_path) if log else None, "via": "server"}


def start_server(blender=None, root=None, idle_minutes=30, wait=60):
    """Launch a detached warm Blender for `root`; returns its live state or None."""
    from ai_studio.assets.tools.blender import run_blender
    root = Path(root or studio_root())
    current = ping(root)
    if current:
        return current
    exe = run_blender.find_blender(blender)
    (root / "tmp").mkdir(parents=True, exist_ok=True)
    _forget(root)
    command = run_blender.blender_command(exe, SOURCE, ["serve", "--root", str(root), "--idle-minutes",
                                                        str(idle_minutes)], studio_root=root)
    sink = open(root / SERVER_LOG, "w", encoding="utf-8", errors="replace")
    options = {"stdin": subprocess.DEVNULL, "stdout": sink, "stderr": subprocess.STDOUT,
               "cwd": str(root), "close_fds": True}
    try:
        if os.name == "nt":
            detached = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
            try:
                # Outlive a tool runner that kills its job object when the command returns.
                process = subprocess.Popen(command, creationflags=detached | subprocess.CREATE_BREAKAWAY_FROM_JOB,
                                           **options)
            except OSError:
                process = subprocess.Popen(command, creationflags=detached, **options)
        else:
            process = subprocess.Popen(command, start_new_session=True, **options)
    finally:
        sink.close()
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        if process.poll() is not None:
            return None
        state = read_state(root)
        if state and state.get("pid") == process.pid:
            process.returncode = 0  # detached on purpose; keeps Popen from warning it still runs
            return ping(root) or state
        time.sleep(0.1)
    process.kill()
    return None


def ensure_server(blender=None, root=None, idle_minutes=30):
    """A live server for `root`, started if needed; None when it cannot start (callers go cold)."""
    try:
        return start_server(blender, root, idle_minutes)
    except Exception as error:  # a helper must never turn a working cold run into a failure
        print(f"blender_server: not started ({error})", file=sys.stderr)
        return None


def stop_server(root=None):
    """Stop the server for `root`; returns its pid or None. A busy server is killed."""
    root = Path(root or studio_root())
    state = live_state(root)
    if not state:
        return None
    try:
        _request(state, {"op": "stop"}, 3.0)
        for _ in range(50):
            if not _alive(state["pid"]):
                break
            time.sleep(0.1)
    except (OSError, ValueError):
        pass
    if _alive(state["pid"]):
        _kill(state, root)
    _forget(root, state)
    return state["pid"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=("start", "stop", "status"))
    parser.add_argument("--blender")
    parser.add_argument("--idle-minutes", type=float, default=30)
    args = parser.parse_args(argv)
    root = studio_root()
    if args.command == "start":
        state = start_server(args.blender, root, args.idle_minutes)
        if not state:
            print(f"blender_server: failed to start, see {root / SERVER_LOG}", file=sys.stderr)
            return 1
        print(json.dumps({k: v for k, v in state.items() if k not in ("token", "ok")}))
        return 0
    if args.command == "stop":
        pid = stop_server(root)
        print(f"stopped pid {pid}" if pid else "no server")
        return 0
    state = live_state(root)
    if not state:
        print("no server")
        return 1
    reply = ping(root, timeout=1.0)
    shown = reply or {**state, "busy": True}
    print(json.dumps({k: v for k, v in shown.items() if k not in ("token", "ok")}))
    return 0


# ---------------------------------------------------------------- server side (inside Blender)

class Server:
    def __init__(self, root, idle_minutes):
        self.root = Path(root)
        self.idle_seconds = idle_minutes * 60
        self.source_mtime = SOURCE.stat().st_mtime_ns
        self.started = time.time()
        self.last_active = time.monotonic()
        self.jobs = 0
        self.failed = 0
        self.warm_seconds = None

    def serve(self):
        import bpy
        # A factory reset re-runs the startup scripts, which prepend their folders to sys.path.
        self.job_path = list(sys.path)
        self.warm_seconds = warm_up()
        self.baseline = set(sys.modules)
        self.handlers = {name: list(getattr(bpy.app.handlers, name)) for name in dir(bpy.app.handlers)
                         if isinstance(getattr(bpy.app.handlers, name), list)}
        self.reset()
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(8)
        listener.settimeout(1.0)
        self.token = secrets.token_hex(16)
        self.state = {"pid": os.getpid(), "port": listener.getsockname()[1], "token": self.token,
                      "blender": bpy.app.binary_path, "version": bpy.app.version_string,
                      "started": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(self.started)),
                      "root": str(self.root), "idle_minutes": self.idle_seconds / 60}
        state_path = self.root / STATE
        temp = state_path.with_suffix(".tmp")
        temp.write_text(json.dumps(self.state, indent=2), encoding="utf-8")
        os.replace(temp, state_path)
        print(f"blender_server: pid {os.getpid()} port {self.state['port']} warm in {self.warm_seconds} s",
              flush=True)
        try:
            while True:
                try:
                    conn, _ = listener.accept()
                except socket.timeout:
                    if time.monotonic() - self.last_active > self.idle_seconds:
                        print("blender_server: idle timeout", flush=True)
                        break
                    continue
                with conn:
                    if not self.handle(conn):
                        break
        finally:
            listener.close()
            current = read_state(self.root)
            if current and current.get("pid") == os.getpid():
                try:
                    state_path.unlink()
                except OSError:
                    pass

    def handle(self, conn):
        """Serve one request; False stops the server."""
        conn.settimeout(5.0)
        try:
            request = _receive(conn)
        except (OSError, ValueError):
            return True
        if not hmac.compare_digest(str(request.get("token", "")), self.token):
            self._reply(conn, {"error": "bad token"})
            return True
        if SOURCE.stat().st_mtime_ns != self.source_mtime:
            self._reply(conn, {"error": "stale server source"})
            return False
        op = request.get("op")
        if op == "ping":
            self._reply(conn, {"ok": True, "jobs": self.jobs, "failed": self.failed,
                               "uptime_s": round(time.time() - self.started),
                               "idle_s": round(time.monotonic() - self.last_active),
                               "warm": self.warm_seconds is not None, "warm_up_s": self.warm_seconds,
                               "busy": False})
            return True
        if op == "stop":
            self._reply(conn, {"ok": True})
            return False
        if op != "job":
            self._reply(conn, {"error": f"unknown op {op!r}"})
            return True
        script, cwd = Path(str(request.get("script", ""))), Path(str(request.get("cwd", "")))
        if not (script.is_absolute() and script.is_file() and script.suffix == ".py" and cwd.is_dir()):
            self._reply(conn, {"error": "script must be an existing .py file and cwd a folder"})
            return True
        code = self.run_job(script, [str(a) for a in request.get("argv", [])],
                            Path(request["log"]), cwd)
        self.jobs += 1
        self.failed += code != 0
        self._reply(conn, {"code": code, "jobs": self.jobs})
        self.last_active = time.monotonic()
        try:
            self.reset()
        except Exception as error:
            print(f"blender_server: reset failed, exiting: {error}", flush=True)
            return False
        return True

    @staticmethod
    def _reply(conn, message):
        try:
            conn.settimeout(5.0)
            _send(conn, message)
        except OSError:
            pass

    def run_job(self, script, argv, log, cwd):
        import runpy
        import traceback
        import bpy
        saved = {"argv": list(sys.argv), "path": list(sys.path), "cwd": os.getcwd(),
                 "environ": dict(os.environ), "stdout": sys.stdout, "stderr": sys.stderr}
        log.parent.mkdir(parents=True, exist_ok=True)
        sink = open(log, "wb", buffering=0)
        sink.write(f"Blender {bpy.app.version_string} (warm server pid {os.getpid()}, job {self.jobs + 1})\n"
                   .encode("utf-8"))
        _flush_all()
        # Blender's own C output (render progress, importer warnings) goes to fd 1/2, so the
        # descriptors move, not just sys.stdout.
        fds = {fd: os.dup(fd) for fd in (1, 2)}
        for fd in fds:
            os.dup2(sink.fileno(), fd)
        sys.stdout = sys.stderr = open(sink.fileno(), "w", encoding="utf-8", errors="replace",
                                       buffering=1, closefd=False)
        code = 0
        try:
            os.chdir(cwd)
            # Exactly what a cold run_blender start shows the script.
            sys.argv = [bpy.app.binary_path, "--background", "--factory-startup", "--python-expr",
                        f"import sys; sys.path.insert(0, {str(self.root)!r})", "--python-exit-code", "1",
                        "--python", str(script), "--", *argv]
            sys.path = list(self.job_path)
            runpy.run_path(str(script), run_name="__main__")
        except SystemExit as stop:
            if stop.code is None or isinstance(stop.code, int):
                code = stop.code or 0
            else:
                print(stop.code, file=sys.stderr)
                code = 1
        except BaseException:
            traceback.print_exc()
            code = 1
        finally:
            _flush_all()
            for fd, copy in fds.items():
                os.dup2(copy, fd)
                os.close(copy)
            sink.close()
            sys.stdout, sys.stderr = saved["stdout"], saved["stderr"]
            sys.argv, sys.path = saved["argv"], saved["path"]
            os.chdir(saved["cwd"])
            os.environ.clear()
            os.environ.update(saved["environ"])
        return code

    def reset(self):
        """Back to what a fresh `--factory-startup` Blender holds, minus the warm caches."""
        import bpy
        bpy.ops.wm.read_factory_settings()
        for name, items in self.handlers.items():
            getattr(bpy.app.handlers, name)[:] = items
        install = Path(bpy.app.binary_path).resolve().parent
        for name in [n for n in sys.modules if n not in self.baseline]:
            module = sys.modules.get(name)
            file = getattr(module, "__file__", None) or ""
            # Only pure-Python files outside Blender and site-packages: compiled extensions
            # (numpy and the like) cannot be imported twice in one process.
            if (file.endswith(".py") and "site-packages" not in Path(file).parts
                    and install not in Path(file).resolve().parents):
                del sys.modules[name]


def _flush_all():
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.flush()
        except (OSError, ValueError, AttributeError):
            pass
    try:
        import ctypes
        libc = ctypes.CDLL("ucrtbase") if os.name == "nt" else ctypes.CDLL(None)
        libc.fflush(None)
    except (OSError, AttributeError):
        pass


def warm_up():
    """Pay the first EEVEE frame and the lazy glTF add-on imports once, at start."""
    import bpy
    import numpy  # noqa: F401
    started = time.monotonic()
    bpy.ops.wm.read_factory_settings()
    scene = bpy.context.scene
    engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 16
    scene.eevee.taa_render_samples = 1
    with tempfile.TemporaryDirectory(prefix="blender_server_warm_") as folder:
        scene.render.filepath = str(Path(folder) / "warm.png")
        bpy.ops.render.render(write_still=True)
        glb = str(Path(folder) / "warm.glb")
        bpy.ops.export_scene.gltf(filepath=glb, export_format="GLB")
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=glb)
    return round(time.monotonic() - started, 2)


def serve_main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("serve")
    parser.add_argument("--root", required=True)
    parser.add_argument("--idle-minutes", type=float, default=30)
    args = parser.parse_args(argv)
    Server(args.root, args.idle_minutes).serve()


if __name__ == "__main__":
    tail = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if tail[:1] == ["serve"]:
        serve_main(tail)
    else:
        sys.exit(main())
