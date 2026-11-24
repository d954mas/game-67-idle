"""The check receipt of an outfit (`<script>.check.json`, beside the outfit's declaration): which gates `check.py` ran
on the built sidecar, what each found, and the content hash of everything those gates read.

Validation runs once, at authoring time: the gates over every clip take minutes and gigabytes, and their answer is a
function of their inputs. The cook therefore runs no gate. It verifies the receipt instead: a missing one is refused,
a stale one is refused naming the input that changed, and one that records a finding of a gate enforced at the
declared stage is refused, by the rule `check.py`'s exit code applies (`gates.blocking`, from `gates.ENFORCED_FROM`).

What a receipt hashes (`files`, relative to the game root, and `modules`, relative to the Studio root): the outfit's
script, its committed sidecar, every file the build read (`extras.build.inputs`: model, fabrics, body, clip manifest
and clips, seats, colliders), the files the gates read (`extras.check`: colliders, clip manifest and every clip,
looks; the body and model of `extras.build`), and the Python modules of the script and of `check.py` under the game
root and the feature (with the shared GLB reader); `versions` holds the numpy and scipy versions and `library` the
feature version. Text files are hashed with LF line ends (garment.build.sha256)."""
import json
from pathlib import Path

from . import build as Bd
from . import gates as G
from . import model as M

SCHEMA = "garment.check.v1"
CHECK = Path(__file__).resolve().parent / "check.py"


def path_for(script):
    """The receipt of an outfit script: `content/garments/<outfit>.check.json` for `content/garments/<outfit>.py`."""
    return Path(script).with_suffix(".check.json")


def files_read(root, script, sidecar_path, sc):
    """{path relative to root: sha256} of every file the build and the gates of one sidecar read."""
    root = Path(root).resolve()
    names = {Path(script).resolve(), Path(sidecar_path).resolve()}
    names |= {root / k for k in sc.build.get("inputs", {})}
    names.add(root / sc.build["model"])
    if isinstance(sc.build.get("body"), str):
        names.add(root / sc.build["body"])
    check = sc.check
    if check.get("colliders"):
        names.add(root / check["colliders"])
    if check.get("clips"):
        names.add(root / check["clips"])
        names |= {Path(p).resolve() for _, p in M.clips(root / check["clips"])}
    names |= {root / p for p in check.get("looks", [])}
    return {p.resolve().relative_to(root).as_posix(): Bd.sha256(p) for p in sorted(names)}


def modules(root, script):
    """{Studio-relative path: sha256} of the Python the script and check.py run."""
    return dict(sorted({**Bd.module_hashes(script, root), **Bd.module_hashes(CHECK, root),
                        Bd.studio_path(CHECK): Bd.sha256(CHECK)}.items()))


def write(path, root, script, sidecar_path, sc, stage, gates_run, findings):
    """Write the receipt of one complete check; the same check gives the same bytes."""
    root = Path(root).resolve()
    doc = {"schema": SCHEMA, "script": Path(script).resolve().relative_to(root).as_posix(),
           "sidecar": Path(sidecar_path).resolve().relative_to(root).as_posix(), "stage": int(stage),
           "gates_run": sorted(gates_run), "library": Bd.LIBRARY_VERSION, "versions": Bd.versions(),
           "files": files_read(root, script, sidecar_path, sc), "modules": modules(root, script),
           "findings": _relative(findings, root)}
    text = json.dumps(doc, indent=1, ensure_ascii=False, allow_nan=False, default=_plain) + "\n"
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


def verify(path, root, stage, sidecar):
    """The findings of a receipt the cook may trust for an outfit declared at `stage` whose sidecar is `sidecar`
    (relative to root); ValueError naming why not: missing, another schema, stage or sidecar, a gate the stage
    enforces that did not run, a changed input, or a blocking finding."""
    path, root = Path(path), Path(root).resolve()
    if not path.is_file():
        raise ValueError(f"no check receipt {path.name}: run check.py on the outfit's script to write it")
    doc = json.loads(path.read_text(encoding="utf-8"))
    if doc.get("schema") != SCHEMA:
        raise ValueError(f"{path.name}: not a {SCHEMA} receipt")
    if doc["stage"] != stage:
        raise ValueError(f"{path.name}: checked at stage {doc['stage']}, the declaration ships stage {stage}")
    if doc["sidecar"] != Path(sidecar).as_posix():
        raise ValueError(f"{path.name}: checked {doc['sidecar']}, the declaration ships {sidecar}")
    missing = ({"rebuild"} | G.enforced_gates(stage)) - set(doc["gates_run"])
    if missing:
        raise ValueError(f"{path.name}: the check did not run {sorted(missing)}, which stage {stage} enforces")
    changed = []
    for name, digest in doc["files"].items():
        if not (root / name).is_file() or Bd.sha256(root / name) != digest:
            changed.append(f"file {name}")
    for name, digest in doc["modules"].items():
        file = Path(name) if Path(name).is_absolute() else Bd.STUDIO / name
        if not file.is_file() or Bd.sha256(file) != digest:
            changed.append(f"module {name}")
    if doc["library"] != Bd.LIBRARY_VERSION:
        changed.append(f"library {doc['library']} -> {Bd.LIBRARY_VERSION}")
    current = Bd.versions()
    changed += [f"version {k} {v} -> {current.get(k)}" for k, v in doc["versions"].items() if current.get(k) != v]
    if changed:
        raise ValueError(f"{path.name} is stale ({'; '.join(changed)}): run check.py on {doc['script']} again")
    blocking = G.blocking(doc["findings"], stage)
    if blocking:
        raise ValueError(f"{path.name} records enforced failures: " + "; ".join(
            f"{f['gate']} {f.get('piece')} {f.get('not_run') or f.get('undecided') or f.get('reason') or f.get('counts') or f.get('worst')}"
            for f in blocking))
    return doc["findings"]
