import importlib.util
import json
from pathlib import Path
import subprocess
from types import SimpleNamespace
import unittest
from unittest.mock import mock_open, patch


spec = importlib.util.spec_from_file_location("host_status", Path(__file__).with_name("host-status.py"))
host_status = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host_status)


class HostStatusTests(unittest.TestCase):
    def collect(self, failed=None, logs=""):
        def run(args, **kwargs):
            if failed and failed(args):
                raise subprocess.CalledProcessError(1, args, stderr="private details")
            if args[1] == "ps":
                output = "stand-meta-1\nrooms-room-1\n"
            elif args[1] == "inspect":
                output = "0" if args[3] == "{{.RestartCount}}" else json.dumps({
                    "Status": "running", "OOMKilled": False, "StartedAt": "2026-09-27T00:00:00Z"})
            elif args[1] == "logs":
                output = logs
            elif args[1] == "stats":
                output = '{"Name":"stand-meta-1","CPUPerc":"1%"}'
            else:
                raise AssertionError(args)
            return SimpleNamespace(stdout=output, stderr="")

        with patch.object(host_status.subprocess, "run", side_effect=run), \
                patch.object(host_status.time, "sleep"), \
                patch("builtins.open", mock_open(read_data="MemTotal: 1000 kB\nMemAvailable: 600 kB\n")), \
                patch.object(host_status.os, "getloadavg", return_value=(0.1, 0.2, 0.3), create=True), \
                patch.object(host_status.shutil, "disk_usage", return_value=SimpleNamespace(total=1000, used=100, free=900)):
            return host_status.collect()

    def test_disappearing_container_preserves_other_container_and_host(self):
        result = self.collect(lambda args: args[1] in ("inspect", "logs") and args[-1] == "rooms-room-1")
        self.assertEqual(result["containers"][0]["status"], "running")
        self.assertTrue(result["containers"][1]["errors"])
        self.assertEqual(result["memory"]["MemAvailable_kib"], 600)
        self.assertEqual(result["disk_bytes"]["free"], 900)
        self.assertNotIn("private details", json.dumps(result))

    def test_stats_failure_preserves_containers_and_host(self):
        result = self.collect(lambda args: args[1] == "stats")
        self.assertEqual(len(result["containers"]), 2)
        self.assertTrue(result["errors"]["container_resources"])
        self.assertEqual(result["memory"]["MemTotal_kib"], 1000)

    def test_log_failure_preserves_container_state(self):
        result = self.collect(lambda args: args[1] == "logs")
        self.assertEqual(result["containers"][0]["status"], "running")
        self.assertTrue(result["containers"][0]["errors"]["logs"])

    def test_docker_unavailable_preserves_host_metrics(self):
        result = self.collect(lambda args: args[1] == "ps")
        self.assertEqual(result["containers"], [])
        self.assertTrue(result["errors"]["containers"])
        self.assertEqual(result["disk_bytes"]["free"], 900)
        self.assertEqual(result["memory"]["MemAvailable_kib"], 600)

    def test_optional_room_stage_timings(self):
        result = self.collect(logs="plw_room_stats t=12 ticks=50 host_tick_avg_ms=1.25 host_tick_max_ms=4.5 replication_avg_ms=0.2 replication_max_ms=0.8\n")
        counters = result["containers"][0]["room_cumulative"]
        self.assertEqual(counters["ticks"], 50)
        self.assertEqual(counters["host_tick_avg_ms"], 1.25)
        self.assertEqual(counters["host_tick_max_ms"], 4.5)
        self.assertEqual(counters["replication_avg_ms"], 0.2)
        self.assertEqual(counters["replication_max_ms"], 0.8)

    def test_room_history_preserves_close_sources_and_times(self):
        logs = (
            "2026-09-27T18:01:00.123Z plw_room_stats t=60 ticks=1800 late=1 dropped=0 seated=2 "
            "closes=peer:1,protocol:0,rate:0,slow:2,app:0,other:0 "
            "slow_sources=outbox:1,transport:1\n"
            "2026-09-27T18:02:00.123Z plw_room_stats t=120 ticks=3600 late=1 dropped=0 seated=1 "
            "closes=peer:2,protocol:0,rate:1,slow:3,app:0,other:0 "
            "slow_sources=outbox:1,transport:2\n"
        )
        room = self.collect(logs=logs)["containers"][1]
        self.assertEqual(len(room["room_history"]), 2)
        self.assertEqual(room["room_history"][0]["at"], "2026-09-27T18:01:00.123Z")
        self.assertEqual(room["room_history"][1]["seated"], 1)
        self.assertEqual(room["room_history"][1]["slow_sources"], {"outbox": 1, "transport": 2})
        self.assertEqual(room["room_cumulative"]["closes"]["slow"], 3)

    def test_room_versions_come_from_any_stats_line(self):
        logs = 'plw_room_stats t=60 ticks=1800 seated=0 ws_library="uWebSockets 20.80.0" tls_library="OpenSSL 3.5.1 1 Jul 2025"\n'
        room = self.collect(logs=logs)["containers"][1]
        self.assertEqual(room["room_versions"], {"ws_library": "uWebSockets 20.80.0", "tls_library": "OpenSSL 3.5.1 1 Jul 2025"})

    def test_system_profile_rates_from_two_readings(self):
        files = {
            "/proc/cpuinfo": "model name\t: Test CPU\n",
            "/proc/sys/net/core/rps_sock_flow_entries": "32768\n",
            "/proc/net/route": "Iface\tDestination\tGateway\neth0\t00000000\t0101A8C0\n",
            "/sys/class/net/eth0/queues/rx-0/rps_cpus": "f\n",
            "/sys/class/net/eth0/queues/rx-0/rps_flow_cnt": "4096\n",
            "/sys/class/net/eth0/queues/tx-0/xps_cpus": "0\n",
            "/proc/irq/24/smp_affinity_list": "0-3\n",
            "/proc/4242/comm": "plw_room_server\n",
            "/proc/4242/statm": "100 50 0 0 0 0 0\n",
        }
        # The room's stat ends at field 39, the CPU it last ran on.
        readings = [
            {"/proc/stat": "cpu 100 0 100 700 0 0 100 0 0 0\ncpu0 50 0 50 350 0 0 100 0 0 0\ncpu1 50 0 50 350 0 0 0 0 0 0\n"
                           "ctxt 1000\nprocesses 50\n", "/proc/softirqs": "NET_RX: 10 10\nNET_TX: 1 1\n",
             "/proc/net/softnet_stat": "00000064 00000000 00000000 0 0 0 0 0 0 00000000 0\n00000064 00000000 00000000 0 0 0 0 0 0 00000064 0\n",
             "/proc/interrupts": "  CPU0 CPU1\n 24: 10 0 PCI-MSI virtio0-input.0\n",
             "/proc/4242/stat": "4242 (plw room) S " + "0 " * 10 + "100 50 " + "0 " * 23 + "1\n",
             "/proc/4242/status": "voluntary_ctxt_switches:\t10\nnonvoluntary_ctxt_switches:\t2\n",
             "rx_bytes": "1000", "tx_bytes": "2000", "rx_packets": "10", "tx_packets": "20"},
            {"/proc/stat": "cpu 150 0 150 800 0 0 100 0 0 0\ncpu0 50 0 50 400 0 0 100 0 0 0\ncpu1 100 0 100 400 0 0 0 0 0 0\n"
                           "ctxt 3000\nprocesses 53\n", "/proc/softirqs": "NET_RX: 60 60\nNET_TX: 6 6\n",
             "/proc/net/softnet_stat": "000000c8 00000001 00000002 0 0 0 0 0 0 00000000 0\n000000c8 00000000 00000000 0 0 0 0 0 0 000000c8 0\n",
             "/proc/interrupts": "  CPU0 CPU1\n 24: 110 0 PCI-MSI virtio0-input.0\n",
             "/proc/4242/stat": "4242 (plw room) S " + "0 " * 10 + "150 60 " + "0 " * 23 + "1\n",
             "/proc/4242/status": "voluntary_ctxt_switches:\t300\nnonvoluntary_ctxt_switches:\t12\n",
             "rx_bytes": "3000", "tx_bytes": "6000", "rx_packets": "30", "tx_packets": "60"},
        ]
        state = {"reading": -1}

        def fake_read(path):
            if path == "/proc/stat":
                state["reading"] += 1
            current = readings[max(0, state["reading"])]
            if path in current:
                return current[path]
            if path.startswith("/sys/class/net/eth0/statistics/"):
                return current[path.rsplit("/", 1)[-1]]
            return files[path]

        def listdir(path):
            return {"/proc": ["4242", "self"], "/sys/class/net/eth0/queues/": ["rx-0", "tx-0"]}[path]

        with patch.object(host_status, "read", side_effect=fake_read), patch.object(host_status.os, "listdir", side_effect=listdir), \
                patch.object(host_status.os, "sysconf", side_effect=lambda name: 100 if name == "SC_CLK_TCK" else 4096, create=True), \
                patch.object(host_status.os, "uname", return_value=SimpleNamespace(release="6.12.0"), create=True), \
                patch.object(host_status, "command", return_value="OpenSSL 3.5.1 1 Jul 2025\n"), \
                patch.object(host_status.time, "sleep"):
            errors = {}
            profile = host_status.system_profile(errors)
        self.assertEqual(errors, {})
        self.assertEqual(profile["kernel"], "6.12.0")
        self.assertEqual(profile["cpu_model"], "Test CPU")
        self.assertEqual(profile["cpu_pct"]["busy"], 50.0)
        self.assertEqual(profile["softirqs_per_s"], {"NET_RX": 100, "NET_TX": 10})
        self.assertEqual(profile["nic_rates"]["eth0"]["tx_packets"], 40)
        self.assertEqual(profile["nics"]["eth0"]["rps_cpus"], {"rx-0": "f"})
        self.assertEqual(profile["nics"]["eth0"]["irqs"]["24"]["affinity"], "0-3")
        rooms = profile["room_processes"]
        self.assertEqual((rooms["count"], rooms["user_pct"], rooms["system_pct"], rooms["rss_kib"]), (1, 50.0, 10.0, 200))
        self.assertEqual((rooms["switches_per_s"], profile["context_switches_per_s"], profile["forks_per_s"]), (300, 2000, 3))
        self.assertEqual(profile["process_groups"]["rooms"]["count"], 1)
        # Receive work lands on CPU 0 and RPS hands it to CPU 1, where the room runs.
        first, second = profile["per_cpu"]
        self.assertEqual((first["softirq"], first["net_rx_per_s"], first["backlog_dropped"], first["budget_squeezed"], first["rooms"]),
                         (0.0, 50, 1, 2, 0))
        self.assertEqual((second["rps_received_per_s"], second["rooms"], second["rooms_pct"], second["busy"]), (100, 1, 60.0, 66.67))
        self.assertEqual(profile["nic_irqs_per_s"], {"virtio0-input.0": [100, 0]})


if __name__ == "__main__":
    unittest.main()
