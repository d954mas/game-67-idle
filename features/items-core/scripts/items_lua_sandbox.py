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
DEFAULT_INSTRUCTION_LIMIT = 1_000_000
DEFAULT_RECURSION_LIMIT = 128
DEFAULT_MAX_OUTPUT_ROWS = 10_000
DEFAULT_MAX_OUTPUT_BYTES = 8 * 1024 * 1024


PRELUDE = r'''
return function()
  local raw_debug, raw_math = debug, math
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
  function levels.generate(options)
    if raw_type(options) ~= "table" or raw_type(options.row) ~= "function" then
      error("__studio_formula_contract__:levels.generate requires max_level and row", 2)
    end
    local index = 1
    while true do
      local name = raw_debug.getupvalue(options.row, index)
      if name == nil then break end
      error("__studio_mutable_upvalue__:" .. name, 2)
      index = index + 1
    end
    return tagged("levels", {
      mode = "generate", max_level = options.max_level, row = options.row,
    })
  end

  local MAX_EXACT = 9007199254740991
  local function checked(value)
    if raw_math.type(value) ~= "integer" or value < -MAX_EXACT or value > MAX_EXACT then
      error("__studio_math__:expected exact integer", 2)
    end
    return value
  end
  local studio_math = {}
  function studio_math.add(a, b)
    a, b = checked(a), checked(b)
    if (b > 0 and a > MAX_EXACT - b) or (b < 0 and a < -MAX_EXACT - b) then
      error("__studio_math__:addition overflow", 2)
    end
    return a + b
  end
  function studio_math.sub(a, b) return studio_math.add(a, -checked(b)) end
  function studio_math.mul(a, b)
    a, b = checked(a), checked(b)
    if a ~= 0 and b ~= 0 and raw_math.abs(a) > MAX_EXACT // raw_math.abs(b) then
      error("__studio_math__:multiplication overflow", 2)
    end
    return a * b
  end
  function studio_math.idiv(a, b)
    a, b = checked(a), checked(b)
    if b == 0 then error("__studio_math__:division by zero", 2) end
    return a // b
  end
  function studio_math.min(a, b) a, b = checked(a), checked(b); if a < b then return a else return b end end
  function studio_math.max(a, b) a, b = checked(a), checked(b); if a > b then return a else return b end end

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

  local math_view = freeze(studio_math)

  local function setup_limits(instruction_limit, recursion_limit)
    local instructions, depth = 0, 0
    local interval = 100
    local function fail(marker)
      local info = raw_debug.getinfo(3, "Sl")
      local source = info and info.source or "items.lua.json"
      if string.sub(source, 1, 1) == "@" then source = string.sub(source, 2) end
      error(marker .. ":" .. source .. ":" .. tostring(info and info.currentline or 1), 0)
    end
    raw_debug.sethook(function(event)
      if event == "count" then
        instructions = instructions + interval
        if instructions > instruction_limit then
          fail("__studio_instruction_limit__")
        end
      elseif event == "call" then
        depth = depth + 1
        if depth > recursion_limit then
          fail("__studio_recursion_limit__")
        end
      elseif event == "return" then
        if depth > 0 then depth = depth - 1 end
      end
    end, "cr", interval)
  end

  local function finalize()
    table.sort(declarations, function(a, b) return a.id < b.id end)
    for _, definition in ipairs(declarations) do
      local spec = definition.levels
      if spec ~= nil and spec.__studio_kind == "levels" then
        local rows = {}
        if spec.mode == "generate" then
          local max_level = checked(spec.max_level)
          if max_level < 1 then error("__studio_formula_contract__:max_level must be positive", 0) end
          for index = 1, max_level do rows[index] = copy(spec.row(index, math_view)) end
        else
          local index = 1
          while spec.rows[index] ~= nil do
            rows[index] = spec.rows[index]
            index = index + 1
          end
        end
        definition.levels = { mode = spec.mode, rows = rows }
      end
    end
    return declarations
  end

  return items, levels, math_view, finalize, freeze, setup_limits
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


def _normalize_lua_failure(error: Exception, fallback_file: str, path: str) -> SandboxFailure:
    if isinstance(error, SandboxFailure):
        return error
    message = str(error)
    match = re.search(r'(?:^|\n)(?:\[string "@)?@?([^"\n:]+\.lua)"?]?:([0-9]+):', message)
    file = match.group(1).strip() if match else fallback_file
    line = int(match.group(2)) if match else 1
    markers = (
        ("__studio_forbidden_global__:", "sandbox.forbidden_global", "global is unavailable"),
        ("__studio_global_assignment__:", "sandbox.global_assignment", "global assignment is forbidden"),
        ("__studio_read_only__:", "sandbox.read_only", "approved input is read-only"),
        ("__studio_mutable_upvalue__:", "formula.mutable_upvalue", "formula captures mutable upvalue"),
        ("__studio_math__:", "formula.math", "deterministic math rejected value"),
        ("__studio_formula_contract__:", "formula.contract", "invalid formula contract"),
    )
    for marker, code, summary in markers:
        if marker in message:
            detail = message.split(marker, 1)[1].splitlines()[0]
            return _failure(code, f"{summary}: {detail}", file=file, line=line, path=path)
    if "__studio_instruction_limit__" in message:
        marker = re.search(r"__studio_instruction_limit__:([^:\n]+):([0-9]+)", message)
        return _failure(
            "sandbox.instruction_limit", "instruction limit exceeded",
            file=marker.group(1) if marker else fallback_file,
            line=int(marker.group(2)) if marker else 1, path=path,
        )
    if "__studio_recursion_limit__" in message:
        marker = re.search(r"__studio_recursion_limit__:([^:\n]+):([0-9]+)", message)
        return _failure(
            "sandbox.recursion_limit", "recursion limit exceeded",
            file=marker.group(1) if marker else fallback_file,
            line=int(marker.group(2)) if marker else 1, path=path,
        )
    if type(error).__name__ == "LuaMemoryError" or "not enough memory" in message.lower():
        return _failure("sandbox.memory_limit", "Lua memory limit exceeded", file=file, line=line, path=path)
    return _failure("lua.execution", message.splitlines()[0], file=file, line=line, path=path)


def _output_rows(items: list[dict[str, Any]]) -> int:
    rows = len(items)
    for item in items:
        levels = item.get("levels")
        if isinstance(levels, dict) and isinstance(levels.get("rows"), list):
            rows += len(levels["rows"])
    return rows


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
    items, levels, studio_math, finalize, freeze, setup_limits = runtime.execute(
        PRELUDE, name="@studio/sandbox.lua", mode="t",
    )()
    builtins = {
        "studio.items": freeze(items),
        "studio.levels": freeze(levels),
        "studio.math": studio_math,
    }
    setup_limits(
        int(request.get("instructionLimit", DEFAULT_INSTRUCTION_LIMIT)),
        int(request.get("recursionLimit", DEFAULT_RECURSION_LIMIT)),
    )
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
        if not isinstance(name, str):
            current = modules[loading[-1]][1] if loading else "items.lua.json"
            raise _failure(
                "module.invalid_name", "module name must be a string", file=current,
                path=f"$.modules.{loading[-1]}.require" if loading else "$.entries",
            )
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
            if "^" in source:
                line = source[:source.index("^")].count("\n") + 1
                raise _failure(
                    "source.raw_exponentiation",
                    "raw exponentiation is unavailable; use the approved Studio math surface",
                    file=rel, line=line, path=f"$.modules.{name}",
                )
            allowed = runtime.table_from({
                "assert": runtime.globals()["assert"],
                "error": runtime.globals()["error"],
                "ipairs": runtime.globals()["ipairs"],
                "select": runtime.globals()["select"],
                "type": runtime.globals()["type"],
                "require": require_module,
            })
            env = make_env(allowed)
            try:
                result = compile_chunk(source, f"@{rel}", env)()
            except Exception as error:
                raise _normalize_lua_failure(error, rel, f"$.modules.{name}") from error
            cache[name] = True if result is None else freeze(result)
            return cache[name]
        finally:
            loading.pop()

    for name in entries:
        require_module(name)
    fallback = modules[entries[0]][1] if entries else "items.lua.json"
    try:
        normalized_items = _convert(lupa.lua_type, finalize())
    except Exception as error:
        raise _normalize_lua_failure(error, fallback, "$.items") from error
    if not isinstance(normalized_items, list):
        normalized_items = [] if normalized_items == {} else normalized_items
    max_rows = int(request.get("maxOutputRows", DEFAULT_MAX_OUTPUT_ROWS))
    if _output_rows(normalized_items) > max_rows:
        raise _failure(
            "output.row_limit", f"output exceeds {max_rows} rows",
            file=fallback, path="$.items",
        )
    payload = {
        "schema": "items.lua.evaluation.v1",
        "backend": {
            "package": f"lupa@{importlib.metadata.version('lupa')}",
            "module": BACKEND_MODULE,
            "implementation": runtime.lua_implementation,
            "version": ".".join(map(str, runtime.lua_version)),
        },
        "items": normalized_items,
    }
    max_bytes = int(request.get("maxOutputBytes", DEFAULT_MAX_OUTPUT_BYTES))
    encoded_size = len(json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8"))
    if encoded_size > max_bytes:
        raise _failure(
            "output.byte_limit", f"output exceeds {max_bytes} bytes",
            file=fallback, path="$.items",
        )
    return payload


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
        "instructionLimit": args.instruction_limit,
        "recursionLimit": args.recursion_limit,
        "maxOutputRows": args.max_output_rows,
        "maxOutputBytes": args.max_output_bytes,
    }
    try:
        result = subprocess.run(
            [sys.executable, str(Path(__file__).resolve()), "--worker"],
            input=json.dumps(request), text=True, capture_output=True, encoding="utf-8",
            timeout=args.timeout_ms / 1000,
        )
    except subprocess.TimeoutExpired:
        try:
            modules, entries = _load_manifest(Path(request["root"]), Path(request["manifest"]))
            file = modules[entries[0]][1] if entries else Path(args.manifest).name
            path = f"$.modules.{entries[0]}" if entries else "$.entries"
        except Exception:
            file, path = Path(args.manifest).name, "$"
        failure = _failure("sandbox.timeout", "evaluation timed out", file=file, path=path)
        print(json.dumps({"schema": "items.lua.error.v1", "error": failure.error}, sort_keys=True), file=sys.stderr)
        return 1
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    if result.returncode and not result.stderr:
        failure = _failure(
            "sandbox.worker_exit", f"isolated worker exited with code {result.returncode}",
            file=Path(args.manifest).name,
        )
        print(json.dumps({"schema": "items.lua.error.v1", "error": failure.error}, sort_keys=True), file=sys.stderr)
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["evaluate"])
    parser.add_argument("--root", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--timeout-ms", type=int, default=DEFAULT_TIMEOUT_MS)
    parser.add_argument("--memory-bytes", type=int, default=DEFAULT_MEMORY_BYTES)
    parser.add_argument("--instruction-limit", type=int, default=DEFAULT_INSTRUCTION_LIMIT)
    parser.add_argument("--recursion-limit", type=int, default=DEFAULT_RECURSION_LIMIT)
    parser.add_argument("--max-output-rows", type=int, default=DEFAULT_MAX_OUTPUT_ROWS)
    parser.add_argument("--max-output-bytes", type=int, default=DEFAULT_MAX_OUTPUT_BYTES)
    args = parser.parse_args(argv)
    return _parent(args)


if __name__ == "__main__":
    if sys.argv[1:] == ["--worker"]:
        raise SystemExit(_worker())
    raise SystemExit(main())
