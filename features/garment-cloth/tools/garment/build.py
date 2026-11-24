"""The build record of a sidecar and the asset record written beside every file the library builds.

A sidecar is current only while everything that made it is unchanged, so its record hashes by content: the script,
every file the build read (model, fabrics, body, clip manifest and clips, the colliders and seats a bake used), every
Python module the script imports under the game root or the feature (found from the import statements, so the set
does not depend on what else the process loaded), the shared GLB reader, the numpy and scipy versions and, per baked
corrective, the runtime sources. `differences` names each input that no longer matches."""
import ast
import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np
import scipy

PACKAGE = Path(__file__).resolve().parent
FEATURE = PACKAGE.parents[1]
STUDIO = FEATURE.parents[1]
LIBRARY_VERSION = json.loads((FEATURE / "feature.json").read_text(encoding="utf-8"))["version"]


# Text a checkout may store with either line ending (git normalises it): hashed as LF, so a CRLF working copy of the
# same content is the same input.
TEXT = {".py", ".json", ".c", ".h", ".md", ".txt"}


def sha256(path):
    data = Path(path).read_bytes()
    if Path(path).suffix.lower() in TEXT:
        data = data.replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def versions():
    return {"numpy": np.__version__, "scipy": scipy.__version__}


def _package(path):
    """The dotted package of a module file, from the __init__.py files above it."""
    parts, folder = [], path.parent
    while (folder / "__init__.py").is_file():
        parts.insert(0, folder.name)
        folder = folder.parent
    return ".".join(parts)


def _spec_origin(name):
    try:
        spec = importlib.util.find_spec(name)
    except (ImportError, ValueError, AttributeError):
        return None
    return Path(spec.origin).resolve() if spec is not None and spec.origin and spec.has_location else None


def imported_modules(script, roots):
    """The Python files under `roots` that `script` imports, directly or through them, by reading their import
    statements (function-level imports included) against the current sys.path."""
    roots = [Path(r).resolve() for r in roots]
    seen, queue = set(), [Path(script).resolve()]
    while queue:
        path = queue.pop()
        if path in seen:
            continue
        seen.add(path)
        package = _package(path)
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"), str(path))):
            names = []
            if isinstance(node, ast.Import):
                names = [a.name for a in node.names]
            elif isinstance(node, ast.ImportFrom):
                if node.level:
                    base = package.split(".") if package else []
                    base = base[:len(base) - (node.level - 1)] if node.level > 1 else base
                    base = ".".join(base + ([node.module] if node.module else []))
                else:
                    base = node.module
                names = [base] + [f"{base}.{a.name}" for a in node.names]
            for name in names:
                origin = _spec_origin(name) if name else None
                if origin and origin.suffix == ".py" and any(origin.is_relative_to(r) for r in roots):
                    queue.append(origin)
    seen.discard(Path(script).resolve())
    return sorted(seen)


def module_hashes(script, game_root):
    """{path: sha256} of the modules the script imports and the shared GLB reader the library loads by path; a path is
    relative to the Studio root (absolute for a game outside it)."""
    from .model import SHARED_GLB
    files = imported_modules(script, [game_root, FEATURE]) + [SHARED_GLB.resolve()]
    return {studio_path(f): sha256(f) for f in sorted(set(files))}


def differences(recorded, current):
    """The inputs of two build records that differ, each named ("input assets/x.glb", "module ...", "numpy 2.4.4 ->
    2.4.6", ...); [] when they agree."""
    out = []
    for key in ("script_sha256", "library", "stages", "body"):
        if recorded.get(key) != current.get(key):
            out.append(f"{key} {recorded.get(key)} -> {current.get(key)}")
    for group, label in (("inputs", "input"), ("modules", "module"), ("versions", "version")):
        a, b = recorded.get(group, {}), current.get(group, {})
        for name in sorted(set(a) | set(b)):
            if a.get(name) != b.get(name):
                out.append(f"{label} {name}" + (f" {a.get(name)} -> {b.get(name)}" if group == "versions" else ""))
    return out


def write_record(asset, root, what, producer, rebuild, **extra):
    """Write `<asset without its last suffix>.provenance.json` for a file this library built: the format of the game's
    asset records (license, origin, integrity, provenance), every hash taken from the file itself."""
    asset, root = Path(asset), Path(root)
    data = asset.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    relative = asset.resolve().relative_to(root.resolve()).as_posix()
    stem = asset.name[:-len(asset.suffix)] if asset.suffix else asset.name
    record = {"id": stem, "file": relative, "origin": "mine",
              "license": "Studio-made by script from the inputs its build lists; adds no content beyond them",
              "sha256": digest, "bytes": len(data), "integrity": {"sha256": digest, "bytes": len(data)},
              "what": what, "producer": producer, **extra, "rebuild": rebuild}
    path = asset.with_name(stem + ".provenance.json")
    text = json.dumps(record, indent=1, ensure_ascii=False) + "\n"
    if not path.is_file() or path.read_bytes() != text.encode("utf-8"):
        path.write_bytes(text.encode("utf-8"))
    return path


def studio_path(path):
    path = Path(path).resolve()
    return path.relative_to(STUDIO).as_posix() if path.is_relative_to(STUDIO) else path.as_posix()


