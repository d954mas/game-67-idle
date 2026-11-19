"""Warm Blender server: same results as a cold run, clean state per job, cold fallback everywhere.

Starts real Blender processes; skipped where none is installed. Each case runs its server for a
throwaway Studio root, so a server the lead runs for the checkout is never touched.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
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
        self.assertFalse(B._alive(state["pid"]))
        self.assertIsNone(B.read_state(self.root))


if __name__ == "__main__":
    unittest.main()
