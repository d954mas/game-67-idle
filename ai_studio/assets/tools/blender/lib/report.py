"""Shared finding/report shape and CLI behaviour for the check tools.

Checks advise by default: they print findings with a fix hint and exit 0. `--strict` turns them
into a gate (exit 1 when a finding at or above `--fail-on` severity exists) for whoever wants one.
"""
import argparse
import json
import sys
from pathlib import Path

SEVERITIES = ("info", "low", "medium", "high")


def finding(check, severity, message, hint="", where=None, value=None, limit=None):
    if severity not in SEVERITIES:
        raise ValueError(f"unknown severity {severity!r}")
    out = {"check": check, "severity": severity, "message": message}
    if where:
        out["where"] = where
    if value is not None:
        out["value"] = value
    if limit is not None:
        out["limit"] = limit
    if hint:
        out["hint"] = hint
    return out


def make_report(tool, asset, findings, metrics=None, options=None, sha256=None):
    counts = {s: sum(1 for f in findings if f["severity"] == s) for s in SEVERITIES}
    ordered = sorted(findings, key=lambda f: -SEVERITIES.index(f["severity"]))
    report = {"tool": tool, "asset": str(asset).replace("\\", "/"), "status":
              "findings" if any(counts[s] for s in ("low", "medium", "high")) else "clean",
              "counts": counts, "metrics": metrics or {}, "findings": ordered}
    if sha256:
        report["sha256"] = sha256
    if options:
        report["options"] = options
    return report


def guarded(findings, section, fn, *args, **kwargs):
    """Run one check section; a malformed file becomes a finding instead of a traceback, so a batch
    over many models keeps going. Returns the section's result, or None when it failed."""
    try:
        return fn(*args, **kwargs)
    except Exception as error:  # noqa: BLE001 - any failure is reported with its type
        findings.append(finding("check_error", "high", f"{section} could not run: {type(error).__name__}: {error}",
                                "re-export the file; if it is valid glTF, this is a tool bug to report"))
        return None


def unreadable(tool, asset, error):
    """Report for a file the check could not open."""
    return make_report(tool, asset if isinstance(asset, (str, Path)) else getattr(asset, "label", "glb"), [
        finding("unreadable", "high", f"cannot read the file: {type(error).__name__}: {error}",
                "re-export as plain binary glTF 2.0 (no Draco/meshopt compression)")])


def worst(report):
    present = [f["severity"] for f in report["findings"]]
    return max(present, key=SEVERITIES.index) if present else None


def fails(report, fail_on="high"):
    level = SEVERITIES.index(fail_on)
    return any(SEVERITIES.index(f["severity"]) >= level for f in report["findings"])


def add_common_arguments(parser):
    parser.add_argument("--json", metavar="PATH", help="also write the report as JSON")
    parser.add_argument("--config", metavar="PATH", help="JSON file of option overrides")
    parser.add_argument("--strict", action="store_true", help="exit 1 when findings reach --fail-on")
    parser.add_argument("--fail-on", choices=SEVERITIES[1:], default="high")
    parser.add_argument("--quiet", action="store_true", help="print only the summary line")
    return parser


def resolve_options(tool, defaults, config=None, overrides=None):
    """Defaults < config (flat, or nested under the tool's name) < explicit options that are not None.
    Unknown keys are refused: a mistyped threshold must not silently fall back to its default."""
    config = config or {}
    if isinstance(config.get(tool), dict):
        config = config[tool]
    elif any(key.endswith("_check") for key in config):
        config = {}
    merged = dict(defaults)
    for source in (config, overrides or {}):
        for key, value in source.items():
            if key not in defaults:
                raise ValueError(f"{tool}: unknown option {key!r}; known: {sorted(defaults)}")
            if value is not None:
                merged[key] = value
    return merged


def cli_overrides(args, defaults):
    """The tool options a parsed command line set explicitly (flags default to None)."""
    return {k: getattr(args, k) for k in defaults if getattr(args, k, None) is not None}


def load_config(path):
    return json.loads(Path(path).read_text(encoding="utf-8")) if path else {}


def format_report(report, quiet=False):
    counts = report["counts"]
    head = (f"{report['tool']}: {report['asset']}: {report['status']}"
            f" (high {counts['high']}, medium {counts['medium']}, low {counts['low']})")
    if quiet:
        return head
    lines = [head]
    for key, value in report.get("metrics", {}).items():
        if not isinstance(value, (dict, list)):
            lines.append(f"  {key}: {value}")
    for f in report["findings"]:
        where = f" @ {json.dumps(f['where'], ensure_ascii=False)}" if f.get("where") else ""
        lines.append(f"  [{f['severity']}] {f['check']}: {f['message']}{where}")
        if f.get("hint"):
            lines.append(f"      fix: {f['hint']}")
    return "\n".join(lines)


def emit(reports, args):
    """Print, optionally write JSON, and return the process exit code."""
    reports = reports if isinstance(reports, list) else [reports]
    for report in reports:
        print(format_report(report, args.quiet))
    if args.json:
        path = Path(args.json)
        path.parent.mkdir(parents=True, exist_ok=True)
        payload = reports[0] if len(reports) == 1 else reports
        path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.strict and any(fails(r, args.fail_on) for r in reports):
        return 1
    return 0


def cli(tool_doc, configure, run):
    """Build a check CLI: `configure(parser)` adds tool flags, `run(args, config)` returns reports."""
    parser = add_common_arguments(argparse.ArgumentParser(description=tool_doc.strip().splitlines()[0]))
    configure(parser)

    def main(argv=None):
        args = parser.parse_args(argv)
        return emit(run(args, load_config(args.config)), args)

    return main


if __name__ == "__main__":
    sys.exit("report.py is a library")
