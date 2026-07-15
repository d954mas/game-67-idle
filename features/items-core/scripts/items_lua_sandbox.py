#!/usr/bin/env python3
"""Fresh-process deterministic Lua evaluator for Items authoring."""

from __future__ import annotations

import argparse
import importlib.metadata
import json
import math
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
DEFAULT_MAX_MANIFEST_BYTES = 1024 * 1024
DEFAULT_MAX_SOURCE_BYTES = 4 * 1024 * 1024


PRELUDE = r'''
return function(raise_internal)
  local raw_error = error
  local raw_debug, raw_math = debug, math
  local raw_pairs, raw_type = pairs, type
  local raw_string_sub = string.sub
  local declarations = {}
  local frozen_targets = {}
  local source_metadata = {}
  local freeze

  local function fail(code, message)
    local source, line = "items.lua.json", 1
    for level = 2, 12 do
      local info = raw_debug.getinfo(level, "Sl")
      if info == nil then break end
      if raw_type(info.source) == "string" and raw_string_sub(info.source, 1, 1) == "@"
          and info.source ~= "@studio/sandbox.lua" then
        source = raw_string_sub(info.source, 2)
        line = info.currentline > 0 and info.currentline or 1
        break
      end
    end
    return raise_internal(code, message, source, line)
  end

  local function safe_error(message)
    if raw_type(message) ~= "string" then
      return fail("sandbox.error_contract", "error message must be a string")
    end
    raw_error(message, 2)
  end

  local function safe_assert(condition, message)
    if condition then return condition, message end
    safe_error(message or "assertion failed")
  end

  local function copy(value, seen)
    if raw_type(value) ~= "table" then return value end
    local source = source_metadata[value]
    value = frozen_targets[value] or value
    seen = seen or {}
    if seen[value] then return fail("declaration.cycle", "cyclic declaration table") end
    seen[value] = true
    local out = {}
    if source ~= nil then source_metadata[out] = source end
    for key, child in raw_pairs(value) do out[copy(key, seen)] = copy(child, seen) end
    seen[value] = nil
    return out
  end

  local function source_at(level)
    local info = raw_debug.getinfo(level, "Sl")
    local file = info and info.source or "items.lua.json"
    if raw_type(file) == "string" and raw_string_sub(file, 1, 1) == "@" then
      file = raw_string_sub(file, 2)
    end
    return {
      file = file,
      line = info and info.currentline > 0 and info.currentline or 1,
      column = 1,
    }
  end

  local function tagged(kind, value, source)
    value.__studio_kind = kind
    local proxy = freeze(value)
    if source ~= nil then source_metadata[proxy] = source end
    return proxy
  end

  local items = {}
  function items.ref(id) return tagged("item_ref", { id = id }, source_at(3)) end
  function items.cost(item, count) return tagged("cost", { item = item, count = count }) end
  function items.costs(entries) return tagged("costs", { entries = entries }) end
  function items.free() return tagged("free", {}) end
  function items.define(definition)
    local copied = copy(definition)
    copied.__studio_source = source_at(3)
    copied.__studio_source.kind = "definition"
    declarations[#declarations + 1] = copied
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
      return fail("formula.contract", "levels.generate requires max_level and row")
    end
    local index = 1
    while true do
      local name = raw_debug.getupvalue(options.row, index)
      if name == nil then break end
      return fail("formula.mutable_upvalue", "formula captures mutable upvalue")
    end
    return tagged("levels", {
      mode = "generate", max_level = options.max_level, row = options.row,
    })
  end

  local MAX_EXACT = 9007199254740991
  local function checked(value)
    if raw_math.type(value) ~= "integer" or value < -MAX_EXACT or value > MAX_EXACT then
      return fail("formula.math", "expected exact integer")
    end
    return value
  end
  local studio_math = {}
  function studio_math.add(a, b)
    a, b = checked(a), checked(b)
    if (b > 0 and a > MAX_EXACT - b) or (b < 0 and a < -MAX_EXACT - b) then
      return fail("formula.math", "addition overflow")
    end
    return a + b
  end
  function studio_math.sub(a, b) return studio_math.add(a, -checked(b)) end
  function studio_math.mul(a, b)
    a, b = checked(a), checked(b)
    if a ~= 0 and b ~= 0 and raw_math.abs(a) > MAX_EXACT // raw_math.abs(b) then
      return fail("formula.math", "multiplication overflow")
    end
    return a * b
  end
  function studio_math.idiv(a, b)
    a, b = checked(a), checked(b)
    if b == 0 then return fail("formula.math", "division by zero") end
    return a // b
  end
  function studio_math.min(a, b) a, b = checked(a), checked(b); if a < b then return a else return b end end
  function studio_math.max(a, b) a, b = checked(a), checked(b); if a > b then return a else return b end end

  freeze = function(value, seen)
    if raw_type(value) ~= "table" then return value end
    if frozen_targets[value] then return value end
    seen = seen or {}
    if seen[value] then return seen[value] end
    local target, proxy = {}, {}
    seen[value] = proxy
    for key, child in raw_pairs(value) do target[key] = freeze(child, seen) end
    frozen_targets[proxy] = target
    setmetatable(proxy, {
      __index = target,
      __newindex = function()
        return fail("sandbox.read_only", "approved input is read-only")
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
    raw_debug.sethook(function(event)
      if event == "count" then
        instructions = instructions + interval
        if instructions > instruction_limit then
          fail("sandbox.instruction_limit", "instruction limit exceeded")
        end
      elseif event == "call" then
        depth = depth + 1
        if depth > recursion_limit then
          fail("sandbox.recursion_limit", "recursion limit exceeded")
        end
      elseif event == "return" then
        if depth > 0 then depth = depth - 1 end
      end
    end, "cr", interval)
  end

  local function finalize()
    local registered = {}
    for _, definition in ipairs(declarations) do
      local id = definition.id
      if raw_type(id) == "string" then
        if registered[id] then
          local source = definition.__studio_source
          return raise_internal(
            "definition.duplicate_id", "duplicate item id: " .. id,
            source.file, source.line
          )
        end
        registered[id] = true
      end
    end
    table.sort(declarations, function(a, b) return a.id < b.id end)

    local function resolve_refs(value, seen, fallback_source)
      if raw_type(value) ~= "table" then return end
      seen = seen or {}
      if seen[value] then return end
      seen[value] = true
      if value.__studio_kind == "item_ref" then
        local source, id = source_metadata[value] or fallback_source, value.id
        if raw_type(id) ~= "string" or not registered[id] then
          local message = "item reference id must be a string"
          if raw_type(id) == "string" then message = "missing item reference: " .. id end
          return raise_internal(
            "reference.missing", message,
            source.file, source.line
          )
        end
        source_metadata[value] = nil
      end
      for _, child in raw_pairs(value) do resolve_refs(child, seen, fallback_source) end
    end

    for _, definition in ipairs(declarations) do
      resolve_refs(definition, nil, definition.__studio_source)
    end
    for _, definition in ipairs(declarations) do
      local spec = definition.levels
      if spec ~= nil and spec.__studio_kind == "levels" then
        local rows = {}
        if spec.mode == "generate" then
          local max_level = checked(spec.max_level)
          if max_level < 1 then return fail("formula.contract", "max_level must be positive") end
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

  local function lock_string_surface()
    raw_debug.setmetatable("", {
      __index = function()
        return fail("sandbox.string_surface", "the Lua string library is unavailable")
      end,
      __metatable = false,
    })
  end

  return items, levels, math_view, finalize, freeze, setup_limits,
    safe_assert, safe_error, lock_string_surface
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
        if manifest_path.stat().st_size > DEFAULT_MAX_MANIFEST_BYTES:
            raise _failure("manifest.byte_limit", "manifest exceeds byte limit")
        raw_manifest = manifest_path.read_bytes()
        if len(raw_manifest) > DEFAULT_MAX_MANIFEST_BYTES:
            raise _failure("manifest.byte_limit", "manifest exceeds byte limit")
        data = json.loads(raw_manifest.decode("utf-8"))
    except SandboxFailure:
        raise
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise _failure("manifest.read", f"cannot read manifest: {error}") from error
    if not isinstance(data, dict):
        raise _failure("manifest.shape", "manifest root must be an object")
    if data.get("schema") != SCHEMA:
        raise _failure("manifest.schema", f"unsupported manifest schema: {data.get('schema')!r}")
    modules: dict[str, tuple[Path, str]] = {}
    for entry in data.get("modules", []):
        if not isinstance(entry, dict):
            raise _failure("manifest.module", "manifest module must be an object")
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
        if isinstance(value, float) and not math.isfinite(value):
            raise _failure("output.non_finite", "Lua output contains a non-finite number")
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


def _normalize_lua_failure(
    error: Exception,
    fallback_file: str,
    path: str,
    allowed_files: set[str] | None = None,
) -> SandboxFailure:
    if isinstance(error, SandboxFailure):
        if error.error["file"] == "items.lua.json":
            error.error["file"] = fallback_file
            error.error["path"] = path
        return error
    message = str(error)
    match = re.search(r'(?:^|\n)(?:\[string "@)?@?([^"\n:]+\.lua)"?]?:([0-9]+):', message)
    file = match.group(1).strip() if match else fallback_file
    if allowed_files is not None and file not in allowed_files:
        file = fallback_file
    line = int(match.group(2)) if match else 1
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


def _raw_exponent_line(source: str) -> int | None:
    index, line, length = 0, 1, len(source)
    while index < length:
        char = source[index]
        if char == "\n":
            line += 1
            index += 1
            continue
        if char in ("'", '"'):
            quote = char
            index += 1
            while index < length:
                if source[index] == "\\":
                    index += 2
                elif source[index] == quote:
                    index += 1
                    break
                else:
                    if source[index] == "\n":
                        line += 1
                    index += 1
            continue
        long_match = re.match(r"\[(=*)\[", source[index:]) if char == "[" else None
        if source.startswith("--", index):
            comment_match = re.match(r"--\[(=*)\[", source[index:])
            if comment_match:
                equals = comment_match.group(1)
                end = source.find("]" + equals + "]", index + len(comment_match.group(0)))
                stop = length if end < 0 else end + len(equals) + 2
                line += source[index:stop].count("\n")
                index = stop
            else:
                end = source.find("\n", index + 2)
                index = length if end < 0 else end
            continue
        if long_match:
            equals = long_match.group(1)
            end = source.find("]" + equals + "]", index + len(long_match.group(0)))
            stop = length if end < 0 else end + len(equals) + 2
            line += source[index:stop].count("\n")
            index = stop
            continue
        if char == "^":
            return line
        index += 1
    return None


def _evaluate(request: dict[str, Any]) -> dict[str, Any]:
    import lupa.lua54 as lupa

    root = Path(request["root"]).resolve()
    manifest_path = Path(request["manifest"]).resolve()
    modules, entries = _load_manifest(root, manifest_path)
    allowed_files = {rel for _path, rel in modules.values()}
    fallback = modules[entries[0]][1] if entries else "items.lua.json"
    file_to_module = {rel: name for name, (_path, rel) in modules.items()}

    def raise_internal(code: str, message: str, file: str, line: int):
        safe_file = file if file in allowed_files else fallback
        module_name = file_to_module.get(safe_file)
        raise _failure(
            code, message, file=safe_file,
            line=line if isinstance(line, int) and line > 0 else 1,
            path=f"$.modules.{module_name}" if module_name else "$",
        )

    runtime = lupa.LuaRuntime(
        register_eval=False,
        register_builtins=False,
        unpack_returned_tuples=True,
        max_memory=int(request.get("memoryBytes", DEFAULT_MEMORY_BYTES)),
        attribute_filter=lambda _obj, _name, _setting: (_ for _ in ()).throw(AttributeError("access denied")),
    )
    (
        items, levels, studio_math, finalize, freeze, setup_limits,
        safe_assert, safe_error, lock_string_surface,
    ) = runtime.execute(
        PRELUDE, name="@studio/sandbox.lua", mode="t",
    )(raise_internal)
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
    source_bytes = 0
    max_source_bytes = int(request.get("maxSourceBytes", DEFAULT_MAX_SOURCE_BYTES))
    make_env = runtime.eval('''function(allowed, raise_env)
      local getinfo, to_string = debug.getinfo, tostring
      return setmetatable({}, {
        __index = function(_, key)
          local value = allowed[key]
          if value ~= nil then return value end
          local info = getinfo(2, "l")
          return raise_env("sandbox.forbidden_global", "global is unavailable: " .. to_string(key), info.currentline)
        end,
        __newindex = function(_, key)
          local info = getinfo(2, "l")
          return raise_env("sandbox.global_assignment", "global assignment is forbidden: " .. to_string(key), info.currentline)
        end,
        __metatable = false,
      })
    end''')
    compile_chunk = runtime.eval('''function(source, name, env)
      local chunk, message = load(source, name, "t", env)
      if not chunk then error(message, 0) end
      return chunk
    end''')
    make_require = runtime.eval('''function(callback)
      local sub = string.sub
      return function(name)
        local info = debug.getinfo(2, "Sl")
        local source = info and info.source or "items.lua.json"
        if sub(source, 1, 1) == "@" then source = sub(source, 2) end
        return callback(name, source, info and info.currentline or 1)
      end
    end''')
    lock_string_surface()

    def require_module(name: str, caller_file: str | None = None, caller_line: int = 1):
        nonlocal source_bytes
        if not isinstance(name, str):
            current = caller_file or (modules[loading[-1]][1] if loading else "items.lua.json")
            raise _failure(
                "module.invalid_name", "module name must be a string", file=current, line=caller_line,
                path=f"$.modules.{loading[-1]}.require" if loading else "$.entries",
            )
        if name in builtins:
            return builtins[name]
        if name in cache:
            return cache[name]
        if name in loading:
            cycle = " -> ".join([*loading[loading.index(name):], name])
            current = caller_file or modules[loading[-1]][1]
            raise _failure(
                "module.cycle", f"module cycle: {cycle}", file=current, line=caller_line,
                path=f"$.modules.{loading[-1]}.require",
            )
        if name not in modules:
            current = caller_file or (modules[loading[-1]][1] if loading else "items.lua.json")
            raise _failure(
                "module.not_approved", f"module not approved: {name}", file=current, line=caller_line,
                path=f"$.modules.{loading[-1]}.require" if loading else "$.entries",
            )
        path, rel = modules[name]
        loading.append(name)
        try:
            try:
                declared_size = path.stat().st_size
                if source_bytes + declared_size > max_source_bytes:
                    raise _failure(
                        "source.byte_limit", f"Lua sources exceed {max_source_bytes} bytes",
                        file=rel, path=f"$.modules.{name}",
                    )
                raw_source = path.read_bytes()
                if source_bytes + len(raw_source) > max_source_bytes:
                    raise _failure(
                        "source.byte_limit", f"Lua sources exceed {max_source_bytes} bytes",
                        file=rel, path=f"$.modules.{name}",
                    )
                source_bytes += len(raw_source)
            except SandboxFailure:
                raise
            except OSError as error:
                raise _failure(
                    "source.read", f"cannot read Lua source: {error}",
                    file=rel, path=f"$.modules.{name}",
                ) from error
            if raw_source.startswith(b"\x1bLua"):
                raise _failure(
                    "source.bytecode", "Lua bytecode is forbidden",
                    file=rel, path=f"$.modules.{name}",
                )
            try:
                source = raw_source.decode("utf-8")
            except UnicodeDecodeError as error:
                raise _failure(
                    "source.encoding", "Lua source must be UTF-8 text",
                    file=rel, path=f"$.modules.{name}",
                ) from error
            exponent_line = _raw_exponent_line(source)
            if exponent_line is not None:
                raise _failure(
                    "source.raw_exponentiation",
                    "raw exponentiation is unavailable; use the approved Studio math surface",
                    file=rel, line=exponent_line, path=f"$.modules.{name}",
                )
            allowed = runtime.table_from({
                "assert": safe_assert,
                "error": safe_error,
                "ipairs": runtime.globals()["ipairs"],
                "select": runtime.globals()["select"],
                "type": runtime.globals()["type"],
                "require": make_require(require_module),
            })
            def raise_env(code: str, message: str, line: int):
                raise _failure(
                    code, message, file=rel,
                    line=line if isinstance(line, int) and line > 0 else 1,
                    path=f"$.modules.{name}",
                )

            env = make_env(allowed, raise_env)
            try:
                result = compile_chunk(source, f"@{rel}", env)()
            except Exception as error:
                raise _normalize_lua_failure(error, rel, f"$.modules.{name}", {rel}) from error
            cache[name] = True if result is None else freeze(result)
            return cache[name]
        finally:
            loading.pop()

    for name in entries:
        require_module(name)
    try:
        normalized_items = _convert(lupa.lua_type, finalize())
    except Exception as error:
        raise _normalize_lua_failure(
            error, fallback, "$.items", {rel for _path, rel in modules.values()},
        ) from error
    if not isinstance(normalized_items, list):
        normalized_items = [] if normalized_items == {} else normalized_items
    sources: dict[str, dict[str, Any]] = {}
    for item in normalized_items if isinstance(normalized_items, list) else []:
        if isinstance(item, dict):
            source = item.pop("__studio_source", None)
            item_id = item.get("id")
            if isinstance(item_id, str) and isinstance(source, dict):
                sources[item_id] = source
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
        "sources": sources,
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


def _request_from_args(args: argparse.Namespace) -> dict[str, Any]:
    return {
        "root": str(Path(args.root).resolve()),
        "manifest": str(Path(args.manifest).resolve()),
        "memoryBytes": args.memory_bytes,
        "instructionLimit": args.instruction_limit,
        "recursionLimit": args.recursion_limit,
        "maxOutputRows": args.max_output_rows,
        "maxOutputBytes": args.max_output_bytes,
        "maxSourceBytes": args.max_source_bytes,
    }


def _parent(args: argparse.Namespace) -> int:
    request = _request_from_args(args)
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
    parser.add_argument("--max-source-bytes", type=int, default=DEFAULT_MAX_SOURCE_BYTES)
    args = parser.parse_args(argv)
    return _parent(args)


if __name__ == "__main__":
    if sys.argv[1:] == ["--worker"]:
        raise SystemExit(_worker())
    raise SystemExit(main())
