#!/usr/bin/env python3
"""Fresh-process deterministic Lua evaluator for Items authoring."""

from __future__ import annotations

import argparse
import importlib.metadata
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any


SCHEMA = "items.lua.sandbox.v1"
BACKEND_MODULE = "lupa.lua54"
DEFAULT_TIMEOUT_MS = 2_000
DEFAULT_MEMORY_BYTES = 16 * 1024 * 1024


PRELUDE = r'''
return function()
  local raw_pairs, raw_type = pairs, type
  local declarations = {}

  local function copy(value, seen)
    if raw_type(value) ~= "table" then return value end
    seen = seen or {}
    if seen[value] then error("cyclic declaration table", 0) end
    seen[value] = true
    local out = {}
    for key, child in raw_pairs(value) do out[copy(key, seen)] = copy(child, seen) end
    seen[value] = nil
    return out
  end

  local function tagged(kind, value)
    value.__studio_kind = kind
    return value
  end

  local items = {}
  function items.ref(id) return tagged("item_ref", { id = id }) end
  function items.cost(item, count) return tagged("cost", { item = item, count = count }) end
  function items.costs(entries) return tagged("costs", { entries = entries }) end
  function items.free() return tagged("free", {}) end
  function items.define(definition)
    declarations[#declarations + 1] = copy(definition)
  end

  local levels = {}
  function levels.single(row)
    return tagged("levels", { mode = "single", rows = { [1] = row } })
  end
  function levels.table(rows)
    return tagged("levels", { mode = "table", rows = rows })
  end

  local function freeze(value, seen)
    if raw_type(value) ~= "table" then return value end
    seen = seen or {}
    if seen[value] then return seen[value] end
    local target, proxy = {}, {}
    seen[value] = proxy
    for key, child in raw_pairs(value) do target[key] = freeze(child, seen) end
    setmetatable(proxy, {
      __index = target,
      __newindex = function(_, key)
        error("__studio_read_only__:" .. tostring(key), 2)
      end,
      __len = function() return #target end,
      __metatable = false,
    })
    return proxy
  end

  local function finalize()
    table.sort(declarations, function(a, b) return a.id < b.id end)
    for _, definition in ipairs(declarations) do
      local spec = definition.levels
      if spec ~= nil and spec.__studio_kind == "levels" then
        local rows = {}
        local index = 1
        while spec.rows[index] ~= nil do
          rows[index] = spec.rows[index]
          index = index + 1
        end
        definition.levels = { mode = spec.mode, rows = rows }
      end
    end
    return declarations
  end

  return items, levels, finalize, freeze
end
'''


class SandboxFailure(Exception):
    def __init__(
        self,
        code: str,
        message: str,
        *,
        file: str = "items.lua.json",
        line: int = 1,
        column: int = 1,
        path: str = "$",
    ) -> None:
        super().__init__(message)
        self.error = {
            "code": code,
            "message": message,
            "file": file,
            "line": line,
            "column": column,
            "path": path,
        }


def _failure(code: str, message: str, **location: Any) -> SandboxFailure:
    return SandboxFailure(code, message, **location)


def _inside(root: Path, value: str) -> Path:
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError as error:
        raise _failure("manifest.path_escape", f"path escapes sandbox root: {value}") from error
    return path


def _load_manifest(root: Path, manifest_path: Path) -> tuple[dict[str, tuple[Path, str]], list[str]]:
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise _failure("manifest.read", f"cannot read manifest: {error}") from error
    if data.get("schema") != SCHEMA:
        raise _failure("manifest.schema", f"unsupported manifest schema: {data.get('schema')!r}")
    modules: dict[str, tuple[Path, str]] = {}
    for entry in data.get("modules", []):
        name = entry.get("name")
        rel = entry.get("file")
        if not isinstance(name, str) or not name or not isinstance(rel, str) or not rel:
            raise _failure("manifest.module", "manifest module requires name and file")
        if name in modules:
            raise _failure("manifest.duplicate_module", f"duplicate module: {name}")
        path = _inside(root, rel)
        modules[name] = (path, path.relative_to(root).as_posix())
    entries = data.get("entries", [])
    if not isinstance(entries, list) or not all(isinstance(name, str) for name in entries):
        raise _failure("manifest.entries", "manifest entries must be module names")
    return modules, sorted(set(entries))


def _convert(lua_type, value: Any, seen: set[int] | None = None) -> Any:
    if lua_type(value) != "table":
        if lua_type(value) is not None:
            raise _failure("output.unsupported_type", f"unsupported Lua output type: {lua_type(value)}")
        return value
    seen = seen or set()
    identity = id(value)
    if identity in seen:
        raise _failure("output.cycle", "cyclic Lua output")
    seen.add(identity)
    keys = list(value.keys())
    if keys and all(isinstance(key, int) and not isinstance(key, bool) for key in keys) and sorted(keys) == list(range(1, len(keys) + 1)):
        result = [_convert(lua_type, value[index], seen) for index in range(1, len(keys) + 1)]
    elif not keys:
        result = {}
    else:
        if not all(isinstance(key, str) for key in keys):
            raise _failure("output.key_type", "Lua object keys must be strings")
        result = {key: _convert(lua_type, value[key], seen) for key in sorted(keys)}
    seen.remove(identity)
    return result


def _evaluate(request: dict[str, Any]) -> dict[str, Any]:
    import lupa.lua54 as lupa

    root = Path(request["root"]).resolve()
    manifest_path = Path(request["manifest"]).resolve()
    modules, entries = _load_manifest(root, manifest_path)
    runtime = lupa.LuaRuntime(
        register_eval=False,
        register_builtins=False,
        unpack_returned_tuples=True,
        max_memory=int(request.get("memoryBytes", DEFAULT_MEMORY_BYTES)),
        attribute_filter=lambda _obj, _name, _setting: (_ for _ in ()).throw(AttributeError("access denied")),
    )
    items, levels, finalize, freeze = runtime.execute(PRELUDE, name="@studio/sandbox.lua", mode="t")()
    builtins = {"studio.items": freeze(items), "studio.levels": freeze(levels)}
    cache: dict[str, Any] = {}
    loading: list[str] = []
    make_env = runtime.eval('''function(allowed)
      return setmetatable({}, {
        __index = function(_, key)
          local value = allowed[key]
          if value ~= nil then return value end
          error("__studio_forbidden_global__:" .. tostring(key), 2)
        end,
        __newindex = function(_, key)
          error("__studio_global_assignment__:" .. tostring(key), 2)
        end,
        __metatable = false,
      })
    end''')
    compile_chunk = runtime.eval('''function(source, name, env)
      local chunk, message = load(source, name, "t", env)
      if not chunk then error(message, 0) end
      return chunk
    end''')

    def require_module(name: str):
        if name in builtins:
            return builtins[name]
        if name in cache:
            return cache[name]
        if name in loading:
            cycle = " -> ".join([*loading[loading.index(name):], name])
            current = modules[loading[-1]][1]
            raise _failure(
                "module.cycle", f"module cycle: {cycle}", file=current,
                path=f"$.modules.{loading[-1]}.require",
            )
        if name not in modules:
            current = modules[loading[-1]][1] if loading else "items.lua.json"
            raise _failure(
                "module.not_approved", f"module not approved: {name}", file=current,
                path=f"$.modules.{loading[-1]}.require" if loading else "$.entries",
            )
        path, rel = modules[name]
        loading.append(name)
        try:
            try:
                source = path.read_text(encoding="utf-8")
            except UnicodeDecodeError as error:
                raise _failure(
                    "source.encoding", "Lua source must be UTF-8 text; bytecode is forbidden",
                    file=rel, path=f"$.modules.{name}",
                ) from error
            except OSError as error:
                raise _failure(
                    "source.read", f"cannot read Lua source: {error}",
                    file=rel, path=f"$.modules.{name}",
                ) from error
            allowed = runtime.table_from({
                "assert": runtime.globals()["assert"],
                "error": runtime.globals()["error"],
                "ipairs": runtime.globals()["ipairs"],
                "pcall": runtime.globals()["pcall"],
                "select": runtime.globals()["select"],
                "tonumber": runtime.globals()["tonumber"],
                "tostring": runtime.globals()["tostring"],
                "type": runtime.globals()["type"],
                "require": require_module,
            })
            env = make_env(allowed)
            try:
                result = compile_chunk(source, f"@{rel}", env)()
            except SandboxFailure:
                raise
            except Exception as error:
                message = str(error)
                match = re.search(r"(?:^|\n)(?:\[string \"@)?@?([^\"\n:]+\.lua)\"?]?:([0-9]+):", message)
                line = int(match.group(2)) if match else 1
                if "__studio_forbidden_global__:" in message:
                    global_name = message.split("__studio_forbidden_global__:", 1)[1].splitlines()[0]
                    code = "sandbox.forbidden_global"
                    clean = f"global is unavailable: {global_name}"
                elif "__studio_global_assignment__:" in message:
                    global_name = message.split("__studio_global_assignment__:", 1)[1].splitlines()[0]
                    code = "sandbox.global_assignment"
                    clean = f"global assignment is forbidden: {global_name}"
                elif "__studio_read_only__:" in message:
                    field_name = message.split("__studio_read_only__:", 1)[1].splitlines()[0]
                    code = "sandbox.read_only"
                    clean = f"approved input is read-only: {field_name}"
                else:
                    code = "lua.execution"
                    clean = message.splitlines()[0]
                raise _failure(
                    code, clean, file=rel, line=line,
                    path=f"$.modules.{name}",
                ) from error
            cache[name] = True if result is None else freeze(result)
            return cache[name]
        finally:
            loading.pop()

    for name in entries:
        require_module(name)
    return {
        "schema": "items.lua.evaluation.v1",
        "backend": {
            "package": f"lupa@{importlib.metadata.version('lupa')}",
            "module": BACKEND_MODULE,
            "implementation": runtime.lua_implementation,
            "version": ".".join(map(str, runtime.lua_version)),
        },
        "items": _convert(lupa.lua_type, finalize()),
    }


def _worker() -> int:
    try:
        payload = _evaluate(json.loads(sys.stdin.read()))
        print(json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")))
        return 0
    except SandboxFailure as error:
        print(json.dumps({"schema": "items.lua.error.v1", "error": error.error}, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1
    except Exception as error:  # fresh worker turns every backend failure into data
        failure = _failure("sandbox.internal", str(error))
        print(json.dumps({"schema": "items.lua.error.v1", "error": failure.error}, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1


def _parent(args: argparse.Namespace) -> int:
    request = {
        "root": str(Path(args.root).resolve()),
        "manifest": str(Path(args.manifest).resolve()),
        "memoryBytes": args.memory_bytes,
    }
    try:
        result = subprocess.run(
            [sys.executable, str(Path(__file__).resolve()), "--worker"],
            input=json.dumps(request), text=True, capture_output=True, encoding="utf-8",
            timeout=args.timeout_ms / 1000,
        )
    except subprocess.TimeoutExpired:
        failure = _failure("sandbox.timeout", "evaluation timed out")
        print(json.dumps({"schema": "items.lua.error.v1", "error": failure.error}, sort_keys=True), file=sys.stderr)
        return 1
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["evaluate"])
    parser.add_argument("--root", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--timeout-ms", type=int, default=DEFAULT_TIMEOUT_MS)
    parser.add_argument("--memory-bytes", type=int, default=DEFAULT_MEMORY_BYTES)
    args = parser.parse_args(argv)
    return _parent(args)


if __name__ == "__main__":
    if sys.argv[1:] == ["--worker"]:
        raise SystemExit(_worker())
    raise SystemExit(main())
