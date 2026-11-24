"""The build record of a sidecar and the asset record written beside every file the library builds.

A sidecar is current only while everything that made it is unchanged, so its record hashes by content: the script,
every file the build read (model, fabrics, body, clip manifest and clips, the colliders and seats a bake used), every
Python module the script imports under the game root or the feature (found from the import statements, so the set
does not depend on what else the process loaded), the shared GLB reader, the Python, numpy and scipy versions and, per
baked corrective, the runtime sources. `differences` names each input that no longer matches. A built file's asset
record lists every source that fed it (`derived_sources`), and the build refuses a file derived from an account-gated or
purchased export that the project owner's grant (`GRANT`) does not list."""
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
    """The interpreter and the numeric packages every build and check computes with."""
    import platform
    return {"numpy": np.__version__, "scipy": scipy.__version__, "python": platform.python_version()}


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


def record_path(asset):
    """`<asset without its last suffix>.provenance.json`, the asset record beside a file."""
    asset = Path(asset)
    stem = asset.name[:-len(asset.suffix)] if asset.suffix else asset.name
    return asset.with_name(stem + ".provenance.json")


def _manifest_row(source):
    """(manifest, row) of the folder manifest (`manifest.json`, `*.manifest.json`) whose row names the file by its
    `file` or `resource` path, as clip and environment manifests record their assets; None when none does."""
    for manifest in sorted(source.parent.glob("*manifest.json")):
        try:
            doc = json.loads(manifest.read_text(encoding="utf-8"))
        except ValueError:
            continue
        for rows in (doc.values() if isinstance(doc, dict) else ()):
            for row in (rows if isinstance(rows, list) else ()):
                if not isinstance(row, dict):
                    continue
                named = [str(row.get(k, "")).replace("\\", "/") for k in ("file", "resource")]
                if any(n and source.as_posix().endswith("/" + n.lstrip("./")) for n in named):
                    return manifest, row
    return None


def source_record(source):
    """(path, record) of a source file's asset record: `<stem>.provenance.json` beside it, the folder's
    `provenance.json` whose `model` or `file` names it, or the row of a folder manifest that names it (its record is
    then that row). Refused when there is none or it lacks origin or license: a file built from it inherits them, and
    nothing else says what it may hold."""
    return _record_of(source)[:2]


def _record_of(source):
    """(path, record, manifest row name or None) of source_record."""
    source = Path(source).resolve()
    own = record_path(source)
    candidates = [own] if own.is_file() else []
    folder = source.with_name("provenance.json")
    if not candidates and folder.is_file():
        doc = json.loads(folder.read_text(encoding="utf-8"))
        named = [str(doc.get(k, "")).replace("\\", "/") for k in ("model", "file")]
        if any(n and source.as_posix().endswith("/" + n.lstrip("./")) for n in named):
            candidates = [folder]
    row_name = None
    if candidates:
        path, record = candidates[0], json.loads(candidates[0].read_text(encoding="utf-8"))
    elif (row := _manifest_row(source)) is not None:
        path, record = row
        row_name = record.get("name")
    else:
        raise ValueError(f"{source}: the source has no asset record ({own.name}, a provenance.json or a manifest row "
                         "naming it); a file built from it inherits its origin and license")
    missing = [k for k in ("origin", "license") if not record.get(k)]
    if missing:
        raise ValueError(f"{path}: the record has no {', '.join(missing)} for the files built from {source.name} to "
                         "inherit")
    return path, record, row_name


# The project owner's grant of account-gated and purchased exports a game repository tracks (AGENTS.md).
GRANT = "assets/PURCHASED_EXPORTS.md"


def granted(root, path):
    """Whether the grant lists a file: its root-relative path in backticks, or its name in backticks in a section
    (between `##` headings) that names its folder in backticks, as the grant's tables do."""
    grant = Path(root) / GRANT
    if not grant.is_file():
        return False
    import re
    path, root = Path(path).resolve(), Path(root).resolve()
    if not path.is_relative_to(root):
        return False
    relative = path.relative_to(root).as_posix()
    folder, name = relative.rsplit("/", 1) if "/" in relative else ("", relative)
    for section in re.split(r"^## ", grant.read_text(encoding="utf-8"), flags=re.M):
        tokens = set(re.findall(r"`([^`\n]+)`", section))
        if relative in tokens or (name in tokens and folder + "/" in tokens):
            return True
    return False


def derived_sources(root, sources, derived):
    """The `sources` entries of the record of `derived`, a file built from every file of `sources` (paths under root):
    each one's file, hash, record and the origin and license that record gives, and whether the grant lists it as an
    account-gated or purchased export. A derived file of such a source is such an export too: unless the grant lists
    it as well, it is refused (ValueError naming the sources), and the project owner, not the build, adds it there."""
    root = Path(root).resolve()

    def relative(p):
        p = Path(p).resolve()
        return p.relative_to(root).as_posix() if p.is_relative_to(root) else p.as_posix()
    out, purchased = [], []
    for source in dict.fromkeys(Path(s).resolve() for s in sources):
        record_file, record, row = _record_of(source)
        entry = {"file": relative(source), "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                 "record": relative(record_file), "origin": record["origin"], "license": record["license"],
                 "purchased": granted(root, source)}
        if row is not None:
            entry["row"] = row
        out.append(entry)
        if entry["purchased"]:
            purchased.append(entry["file"])
    if purchased and not granted(root, derived):
        raise ValueError(f"{relative(derived)} is derived from account-gated or "
                         f"purchased exports ({', '.join(purchased)}), so it is one too: it needs its own entry in "
                         f"{GRANT}, the project owner's grant, before the build writes it")
    return out


def write_record(asset, root, what, producer, rebuild, source, sources=None, **extra):
    """Write the asset record of a file this library built from `source` (the model it is derived from), in the format
    of the game's asset records (license, origin, integrity, provenance), every hash taken from the files themselves.
    A derived file carries what its source carries: origin and license come from the source's record (source_record),
    which the record names; `sources` lists every file that fed it (derived_sources; default: the source alone)."""
    asset, root, source = Path(asset), Path(root), Path(source)
    parent_path, parent = source_record(source)
    if sources is None:
        sources = derived_sources(root, [source], asset)
    data = asset.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    def relative(p):
        p = p.resolve()
        return p.relative_to(root.resolve()).as_posix() if p.is_relative_to(root.resolve()) else p.as_posix()
    stem = asset.name[:-len(asset.suffix)] if asset.suffix else asset.name
    record = {"id": stem, "file": relative(asset), "origin": parent["origin"], "license": parent["license"],
              "sha256": digest, "bytes": len(data), "integrity": {"sha256": digest, "bytes": len(data)},
              "what": what, "producer": producer, "source": relative(source),
              "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "source_record": relative(parent_path),
              "sources": list(sources), **extra, "rebuild": rebuild}
    path = record_path(asset)
    text = json.dumps(record, indent=1, ensure_ascii=False) + "\n"
    if not path.is_file() or path.read_bytes() != text.encode("utf-8"):
        path.write_bytes(text.encode("utf-8"))
    return path


def studio_path(path):
    path = Path(path).resolve()
    return path.relative_to(STUDIO).as_posix() if path.is_relative_to(STUDIO) else path.as_posix()


