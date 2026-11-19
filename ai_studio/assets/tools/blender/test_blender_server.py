"""Warm Blender server: same results as a cold run, clean state per job, cold fallback everywhere.

Starts real Blender processes; skipped where none is installed. Each case runs its server for a
throwaway Studio root, so a server the lead runs for the checkout is never touched.
"""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import textwrap
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

from ai_studio.assets.tools.blender import blender_server as B
from ai_studio.assets.tools.blender import run_blender
from ai_studio.assets.tools.blender.lib.studio_root import MARKER

PROBE = textwrap.dedent("""
    import json, os, sys
    import bpy
    sys.path.insert(0, os.path.dirname(__file__))
    import helper
    out = sys.argv[sys.argv.index("--") + 1]
    json.dump({"objects": sorted(o.name for o in bpy.data.objects), "argv": sys.argv[sys.argv.index("--"):],
               "name": __name__, "cwd": os.getcwd(), "path0": sys.path[1], "helper": helper.VALUE,
               "pid": os.getpid()}, open(out, "w"))
    bpy.data.objects.new("Probe", None)
    print("probe done")
""")


def setUpModule():
    try:
        run_blender.find_blender()
    except FileNotFoundError as error:
        raise unittest.SkipTest(str(error))


def raw(state, payload, reply=True):
    """Send bytes as-is to the server; returns its reply or None."""
    with socket.create_connection(("127.0.0.1", state["port"]), timeout=5) as conn:
        conn.sendall(payload)
        if not reply:
            return None
        try:
            return B._receive(conn)
        except (OSError, ValueError):
            return None


def raw_job(state, script, timeout):
    """An open connection holding a job; the caller decides when (or whether) to close it."""
    conn = socket.create_connection(("127.0.0.1", state["port"]), timeout=5)
    B._send(conn, {"op": "job", "token": state["token"], "script": str(script), "argv": [],
                   "log": str(Path(script).with_suffix(".log")), "cwd": os.getcwd(),
                   "env": dict(os.environ), "timeout": timeout})
    return conn


def wait_until(condition, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.1)
    return False


def fake_root():
    root = Path(tempfile.mkdtemp(prefix="blender_server_"))
    (root / MARKER).mkdir(parents=True)
    (root / "jobs").mkdir()
    return root


class ServerCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.env = mock.patch.dict(os.environ, {"NT_BLENDER_SERVER": "1"})
        cls.env.start()
        os.environ.pop("NT_STUDIO_ROOT", None)
        cls.root = fake_root()
        cls.addClassCleanup(shutil.rmtree, cls.root, True)
        cls.addClassCleanup(cls.env.stop)
        cls.addClassCleanup(B.stop_server, cls.root)
        cls.state = B.start_server(root=cls.root, idle_minutes=5)
        if not cls.state:
            raise AssertionError("server did not start: " + (cls.root / B.SERVER_LOG).read_text(errors="replace"))

    def script(self, name, body):
        path = self.root / "jobs" / name
        path.write_text(textwrap.dedent(body), encoding="utf-8")
        return path

    def run_job(self, script, *args, server="auto", timeout=120):
        log = self.root / "logs" / f"{script.stem}_{len(list((self.root).glob('logs/*')))}.log"
        return run_blender.run(script, [str(a) for a in args], log=log, timeout=timeout, server=server)

    def probe(self, server="auto"):
        out = self.root / "probe.json"
        result = self.run_job(self.root / "jobs" / "probe.py", out, "two words", server=server)
        self.assertEqual(result["code"], 0, Path(result["log"]).read_text(errors="replace"))
        return result, json.loads(out.read_text(encoding="utf-8"))

    def test_jobs_match_a_cold_run_and_start_from_a_fresh_scene(self):
        self.script("helper.py", "VALUE = 1\n")
        self.script("probe.py", PROBE)
        cold, cold_seen = self.probe(server=False)
        first, seen = self.probe()
        second, seen_again = self.probe()
        self.assertEqual((cold["via"], first["via"], second["via"]), ("cold", "server", "server"))
        self.assertEqual(seen["pid"], self.state["pid"])
        self.assertEqual(seen_again["pid"], self.state["pid"])
        self.assertNotIn("Probe", seen_again["objects"])
        self.assertEqual({k: v for k, v in seen.items() if k != "pid"},
                         {k: v for k, v in cold_seen.items() if k != "pid"})
        self.assertLess(second["seconds"], cold["seconds"])
        self.assertIn("probe done", Path(second["log"]).read_text(encoding="utf-8"))
        self.assertEqual(second["version"], cold["version"])

    def test_edited_job_modules_are_reloaded(self):
        self.script("probe.py", PROBE)
        self.script("helper.py", "VALUE = 'first'\n")
        self.assertEqual(self.probe()[1]["helper"], "first")
        self.script("helper.py", "VALUE = 'second edit'\n")
        result, seen = self.probe()
        self.assertEqual((result["via"], seen["helper"]), ("server", "second edit"))

    def test_failing_jobs_return_their_code_and_the_server_keeps_serving(self):
        raising = self.script("raising.py", "raise RuntimeError('job broke')\n")
        exiting = self.script("exiting.py", "import sys\nsys.exit(3)\n")
        fine = self.script("fine.py", "print('fine')\n")
        result = self.run_job(raising)
        self.assertEqual((result["via"], result["code"]), ("server", 1))
        self.assertIn("RuntimeError: job broke", Path(result["log"]).read_text(encoding="utf-8"))
        self.assertEqual(self.run_job(exiting, server=False)["code"], 3)
        self.assertEqual(self.run_job(exiting)["code"], 3)
        result = self.run_job(fine)
        self.assertEqual((result["via"], result["code"]), ("server", 0))
        self.assertEqual(B.ping(self.root)["pid"], self.state["pid"])

    def test_edited_package_module_is_seen_through_from_import(self):
        # The Studio's packages have no __init__.py, so this is how every lib import looks.
        package = self.root / "jobs" / "nspkg"
        package.mkdir(exist_ok=True)
        (package / "mod.py").write_text("VALUE = 'old'\n", encoding="utf-8")
        script = self.script("from_import.py", """
            import json, os, sys
            sys.path.insert(0, os.path.dirname(__file__))
            from nspkg import mod
            json.dump(mod.VALUE, open(sys.argv[sys.argv.index("--") + 1], "w"))
        """)
        out = self.root / "from_import.json"
        self.assertEqual(self.run_job(script, out)["via"], "server")
        self.assertEqual(json.loads(out.read_text()), "old")
        (package / "mod.py").write_text("VALUE = 'new code'\n", encoding="utf-8")
        self.assertEqual(self.run_job(script, out)["via"], "server")
        self.assertEqual(json.loads(out.read_text()), "new code")

    def test_job_sees_the_caller_environment_only_for_that_job(self):
        script = self.script("env.py", """
            import json, os, sys
            json.dump(os.environ.get("NT_TEST_JOB_VAR"), open(sys.argv[sys.argv.index("--") + 1], "w"))
        """)
        out = self.root / "env.json"
        with mock.patch.dict(os.environ, {"NT_TEST_JOB_VAR": "from caller"}):
            self.assertEqual(self.run_job(script, out)["via"], "server")
        self.assertEqual(json.loads(out.read_text()), "from caller")
        self.assertEqual(self.run_job(script, out)["via"], "server")
        self.assertIsNone(json.loads(out.read_text()))
        # Blender reads these only at start-up, so a warm process cannot honour a new value.
        with mock.patch.dict(os.environ, {"BLENDER_NT_TEST_FLAG": "1"}):
            self.assertEqual(self.run_job(script, out)["via"], "cold")

    def test_malformed_requests_get_an_error_and_the_server_keeps_serving(self):
        token = self.state["token"]
        for payload in (b"[1, 2]\n", b'"text"\n', '{"token": "\u0436"}\n'.encode("utf-8"),
                        json.dumps({"token": token, "op": "job", "argv": 5}).encode() + b"\n",
                        json.dumps({"token": token, "op": 7}).encode() + b"\n"):
            reply = raw(self.state, payload)
            self.assertIn("error", reply, payload)
        raw(self.state, b"\xff\xfe\n", reply=False)
        self.assertEqual(B.ping(self.root)["pid"], self.state["pid"])

    def test_a_busy_server_is_neither_replaced_nor_raced(self):
        marker = self.root / "busy.started"
        marker.unlink(missing_ok=True)
        script = self.script("busy.py", f"""
            import time
            open({str(marker)!r}, "w").close()
            time.sleep(3)
        """)
        results = []
        worker = threading.Thread(target=lambda: results.append(self.run_job(script)))
        worker.start()
        self.assertTrue(wait_until(marker.exists, 30))
        self.assertEqual(B.ensure_server(root=self.root)["pid"], self.state["pid"])
        self.assertEqual(B.start_server(root=self.root, lock_wait=0.5)["pid"], self.state["pid"])
        worker.join(60)
        self.assertEqual((results[0]["via"], results[0]["code"]), ("server", 0))
        self.assertEqual(B.read_state(self.root)["pid"], self.state["pid"])

    def test_busy_server_or_disabled_env_runs_cold(self):
        fine = self.script("fine.py", "print('fine')\n")
        other = B._Lock(self.root)
        self.assertTrue(other.acquire())
        try:
            self.assertEqual(self.run_job(fine)["via"], "cold")
        finally:
            other.release()
        with mock.patch.dict(os.environ, {"NT_BLENDER_SERVER": "0"}):
            self.assertEqual(self.run_job(fine)["via"], "cold")
        self.assertEqual(self.run_job(fine)["via"], "server")


class LifecycleCase(unittest.TestCase):
    def setUp(self):
        patch = mock.patch.dict(os.environ, {"NT_BLENDER_SERVER": "1"})
        patch.start()
        self.addCleanup(patch.stop)
        os.environ.pop("NT_STUDIO_ROOT", None)
        self.root = fake_root()
        self.addCleanup(shutil.rmtree, self.root, True)
        self.addCleanup(B.stop_server, self.root)

    def test_stale_state_is_replaced_and_a_hung_job_kills_the_server(self):
        dead = subprocess.Popen([sys.executable, "-c", "pass"])
        dead.wait()
        (self.root / "tmp").mkdir()
        (self.root / B.STATE).write_text(json.dumps({"pid": dead.pid, "port": 9, "token": "x",
                                                     "blender": str(run_blender.find_blender())}))
        fine = self.root / "jobs" / "fine.py"
        fine.write_text("print('fine')\n", encoding="utf-8")
        result = run_blender.run(fine, quiet=True)
        self.assertEqual((result["via"], result["code"]), ("cold", 0))
        self.assertFalse((self.root / B.STATE).exists())

        state = B.start_server(root=self.root, idle_minutes=5)
        self.assertTrue(state and state["pid"] != dead.pid)
        hung = self.root / "jobs" / "hung.py"
        hung.write_text("import time\ntime.sleep(120)\n", encoding="utf-8")
        result = run_blender.run(hung, quiet=True, timeout=3)
        self.assertEqual((result["via"], result["code"]), ("server", -9))
        self.assertFalse(B.is_server(state))
        self.assertIsNone(B.read_state(self.root))

    def test_a_recycled_pid_is_neither_used_nor_killed(self):
        bystander = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
        self.addCleanup(bystander.kill)
        (self.root / "tmp").mkdir()
        state = {"pid": bystander.pid, "created": "1", "port": 9, "token": "x",
                 "blender": str(run_blender.find_blender())}
        (self.root / B.STATE).write_text(json.dumps(state))
        self.assertIsNone(B.live_state(self.root))
        self.assertFalse((self.root / B.STATE).exists())
        (self.root / B.STATE).write_text(json.dumps(state))
        self.assertIsNone(B.stop_server(self.root))
        B._kill(state, self.root)
        self.assertIsNone(bystander.poll())

    def test_simultaneous_starts_launch_one_server(self):
        started = []
        threads = [threading.Thread(target=lambda: started.append(B.start_server(root=self.root, idle_minutes=2)))
                   for _ in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(120)
        self.assertEqual(len({s["pid"] for s in started if s}), 1, started)
        self.assertEqual(B.read_state(self.root)["pid"], started[0]["pid"])

    def test_an_orphaned_or_overdue_job_ends_the_server(self):
        hung = self.root / "jobs" / "hung.py"
        hung.write_text("import time\ntime.sleep(120)\n", encoding="utf-8")
        state = B.start_server(root=self.root, idle_minutes=5)
        conn = raw_job(state, hung, timeout=3600)
        time.sleep(1)
        conn.close()
        self.assertTrue(wait_until(lambda: not B.is_server(state), 10), "server outlived its caller")
        self.assertIsNone(B.read_state(self.root))

        state = B.start_server(root=self.root, idle_minutes=5)
        conn = raw_job(state, hung, timeout=0.5)
        self.addCleanup(conn.close)
        self.assertTrue(wait_until(lambda: not B.is_server(state), 15), "server outlived the job deadline")


if __name__ == "__main__":
    unittest.main()
