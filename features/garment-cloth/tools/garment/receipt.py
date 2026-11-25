"""The check receipt of an outfit (`<script>.check.json`, beside the outfit's declaration): which gates `check.py` ran
on the built sidecar, what each found, and the content hash of everything those gates read.

Validation runs once, at authoring time: the gates over every clip take minutes and gigabytes, and their answer is a
function of their inputs. The cook therefore does not run the per-frame gates; it accepts a receipt instead (`accept`)
and re-runs only the gates that need no clip (`gates.STATIC`) on what it is about to ship, so a shipped outfit's
structural validity never rests on a recorded finding.

A receipt certifies exactly what the gates read. `check.py` snapshots the hash of every file and module before its first
gate and again before it writes, and writes no receipt when they differ or when the build inputs on disk differ from
the hashes the fresh sidecar recorded during its own build. `verify` recomputes the set of files and modules a check
of the declared outfit reads (from the declaration and the committed sidecar) and requires the receipt to name exactly
that set, each at its current hash; it compares the receipt's script, sidecar and stage with the declaration's, and its
digest with its content, so a hand-edited receipt is refused. A receipt that records a failure of a gate enforced at
the declared stage is refused by the rule `check.py`'s exit code applies (`gates.blocking`).

What a check reads (`files`, relative to the game root, and `modules`, relative to the Studio root): the outfit's
script and declaration, its committed sidecar, every file the build read (`extras.build.inputs`), the model and the
declared body, the files the gates read (`extras.check`: colliders, clip manifest and every clip the game can show the
outfit in, `shown`; looks), the Python
modules the script imports (`extras.build.modules`) and those of `check.py`; `versions` holds the Python, numpy and scipy
versions and `library` the feature version. Text files are hashed with LF line ends (garment.build.sha256)."""
import hashlib
import json
from pathlib import Path

from . import build as Bd
from . import gates as G
from . import model as M

SCHEMA = "garment.check.v2"
CHECK = Path(__file__).resolve().parent / "check.py"
MISSING = "missing"


def path_for(script):
    """The receipt of an outfit script: `content/garments/<outfit>.check.json` for `content/garments/<outfit>.py` (or
    for its declaration `<outfit>.json`)."""
    return Path(script).with_suffix(".check.json")


def declaration_for(script):
    """The outfit's declaration beside its script: `<outfit>.json`."""
    return Path(script).with_suffix(".json")


def _files(root, script, sidecar_path, sc):
    """The files a check of one built sidecar reads, as resolved paths."""
    root = Path(root).resolve()
    names = {Path(script).resolve(), Path(sidecar_path).resolve(), declaration_for(script).resolve()}
    names |= {root / k for k in sc.build.get("inputs", {})}
    names.add(root / sc.build["model"])
    if isinstance(sc.build.get("body"), str):
        names.add(root / sc.build["body"])
    check = sc.check
    if check.get("colliders"):
        names.add(root / check["colliders"])
    if check.get("clips"):
        names.add(root / check["clips"])
        names |= {Path(p).resolve() for _, p in M.shown(root / check["clips"], check["shown"])}
    names |= {root / p for p in check.get("looks", [])}
    return {p.resolve() for p in names}


def _modules(root, sc):
    """The Python a check runs: the modules the script imports (as its build recorded them) and check.py's own."""
    return set(sc.build.get("modules", {})) | set(Bd.module_hashes(CHECK, root)) | {Bd.studio_path(CHECK)}


def _hash(path):
    return Bd.sha256(path) if Path(path).is_file() else MISSING


def snapshot(root, script, sidecar_path, sc):
    """{"files": {path relative to root: sha256}, "modules": {Studio-relative path: sha256}} of everything a check of
    the built sidecar `sc` reads, at this moment."""
    root = Path(root).resolve()
    files = {p.relative_to(root).as_posix(): _hash(p) for p in _files(root, script, sidecar_path, sc)}
    modules = {}
    for name in _modules(root, sc):
        modules[name] = _hash(Path(name) if Path(name).is_absolute() else Bd.STUDIO / name)
    return {"files": dict(sorted(files.items())), "modules": dict(sorted(modules.items()))}


def differences(before, after):
    """The entries of two snapshots that differ, each named."""
    out = []
    for group in ("files", "modules"):
        a, b = before[group], after[group]
        out += [f"{group[:-1]} {k}" for k in sorted(set(a) | set(b)) if a.get(k) != b.get(k)]
    return out


def build_differences(sc, snap, root, script):
    """Where the files on disk (`snap`) differ from the hashes the built sidecar `sc` recorded during its own build:
    the script, every build input and every module the script imports."""
    root = Path(root).resolve()
    out = []
    if sc.build.get("script_sha256") != snap["files"].get(Path(script).resolve().relative_to(root).as_posix()):
        out.append(f"script {Path(script).name}")
    out += [f"input {k}" for k, v in sc.build.get("inputs", {}).items() if snap["files"].get(k) != v]
    out += [f"module {k}" for k, v in sc.build.get("modules", {}).items() if snap["modules"].get(k) != v]
    return out


def _digest(doc):
    body = {k: v for k, v in doc.items() if k != "digest"}
    return hashlib.sha256(json.dumps(body, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
                                     allow_nan=False).encode("utf-8")).hexdigest()


def write(path, root, script, sidecar_path, stage, gates_run, findings, snap):
    """Write the receipt of one complete check whose gates read the files and modules of `snap` (taken before the first
    gate and found unchanged after the last); the same check gives the same bytes."""
    root = Path(root).resolve()
    doc = {"schema": SCHEMA, "script": Path(script).resolve().relative_to(root).as_posix(),
           "declaration": declaration_for(script).resolve().relative_to(root).as_posix(),
           "sidecar": Path(sidecar_path).resolve().relative_to(root).as_posix(), "stage": int(stage),
           "gates_run": sorted(gates_run), "library": Bd.LIBRARY_VERSION, "versions": Bd.versions(),
           "files": snap["files"], "modules": snap["modules"], "findings": _relative(findings, root)}
    doc = json.loads(json.dumps(doc, ensure_ascii=False, allow_nan=False, default=_plain))
    doc["digest"] = _digest(doc)
    text = json.dumps(doc, indent=1, ensure_ascii=False, allow_nan=False) + "\n"
    Path(path).write_bytes(text.encode("utf-8"))
    return Path(path)


def _relative(value, root):
    """The findings with every path under the game root written relative to it, so a receipt does not depend on where
    the checkout lies."""
    if isinstance(value, dict):
        return {k: _relative(v, root) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_relative(v, root) for v in value]
    if isinstance(value, str) and Path(value).is_absolute() and Path(value).resolve().is_relative_to(root):
        return Path(value).resolve().relative_to(root).as_posix()
    return value


def _plain(value):
    import numpy as np
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, np.ndarray):
        return value.tolist()
    raise TypeError(f"{type(value).__name__} is not JSON")


def _declared(root, declaration):
    declaration = Path(declaration)
    decl = json.loads(declaration.read_text(encoding="utf-8"))
    missing = [k for k in ("script", "stage", "sidecar") if k not in decl]
    if missing:
        raise ValueError(f"{declaration.name}: the declaration names no {', '.join(missing)}")
    return decl


def verify(root, declaration):
    """(findings, read sidecar) of the receipt of the outfit `declaration` declares, if the cook may trust it;
    ValueError naming why not: missing, another schema, a digest unlike its content, another script, declaration,
    sidecar or stage than declared, a gate the stage enforces that did not run, another set of files or modules than
    a check of that outfit reads, a changed file, module, library or version, or a blocking finding."""
    from .sidecar import read
    root, declaration = Path(root).resolve(), Path(declaration).resolve()
    decl = _declared(root, declaration)
    path = path_for(declaration)
    if not path.is_file():
        raise ValueError(f"no check receipt {path.name}: run check.py on the outfit's script to write it")
    doc = json.loads(path.read_text(encoding="utf-8"))
    if doc.get("schema") != SCHEMA:
        raise ValueError(f"{path.name}: not a {SCHEMA} receipt")
    if doc.get("digest") != _digest(doc):
        raise ValueError(f"{path.name}: its content is not what check.py wrote (the digest differs): run check.py again")
    expected = {"script": decl["script"], "declaration": declaration.relative_to(root).as_posix(),
                "sidecar": decl["sidecar"], "stage": decl["stage"]}
    for key, value in expected.items():
        if doc[key] != value:
            raise ValueError(f"{path.name}: checked {key} {doc[key]}, the declaration ships {value}")
    stage = int(decl["stage"])
    missing = ({"rebuild"} | G.enforced_gates(stage)) - set(doc["gates_run"])
    if missing:
        raise ValueError(f"{path.name}: the check did not run {sorted(missing)}, which stage {stage} enforces")
    sidecar_path = root / decl["sidecar"]
    if doc["files"].get(decl["sidecar"]) != _hash(sidecar_path):
        # The set of files a check reads comes from the sidecar, so a sidecar other than the checked one decides it.
        raise ValueError(f"{path.name} is stale (file {decl['sidecar']}): run check.py on {doc['script']} again")
    sc = read(sidecar_path.read_bytes())
    now = snapshot(root, root / decl["script"], sidecar_path, sc)
    changed = []
    for group in ("files", "modules"):
        have, recorded = now[group], doc[group]
        changed += [f"{group[:-1]} {k} not checked" for k in sorted(set(have) - set(recorded))]
        changed += [f"{group[:-1]} {k} not read by this outfit's check" for k in sorted(set(recorded) - set(have))]
        changed += [f"{group[:-1]} {k}" for k in sorted(set(have) & set(recorded)) if have[k] != recorded[k]]
    if doc["library"] != Bd.LIBRARY_VERSION:
        changed.append(f"library {doc['library']} -> {Bd.LIBRARY_VERSION}")
    current = Bd.versions()
    changed += [f"version {k} {doc['versions'].get(k)} -> {current.get(k)}"
                for k in sorted(set(doc["versions"]) | set(current)) if doc["versions"].get(k) != current.get(k)]
    if changed:
        raise ValueError(f"{path.name} is stale ({'; '.join(changed)}): run check.py on {doc['script']} again")
    blocking = G.blocking(doc["findings"], stage)
    if blocking:
        raise ValueError(f"{path.name} records enforced failures: " + _named(blocking))
    return doc["findings"], sc


def _named(findings):
    return "; ".join(f"{f['gate']} {f.get('piece')} "
                     f"{f.get('not_run') or f.get('undecided') or f.get('reason') or f.get('counts') or f.get('worst')}"
                     for f in findings)


def accept(root, declaration, model=None):
    """The findings the cook ships an outfit on: its receipt verified (`verify`), with the gates that need no clip run
    again on the committed sidecar in place of the receipt's; ValueError when the receipt is refused or a re-run gate
    blocks. model: the outfit's RenderModel when the caller has it loaded."""
    root = Path(root).resolve()
    findings, sc = verify(root, declaration)
    stage = int(_declared(root, declaration)["stage"])
    model = model or M.RenderModel(root / sc.build["model"])
    fresh = G.static_findings(sc, model, [root / p for p in sc.check.get("looks", [])], sc.check["fabrics"], stage)
    for f in fresh:
        if f["gate"] == "G9":
            f["piece"] = Path(f["piece"]).resolve().relative_to(root).as_posix()
    blocking = G.blocking(fresh, stage)
    if blocking:
        raise ValueError(f"{Path(declaration).name}: the sidecar fails a gate the cook runs itself: " + _named(blocking))
    return [f for f in findings if f["gate"] not in G.STATIC] + fresh
