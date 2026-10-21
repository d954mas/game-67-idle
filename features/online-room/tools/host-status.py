#!/usr/bin/env python3
"""Read-only host/container diagnostics; executed over SSH by server-status.mjs."""
import collections
import json
import os
import re
import shutil
import subprocess
import sys
import time


def command(*args):
    return subprocess.run(args, capture_output=True, text=True, timeout=25, check=True).stdout


def diagnostic_error(error):
    # Command output can contain credentials; expose only the failure class.
    return type(error).__name__ + ": diagnostic section unavailable"


# Seconds between the two /proc readings the rates come from; the first
# argument sets it (up to a minute) for a quieter reading of an idle machine.
SAMPLE_SECONDS = 1.0

# Processes read as groups, by their command name: the rooms, a TLS proxy in
# front of them, and the container machinery around them. runc lives a moment per
# healthcheck exec, so forks per second show it better than its CPU does.
PROCESS_GROUPS = {"plw_room_server": "rooms", "slw_room_server": "rooms", "dockerd": "dockerd", "containerd": "containerd",
                  "containerd-shim": "containerd_shim", "runc": "runc", "haproxy": "haproxy"}


def process_group(comm):
    for prefix, group in PROCESS_GROUPS.items():
        if comm == prefix or (prefix in ("containerd-shim", "runc") and comm.startswith(prefix)):
            return group
    return None


def read(path):
    with open(path, encoding="utf-8") as source:
        return source.read()


def proc_sample():
    """One reading of the counters the rates are taken from."""
    sample = {}
    stat_lines = read("/proc/stat").splitlines()
    fields = stat_lines[0].split()
    sample["cpu"] = [int(v) for v in fields[1:9]]
    sample["cpus"] = []
    for line in stat_lines[1:]:
        parts = line.split()
        if parts and parts[0] in ("ctxt", "processes"):
            sample[parts[0]] = int(parts[1])
        elif parts and parts[0].startswith("cpu"):
            sample["cpus"].append([int(v) for v in parts[1:9]])
    net = {}
    net_each = {}
    for line in read("/proc/softirqs").splitlines():
        parts = line.split()
        if parts and parts[0] in ("NET_RX:", "NET_TX:"):
            net[parts[0][:-1]] = sum(int(v) for v in parts[1:])
            net_each[parts[0][:-1]] = [int(v) for v in parts[1:]]
    sample["softirqs"] = net
    sample["softirqs_each"] = net_each
    # softnet_stat, a hex row per CPU: processed, dropped, time_squeeze, ...,
    # received_rps (column 10). A missing file leaves the per-CPU view without it.
    sample["softnet"] = optional(lambda: [[int(v, 16) for v in line.split()] for line in read("/proc/net/softnet_stat").splitlines()])
    sample["nic_irqs"] = nic_irq_counts()
    sample["nic"] = {}
    for device in nic_devices():
        stats = f"/sys/class/net/{device}/statistics/"
        try:
            sample["nic"][device] = {k: int(read(stats + k)) for k in ("rx_bytes", "tx_bytes", "rx_packets", "tx_packets")}
        except (OSError, ValueError):
            continue
    rooms = {}
    groups = {}
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            group = process_group(read(f"/proc/{pid}/comm").strip())
            if group is None:
                continue
            stat = read(f"/proc/{pid}/stat")
            after = stat[stat.rindex(")") + 2:].split()
            rss_pages = int(read(f"/proc/{pid}/statm").split()[1])
            # utime and stime are fields 14 and 15 of stat, 12 and 13 after the command.
            # processor, field 39 of stat: the CPU the process last ran on.
            entry = {"utime": int(after[11]), "stime": int(after[12]), "rss_kib": rss_pages * os.sysconf("SC_PAGE_SIZE") // 1024,
                     "cpu": int(after[36]) if len(after) > 36 else None}
            if group == "rooms":
                switches = 0
                for line in read(f"/proc/{pid}/status").splitlines():
                    if line.startswith(("voluntary_ctxt_switches", "nonvoluntary_ctxt_switches")):
                        switches += int(line.split()[1])
                entry["switches"] = switches
                rooms[pid] = entry
            groups.setdefault(group, {})[pid] = entry
        except (OSError, ValueError, IndexError):
            continue
    sample["rooms"] = rooms
    sample["groups"] = groups
    return sample


def nic_devices():
    """Interfaces that carry the default route (the rooms use host networking)."""
    devices = []
    for line in read("/proc/net/route").splitlines()[1:]:
        parts = line.split()
        if len(parts) > 1 and parts[1] == "00000000" and parts[0] not in devices:
            devices.append(parts[0])
    return devices


def optional(read_value):
    try:
        return read_value()
    except (OSError, ValueError):
        return None


def nic_queue_irq(line):
    # virtio-net names its queue IRQs virtioN-input.Q / virtioN-output.Q; other virtio devices are disks.
    return re.search(r"virtio\d+-(input|output)\.\d+", line)


def nic_irq_counts():
    """Interrupts each NIC queue IRQ has raised on each CPU."""
    lines = read("/proc/interrupts").splitlines()
    cpus = len(lines[0].split())
    counts = {}
    for line in lines[1:]:
        found = nic_queue_irq(line)
        if found:
            counts[found.group(0)] = [int(v) for v in line.split()[1:1 + cpus]]
    return counts


def nic_channels(device):
    """ethtool -l: the queue count the device offers and the one it runs."""
    channels = {}
    section = None
    for line in command("ethtool", "-l", device).splitlines():
        if line.startswith("Pre-set maximums"):
            section = "max"
        elif line.startswith("Current hardware settings"):
            section = "current"
        elif section and ":" in line:
            key, value = (part.strip() for part in line.split(":", 1))
            if value.isdigit():
                channels[f"{section}_{key.lower()}"] = int(value)
    return channels


def nic_config(device):
    base = f"/sys/class/net/{device}/queues/"
    queues = sorted(os.listdir(base))
    config = {"rx_queues": [q for q in queues if q.startswith("rx-")], "tx_queues": [q for q in queues if q.startswith("tx-")],
              "rps_cpus": {}, "rps_flow_cnt": {}, "xps_cpus": {}}
    # A kernel without RPS/XPS has no such files: the queue says null.
    for queue in config["rx_queues"]:
        config["rps_cpus"][queue] = optional(lambda: read(base + queue + "/rps_cpus").strip())
        config["rps_flow_cnt"][queue] = optional(lambda: int(read(base + queue + "/rps_flow_cnt")))
    for queue in config["tx_queues"]:
        config["xps_cpus"][queue] = optional(lambda: read(base + queue + "/xps_cpus").strip())
    irqs = {}
    for line in read("/proc/interrupts").splitlines()[1:]:
        parts = line.split()
        if parts and parts[0].endswith(":") and parts[0][:-1].isdigit() and (device in line or nic_queue_irq(line)):
            irq = parts[0][:-1]
            irqs[irq] = {"name": parts[-1], "affinity": optional(lambda: read(f"/proc/irq/{irq}/smp_affinity_list").strip())}
    config["irqs"] = irqs
    config["channels"] = optional(lambda: nic_channels(device))
    return config


def system_profile(errors):
    """Host context a load run is compared by: CPU and softirq shares, packet
    rates, queue and steering configuration, versions, room processes."""
    out = {"sample_seconds": SAMPLE_SECONDS}
    try:
        out["kernel"] = os.uname().release
        model = [line.split(":", 1)[1].strip() for line in read("/proc/cpuinfo").splitlines() if line.startswith("model name")]
        out["cpu_model"] = model[0] if model else None
    except Exception as error:
        errors["system_kernel"] = diagnostic_error(error)
    try:
        out["openssl"] = command("openssl", "version").strip()
    except Exception as error:
        errors["system_openssl"] = diagnostic_error(error)
    try:
        out["rps_sock_flow_entries"] = int(read("/proc/sys/net/core/rps_sock_flow_entries"))
        out["nics"] = {device: nic_config(device) for device in nic_devices()}
    except Exception as error:
        errors["system_nic_config"] = diagnostic_error(error)
    try:
        before = proc_sample()
        time.sleep(SAMPLE_SECONDS)
        after = proc_sample()
    except Exception as error:
        errors["system_sample"] = diagnostic_error(error)
        return out
    names = ("user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal")
    grown = [b - a for a, b in zip(before["cpu"], after["cpu"])]
    total = sum(grown) or 1
    out["cpu_pct"] = {name: round(100 * value / total, 2) for name, value in zip(names, grown)}
    out["cpu_pct"]["busy"] = round(100 * (total - grown[3] - grown[4]) / total, 2)
    out["softirqs_per_s"] = {k: round((after["softirqs"].get(k, 0) - v) / SAMPLE_SECONDS) for k, v in before["softirqs"].items()}
    out["nic_rates"] = {device: {k: round((after["nic"][device][k] - v) / SAMPLE_SECONDS) for k, v in counters.items()}
                        for device, counters in before["nic"].items() if device in after["nic"]}
    ticks = os.sysconf("SC_CLK_TCK")
    rooms = []
    for pid, a in before["rooms"].items():
        b = after["rooms"].get(pid)
        if b:
            rooms.append({"pid": int(pid), "cpu": b.get("cpu"), "user_pct": round(100 * (b["utime"] - a["utime"]) / ticks / SAMPLE_SECONDS, 2),
                          "system_pct": round(100 * (b["stime"] - a["stime"]) / ticks / SAMPLE_SECONDS, 2), "rss_kib": b["rss_kib"]})
    out["per_cpu"] = per_cpu(before, after, rooms)
    out["nic_irqs_per_s"] = {name: [round((b - a) / SAMPLE_SECONDS) for a, b in zip(before["nic_irqs"][name], counts)]
                             for name, counts in after["nic_irqs"].items() if name in before["nic_irqs"]}
    switches = sum(after["rooms"][pid].get("switches", 0) - a.get("switches", 0) for pid, a in before["rooms"].items() if pid in after["rooms"])
    out["room_processes"] = {"count": len(rooms), "user_pct": round(sum(r["user_pct"] for r in rooms), 2),
                             "system_pct": round(sum(r["system_pct"] for r in rooms), 2),
                             "rss_kib": sum(r["rss_kib"] for r in rooms),
                             "switches_per_s": round(switches / SAMPLE_SECONDS), "each": rooms}
    out["process_groups"] = {}
    for group, members in after.get("groups", {}).items():
        was = before.get("groups", {}).get(group, {})
        user = sum(m["utime"] - was[pid]["utime"] for pid, m in members.items() if pid in was)
        system = sum(m["stime"] - was[pid]["stime"] for pid, m in members.items() if pid in was)
        out["process_groups"][group] = {"count": len(members), "user_pct": round(100 * user / ticks / SAMPLE_SECONDS, 2),
                                        "system_pct": round(100 * system / ticks / SAMPLE_SECONDS, 2),
                                        "rss_kib": sum(m["rss_kib"] for m in members.values())}
    for counter, name in (("ctxt", "context_switches_per_s"), ("processes", "forks_per_s")):
        if counter in before and counter in after:
            out[name] = round((after[counter] - before[counter]) / SAMPLE_SECONDS, 1)
    return out


def per_cpu(before, after, rooms):
    """Each CPU's time split, network softirqs and backlog, and the rooms last
    seen on it: where receive work and room work actually land."""
    cpus = []
    for index, (a, b) in enumerate(zip(before["cpus"], after["cpus"])):
        grown = [y - x for x, y in zip(a, b)]
        total = sum(grown) or 1
        cpu = {"cpu": index, "busy": round(100 * (total - grown[3] - grown[4]) / total, 2)}
        for name, slot in (("user", 0), ("system", 2), ("irq", 5), ("softirq", 6), ("steal", 7)):
            cpu[name] = round(100 * grown[slot] / total, 2)
        for name, counts in after["softirqs_each"].items():
            was = before["softirqs_each"].get(name, [])
            if index < len(counts) and index < len(was):
                cpu[name.lower() + "_per_s"] = round((counts[index] - was[index]) / SAMPLE_SECONDS)
        if before.get("softnet") and after.get("softnet") and index < len(after["softnet"]):
            x, y = before["softnet"][index], after["softnet"][index]
            cpu["backlog_processed_per_s"] = round((y[0] - x[0]) / SAMPLE_SECONDS)
            cpu["backlog_dropped"] = y[1] - x[1]
            cpu["budget_squeezed"] = y[2] - x[2]
            if len(y) > 9:
                cpu["rps_received_per_s"] = round((y[9] - x[9]) / SAMPLE_SECONDS)
        placed = [room for room in rooms if room.get("cpu") == index]
        cpu["rooms"] = len(placed)
        cpu["rooms_pct"] = round(sum(room["user_pct"] + room["system_pct"] for room in placed), 2)
        cpus.append(cpu)
    return cpus


def collect():
    errors = {}
    try:
        names = command("docker", "ps", "-a", "--format", "{{.Names}}").splitlines()
    except Exception as error:
        names = []
        errors["containers"] = diagnostic_error(error)
    names = [n for n in names if n.startswith(("stand-", "rooms-"))]
    containers = []
    for name in names:
        result = {"name": name, "errors": {}}
        containers.append(result)
        # Inspect only selected fields; container environments contain credentials.
        try:
            state = json.loads(command("docker", "inspect", "--format", "{{json .State}}", name))
            result.update(status=state["Status"], health=state.get("Health", {}).get("Status"),
                          oom_killed=state["OOMKilled"], started=state["StartedAt"])
        except Exception as error:
            result["errors"]["state"] = diagnostic_error(error)
        try:
            result["restarts"] = int(command("docker", "inspect", "--format", "{{.RestartCount}}", name))
        except Exception as error:
            result["errors"]["restarts"] = diagnostic_error(error)
        try:
            logs = subprocess.run(["docker", "logs", "--timestamps", "--since", "1h", "--tail", "5000", name],
                                  capture_output=True, text=True, timeout=25, check=True)
        except Exception as error:
            result["errors"]["logs"] = diagnostic_error(error)
            continue
        lines = (logs.stdout + "\n" + logs.stderr).splitlines()
        messages = collections.Counter()
        periods = []
        room_history = collections.deque(maxlen=120)
        for line in lines:
            stamp = None
            if re.match(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\S* ", line):
                stamp, line = line.split(" ", 1)
            try:
                entry = json.loads(line)
            except (ValueError, TypeError):
                entry = None
            if isinstance(entry, dict):
                if str(entry.get("level", "")).upper() in ("WARN", "WARNING", "ERROR", "FATAL", "PANIC"):
                    messages[str(entry.get("msg", "unknown"))[:256]] += 1
                if entry.get("msg") == "period":
                    periods.append(entry)
            versions = re.search(r'(?:ws_library|lws)="([^"]*)" tls_library="([^"]*)"', line)
            if versions:
                result["room_versions"] = {"ws_library": versions[1], "tls_library": versions[2]}
            if re.match(r"[a-z]+_room_stats ", line):
                fields = r"t|ticks|late|dropped|refused|seated|host_tick_avg_ms|host_tick_max_ms|replication_avg_ms|replication_max_ms"
                counters = {k: float(v) for k, v in re.findall(r"\b(" + fields + r")=(\d+(?:\.\d+)?)\b", line)}
                close = re.search(r"closes=([\w:,]+)", line)
                if close:
                    counters["closes"] = {k: int(v) for k, v in re.findall(r"(\w+):(\d+)", close[1])}
                sources = re.search(r"slow_sources=([\w:,]+)", line)
                if sources:
                    counters["slow_sources"] = {k: int(v) for k, v in re.findall(r"(\w+):(\d+)", sources[1])}
                result["room_cumulative"] = counters
                if stamp:
                    room_history.append({"at": stamp, "seated": int(counters.get("seated", 0)),
                                         "late": int(counters.get("late", 0)), "dropped": int(counters.get("dropped", 0)),
                                         "closes": counters.get("closes", {}), "slow_sources": counters.get("slow_sources", {})})
            if entry is None:
                for label in ("error", "fatal", "panic", "failed", "fenced"):
                    if re.search(r"\b" + label + r"\b", line, re.I):
                        messages["unstructured " + label + " (inspect Docker logs)"] += 1
                        break
        result["log_window"] = "last hour, at most 5000 lines; counts may be truncated"
        if room_history:
            result["room_history"] = list(room_history)
        result["warnings_errors"] = dict(messages)
        if periods:
            result["latest_api_period"] = periods[-1]
            result["api_hour_max_ms"] = {}
            for period in periods:
                for route, value in period.items():
                    if isinstance(value, dict) and "max_ms" in value:
                        result["api_hour_max_ms"][route] = max(result["api_hour_max_ms"].get(route, 0), value["max_ms"])
    memory = {}
    out = {"at_unix": time.time(), "cpu_count": os.cpu_count(), "memory": memory,
           "containers": containers, "container_resources": [], "errors": errors}
    try:
        out["load_average"] = os.getloadavg()
    except Exception as error:
        errors["load_average"] = diagnostic_error(error)
    try:
        with open("/proc/meminfo", encoding="utf-8") as source:
            for line in source:
                key, value = line.split(":", 1)
                if key in ("MemTotal", "MemAvailable", "SwapTotal", "SwapFree"):
                    memory[key + "_kib"] = int(value.split()[0])
    except Exception as error:
        errors["memory"] = diagnostic_error(error)
    try:
        disk = shutil.disk_usage("/")
        out["disk_bytes"] = {"total": disk.total, "used": disk.used, "free": disk.free}
    except Exception as error:
        errors["disk_bytes"] = diagnostic_error(error)
    try:
        out["system"] = system_profile(errors)
    except Exception as error:
        errors["system"] = diagnostic_error(error)
    try:
        stats = command("docker", "stats", "--no-stream", "--format", "{{json .}}", *names) if names else ""
        out["container_resources"] = [json.loads(line) for line in stats.splitlines() if line]
    except Exception as error:
        errors["container_resources"] = diagnostic_error(error)
    return out


if __name__ == "__main__":
    if len(sys.argv) > 1:
        SAMPLE_SECONDS = min(60.0, max(1.0, float(sys.argv[1])))
    try:
        print(json.dumps(collect()))
    except Exception as error:
        print(json.dumps({"error": type(error).__name__ + ": host diagnostics failed; check SSH and Docker"}))
        raise SystemExit(1)
