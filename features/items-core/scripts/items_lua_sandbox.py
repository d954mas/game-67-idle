#!/usr/bin/env python3
"""Fresh-process deterministic Lua evaluator for Items authoring."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import sys
from typing import Any

import lupa as lupa_package


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
  local raw_string_match, raw_string_sub = string.match, string.sub
  local declarations = {}
  local schema_extensions = {}
  local requirement_declarations, waiver_declarations = {}, {}
  local frozen_targets = {}
  local formula_safe_upvalues = {}
  local requirement_safe_upvalues = {}
  local formula_upvalues = {}
  local formula_sources = {}
  local active_formula_source = nil
  local authentic_item_refs = {}
  local authentic_fields = {}
  local authentic_requirement_results = {}
  local requirement_upvalues = {}
  local source_metadata = {}
  local evaluation_finalizing = false
  local freeze

  local function fail(code, message)
    local source, line = "items.lua.json", 1
    for level = 2, 32 do
      local info = raw_debug.getinfo(level, "Sl")
      if info == nil then break end
      if raw_type(info.source) == "string" and raw_string_sub(info.source, 1, 1) == "@"
          and info.source ~= "@studio/sandbox.lua" then
        source = raw_string_sub(info.source, 2)
        line = info.currentline > 0 and info.currentline or 1
        break
      end
    end
    if source == "items.lua.json" and active_formula_source ~= nil then
      source, line = active_formula_source.file, active_formula_source.line
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
    local authentic_item_ref = authentic_item_refs[value]
    local authentic_field = authentic_fields[value]
    value = frozen_targets[value] or value
    seen = seen or {}
    if seen[value] then return fail("declaration.cycle", "cyclic declaration table") end
    seen[value] = true
    local out = {}
    if source ~= nil then source_metadata[out] = source end
    if authentic_item_ref then authentic_item_refs[out] = true end
    if authentic_field then authentic_fields[out] = true end
    for key, child in raw_pairs(value) do out[copy(key, seen)] = copy(child, seen) end
    seen[value] = nil
    return out
  end

  local function source_at(_level)
    for level = 2, 32 do
      local info = raw_debug.getinfo(level, "Sl")
      if info == nil then break end
      local file = info.source
      if raw_type(file) == "string" and raw_string_sub(file, 1, 1) == "@"
          and file ~= "@studio/sandbox.lua" then
        return {
          file = raw_string_sub(file, 2),
          line = info.currentline > 0 and info.currentline or 1,
          column = 1,
        }
      end
    end
    if active_formula_source ~= nil then return active_formula_source end
    return {
      file = "items.lua.json",
      line = 1,
      column = 1,
    }
  end

  local function valid_dotted_id(value)
    return raw_type(value) == "string"
      and raw_string_match(value, "^[a-z][a-z0-9_]*%.[a-z][a-z0-9_.]*$") ~= nil
      and raw_string_match(value, "%.%.") == nil
      and raw_string_match(value, "%.$") == nil
      and raw_string_match(value, "%.[^a-z]") == nil
  end

  local function tagged(kind, value, source)
    value.__studio_kind = kind
    local proxy = freeze(value)
    if source ~= nil then source_metadata[proxy] = source end
    if kind == "item_ref" then
      authentic_item_refs[proxy] = true
      formula_safe_upvalues[proxy] = true
      requirement_safe_upvalues[proxy] = true
    elseif kind == "field" then
      authentic_fields[proxy] = true
      requirement_safe_upvalues[proxy] = true
    elseif kind == "requirement_result" then
      authentic_requirement_results[proxy] = true
    end
    return proxy
  end

  local items = {}
  function items.ref(id)
    if evaluation_finalizing then
      return fail("evaluation.phase", "item refs must be registered before formula evaluation")
    end
    return tagged("item_ref", { id = id }, source_at(3))
  end
  function items.cost(item, count) return tagged("cost", { item = item, count = count }, source_at(3)) end
  function items.costs(entries) return tagged("costs", { entries = entries }, source_at(3)) end
  function items.free() return tagged("free", {}, source_at(3)) end
  function items.define(definition)
    if evaluation_finalizing then
      return fail("evaluation.phase", "item definitions are closed before formula evaluation")
    end
    local copied = copy(definition)
    copied.__studio_source = source_at(3)
    copied.__studio_source.kind = "definition"
    declarations[#declarations + 1] = copied
  end
  function items.extend_schema(extension)
    if evaluation_finalizing then
      return fail("evaluation.phase", "schema extensions are closed before formula evaluation")
    end
    if raw_type(extension) ~= "table" then
      return fail("schema.contract", "items.extend_schema requires a table")
    end
    local copied = copy(extension)
    copied.__studio_source = source_at(3)
    schema_extensions[#schema_extensions + 1] = copied
  end

  local field = {}
  function field.i64(options)
    if raw_type(options) ~= "table" then
      return fail("schema.field_contract", "field.i64 requires a table")
    end
    local descriptor = copy(options)
    descriptor.__studio_field_type = "i64"
    return tagged("field", descriptor, source_at(3))
  end

  local requirements = {}
  function requirements.define(options)
    if evaluation_finalizing then
      return fail("evaluation.phase", "requirements are closed during finalization")
    end
    if raw_type(options) ~= "table" or raw_type(options.check) ~= "function" then
      return fail("requirement.contract", "requirements.define requires a check function")
    end
    local captured, index = {}, 1
    while true do
      local name, value = raw_debug.getupvalue(options.check, index)
      if name == nil then break end
      if raw_type(value) ~= "table" or requirement_safe_upvalues[value] ~= true then
        return fail("requirement.mutable_upvalue", "requirement captures a mutable upvalue")
      end
      captured[index] = value
      index = index + 1
    end
    local declaration = copy(options)
    declaration.__studio_source = source_at(3)
    declaration.__studio_source.kind = "requirement"
    requirement_upvalues[declaration.check] = captured
    requirement_declarations[#requirement_declarations + 1] = declaration
  end
  function requirements.waive(options)
    if evaluation_finalizing then
      return fail("evaluation.phase", "requirement waivers are closed during finalization")
    end
    if raw_type(options) ~= "table" then
      return fail("waiver.contract", "requirements.waive requires a table")
    end
    local declaration = copy(options)
    declaration.__studio_source = source_at(3)
    declaration.__studio_source.kind = "waiver"
    waiver_declarations[#waiver_declarations + 1] = declaration
  end

  local levels = {}
  function levels.single(row)
    return tagged("levels", { mode = "single", rows = { [1] = row } }, source_at(3))
  end
  function levels.table(rows)
    return tagged("levels", { mode = "table", rows = rows }, source_at(3))
  end
  function levels.generate(options)
    if raw_type(options) ~= "table" then
      return fail("formula.contract", "levels.generate requires max_level and formula columns")
    end
    local columns, count = {}, 0
    for name, formula in raw_pairs(options) do
      if name ~= "max_level" and name ~= "overrides" then
        if raw_type(name) ~= "string" or raw_type(formula) ~= "function" then
          return fail("formula.contract", "generated columns must be named functions")
        end
        local captured, index = {}, 1
        while true do
          local upvalue_name, value = raw_debug.getupvalue(formula, index)
          if upvalue_name == nil then break end
          if raw_type(value) ~= "table" or formula_safe_upvalues[value] ~= true then
            return fail("formula.mutable_upvalue", "formula captures mutable upvalue")
          end
          captured[index] = value
          index = index + 1
        end
        formula_upvalues[formula] = captured
        local info = raw_debug.getinfo(formula, "S")
        local file = info and info.source or "items.lua.json"
        if raw_type(file) == "string" and raw_string_sub(file, 1, 1) == "@" then
          file = raw_string_sub(file, 2)
        end
        formula_sources[formula] = {
          file = file,
          line = info and info.linedefined > 0 and info.linedefined or 1,
          column = 1,
        }
        columns[name], count = formula, count + 1
      end
    end
    if count == 0 then
      return fail("formula.contract", "levels.generate requires at least one formula column")
    end
    return tagged("levels", {
      mode = "generate",
      max_level = options.max_level,
      columns = columns,
      overrides = copy(options.overrides),
    }, source_at(3))
  end
  function levels.linear(options)
    if raw_type(options) ~= "table" then
      return fail("levels.column_contract", "levels.linear requires a table")
    end
    return tagged("level_column", {
      mode = "linear", start = options.start, step = options.step,
    }, source_at(3))
  end
  function levels.values(values)
    if raw_type(values) ~= "table" then
      return fail("levels.column_contract", "levels.values requires a table")
    end
    return tagged("level_column", {
      mode = "values", values = copy(values),
    }, source_at(3))
  end
  function levels.columns(options)
    if raw_type(options) ~= "table" then
      return fail("levels.column_contract", "levels.columns requires a table")
    end
    local columns, count = {}, 0
    for name, column in raw_pairs(options) do
      if name ~= "max_level" and name ~= "overrides" then
        if raw_type(name) ~= "string" then
          return fail("levels.column_contract", "mixed columns must have string names")
        end
        columns[name] = column
        count = count + 1
      end
    end
    if count == 0 then
      return fail("levels.column_contract", "levels.columns requires at least one column")
    end
    return tagged("levels", {
      mode = "columns",
      max_level = options.max_level,
      columns = columns,
      overrides = copy(options.overrides),
    }, source_at(3))
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
    local source = source_metadata[value]
    local authentic_item_ref = authentic_item_refs[value]
    seen = seen or {}
    if seen[value] then return seen[value] end
    local target, proxy = {}, {}
    seen[value] = proxy
    for key, child in raw_pairs(value) do target[key] = freeze(child, seen) end
    frozen_targets[proxy] = target
    if source ~= nil then source_metadata[proxy] = source end
    if authentic_item_ref then
      authentic_item_refs[proxy] = true
      formula_safe_upvalues[proxy] = true
    end
    if value == items or value == studio_math then
      formula_safe_upvalues[proxy] = true
    end
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
    evaluation_finalizing = true
    local function fail_at(code, message, value, fallback)
      local source = source_metadata[value] or fallback
      return raise_internal(code, message, source.file, source.line)
    end

    local fields, field_sources, registered_field_ids, registered_members = {}, {}, {}, {}
    local registered_field_by_id = {}
    local registered_kinds, candidates = {}, {}
    for _, extension in ipairs(schema_extensions) do
      local extension_source = extension.__studio_source
      for section, members in raw_pairs(extension) do
        if section ~= "__studio_source" then
          if raw_type(section) ~= "string" or raw_type(members) ~= "table" then
            return fail_at(
              "schema.contract", "schema sections must contain field members",
              members, extension_source
            )
          end
          for member, descriptor in raw_pairs(members) do
            candidates[#candidates + 1] = {
              descriptor = descriptor,
              fallback = extension_source,
              member = member,
              section = section,
            }
          end
        end
      end
    end
    local function candidate_text(value)
      if raw_type(value) == "string" then return value end
      return ""
    end
    table.sort(candidates, function(a, b)
      local a_id = raw_type(a.descriptor) == "table" and candidate_text(a.descriptor.id) or ""
      local b_id = raw_type(b.descriptor) == "table" and candidate_text(b.descriptor.id) or ""
      local a_key = candidate_text(a.section) .. "\0" .. candidate_text(a.member) .. "\0" .. a_id
      local b_key = candidate_text(b.section) .. "\0" .. candidate_text(b.member) .. "\0" .. b_id
      if a_key ~= b_key then return a_key < b_key end
      local a_source = raw_type(a.descriptor) == "table" and source_metadata[a.descriptor] or a.fallback
      local b_source = raw_type(b.descriptor) == "table" and source_metadata[b.descriptor] or b.fallback
      if a_source.file ~= b_source.file then return a_source.file < b_source.file end
      return a_source.line < b_source.line
    end)
    for _, candidate in ipairs(candidates) do
      local descriptor = candidate.descriptor
      local descriptor_source = raw_type(descriptor) == "table"
        and source_metadata[descriptor] or nil
      if raw_type(candidate.member) ~= "string" or raw_type(descriptor) ~= "table"
          or descriptor.__studio_kind ~= "field" or descriptor_source == nil then
        return fail_at(
          "schema.invalid_handle", "schema fields must come from studio.field",
          descriptor, candidate.fallback
        )
      end
      local field_id = descriptor.id
      if not valid_dotted_id(field_id) then
        return fail_at(
          "schema.field_id", "field id must contain valid lowercase segments",
          descriptor, candidate.fallback
        )
      end
      if raw_string_sub(field_id, 1, 6) == "items." then
        return fail_at(
          "schema.sealed_field_id", "items.* field ids are sealed",
          descriptor, candidate.fallback
        )
      end
      if registered_field_ids[field_id] ~= nil then
        return fail_at(
          "schema.duplicate_field_id", "duplicate field id: " .. field_id,
          descriptor, candidate.fallback
        )
      end
      local member_key = candidate.section .. "." .. candidate.member
      if registered_members[member_key] ~= nil then
        return fail_at(
          "schema.duplicate_member", "duplicate schema member: " .. member_key,
          descriptor, candidate.fallback
        )
      end
      for _, reserved in ipairs({ "member", "section", "type" }) do
        if descriptor[reserved] ~= nil then
          return fail_at(
            "schema.reserved_key", "field descriptor cannot set reserved key: " .. reserved,
            descriptor, candidate.fallback
          )
        end
      end
      registered_field_ids[field_id], registered_members[member_key] = true, true
      local normalized = {
        id = field_id,
        member = candidate.member,
        section = candidate.section,
        type = descriptor.__studio_field_type,
      }
      field_sources[field_id] = {
        file = descriptor_source.file,
        line = descriptor_source.line,
        column = descriptor_source.column,
        kind = "field",
      }
      for key, value in raw_pairs(descriptor) do
        if key ~= "__studio_kind" and key ~= "__studio_field_type" and key ~= "id" then
          normalized[key] = copy(value)
        end
      end
      registered_field_by_id[field_id] = normalized
      local required_for = normalized.required_for
      if required_for ~= nil then
        if raw_type(required_for) ~= "table" then
          return fail_at(
            "schema.required_for", "required_for must be a contiguous kind list",
            descriptor, candidate.fallback
          )
        end
        local count, max_index = 0, 0
        for index, kind in raw_pairs(required_for) do
          if raw_math.type(index) ~= "integer" or index < 1
              or raw_type(kind) ~= "string" or kind == "" then
            return fail_at(
              "schema.required_for", "required_for must be a contiguous kind list",
              descriptor, candidate.fallback
            )
          end
          count = count + 1
          if index > max_index then max_index = index end
          registered_kinds[kind] = true
        end
        if count ~= max_index then
          return fail_at(
            "schema.required_for", "required_for must be a contiguous kind list",
            descriptor, candidate.fallback
          )
        end
      end
      fields[#fields + 1] = normalized
    end
    table.sort(fields, function(a, b) return a.id < b.id end)

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
        registered[id] = definition
      end
      if raw_type(definition.kind) == "string" and definition.kind ~= "" then
        registered_kinds[definition.kind] = true
      end
    end
    table.sort(declarations, function(a, b) return a.id < b.id end)

    local function resolve_refs(value, seen, fallback_source)
      if raw_type(value) ~= "table" then return end
      seen = seen or {}
      if seen[value] then return end
      seen[value] = true
      if value.__studio_kind == "item_ref" then
        local source, id = source_metadata[value], value.id
        if source == nil or authentic_item_refs[value] ~= true then
          return raise_internal(
            "reference.invalid_handle", "item references must come from items.ref",
            fallback_source.file, fallback_source.line
          )
        end
        if raw_type(id) ~= "string" or registered[id] == nil then
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

    local function normalize_cost(value, fallback)
      if raw_type(value) ~= "table" then
        return fail_at("cost.contract", "cost must be items.cost, items.costs, or items.free", value, fallback)
      end
      local value_source = source_metadata[value]
      if value_source == nil then
        return fail_at("cost.invalid_handle", "costs must come from studio.items", value, fallback)
      end
      if value.__studio_kind == "free" then return { __studio_kind = "free" } end
      local entries
      if value.__studio_kind == "cost" then
        entries = { value }
      elseif value.__studio_kind == "costs" and raw_type(value.entries) == "table" then
        entries = value.entries
      else
        return fail_at("cost.contract", "cost must be items.cost, items.costs, or items.free", value, fallback)
      end

      local count, max_index = 0, 0
      for key, _ in raw_pairs(entries) do
        if raw_math.type(key) ~= "integer" or key < 1 then
          return fail_at("cost.contract", "composite cost entries must be a contiguous list", value, fallback)
        end
        count = count + 1
        if key > max_index then max_index = key end
      end
      if count == 0 or count ~= max_index then
        return fail_at("cost.contract", "composite cost entries must be a non-empty contiguous list", value, fallback)
      end

      local sums, refs, ids = {}, {}, {}
      for index = 1, max_index do
        local entry = entries[index]
        local entry_source = raw_type(entry) == "table" and source_metadata[entry] or nil
        if raw_type(entry) ~= "table" or entry.__studio_kind ~= "cost"
            or entry_source == nil
            or raw_type(entry.item) ~= "table" or entry.item.__studio_kind ~= "item_ref"
            or authentic_item_refs[entry.item] ~= true then
          return fail_at("cost.contract", "each cost entry must come from items.cost", entry, value_source)
        end
        local id, amount = entry.item.id, entry.count
        local resource = registered[id]
        if resource == nil then
          return fail_at("reference.missing", "missing item reference: " .. id, entry, value_source)
        end
        if raw_math.type(resource.stack) ~= "integer" or resource.stack < 0
            or resource.stack > MAX_EXACT or resource.stack == 1 then
          return fail_at("cost.stackable_required", "cost resource must use stack=0 or stack>1: " .. id, entry, value_source)
        end
        if raw_math.type(amount) ~= "integer" or amount <= 0 or amount > MAX_EXACT then
          return fail_at("cost.count", "cost count must be a positive exact integer", entry, value_source)
        end
        if sums[id] == nil then
          sums[id], refs[id], ids[#ids + 1] = amount, entry.item, id
        else
          if sums[id] > MAX_EXACT - amount then
            return fail_at("cost.overflow", "cost sum exceeds exact integer range", entry, value_source)
          end
          sums[id] = sums[id] + amount
        end
      end
      table.sort(ids)
      local normalized = {}
      for index, id in ipairs(ids) do
        normalized[index] = { __studio_kind = "cost", item = refs[id], count = sums[id] }
      end
      if value.__studio_kind == "cost" then return normalized[1] end
      return { __studio_kind = "costs", entries = normalized }
    end

    for _, definition in ipairs(declarations) do
      local spec = definition.levels
      definition.authoring_mode = "none"
      if spec ~= nil then
        if raw_type(spec) ~= "table" or spec.__studio_kind ~= "levels" or source_metadata[spec] == nil then
          return fail_at("levels.invalid_handle", "levels must come from studio.levels", spec, definition.__studio_source)
        end
        if definition.stack ~= 1 then
          return fail_at("levels.unique_required", "levelled items must use stack=1", spec, definition.__studio_source)
        end
        local rows = {}
        if spec.mode == "generate" or spec.mode == "columns" then
          if raw_math.type(spec.max_level) ~= "integer"
              or spec.max_level < 1 or spec.max_level > MAX_EXACT then
            return fail_at(
              "levels.max_level", "generated and mixed levels require a positive exact max_level",
              spec, definition.__studio_source
            )
          end
        end
        if spec.mode == "generate" then
          local max_level = spec.max_level
          local columns = copy(spec.columns)
          local names = {}
          for name, _ in raw_pairs(columns) do names[#names + 1] = name end
          table.sort(names)
          for index = 1, max_level do
            local row = {}
            for _, name in ipairs(names) do
              local formula = columns[name]
              active_formula_source = formula_sources[formula]
              local value = formula(index, math_view)
              if raw_type(value) == "number" and raw_math.type(value) == "integer" then
                value = checked(value)
              end
              for upvalue_index, expected in ipairs(formula_upvalues[formula]) do
                local _, current = raw_debug.getupvalue(formula, upvalue_index)
                if current ~= expected then
                  return fail_at(
                    "formula.mutable_upvalue", "formula changed a captured upvalue",
                    spec, definition.__studio_source
                  )
                end
              end
              active_formula_source = nil
              if value ~= nil then row[name] = copy(value) end
            end
            rows[index] = row
          end
        elseif spec.mode == "columns" then
          local max_level, columns = spec.max_level, copy(spec.columns)
          for index = 1, max_level do rows[index] = {} end
          local names = {}
          for name, _ in raw_pairs(columns) do names[#names + 1] = name end
          table.sort(names)
          for _, name in ipairs(names) do
            local column = columns[name]
            local column_source = raw_type(column) == "table" and source_metadata[column] or nil
            if raw_type(column) ~= "table" or column.__studio_kind ~= "level_column"
                or column_source == nil then
              return fail_at(
                "levels.column_handle", "mixed columns must come from studio.levels",
                column, source_metadata[spec]
              )
            end
            if column.mode == "linear" then
              active_formula_source = column_source
              local start, step = checked(column.start), checked(column.step)
              for index = 1, max_level do
                rows[index][name] = studio_math.add(
                  start, studio_math.mul(index - 1, step)
                )
              end
              active_formula_source = nil
            elseif column.mode == "values" then
              local values = copy(column.values)
              for level, value in raw_pairs(values) do
                if raw_math.type(level) ~= "integer" or level < 1 or level > max_level then
                  return fail_at(
                    "levels.column_range", "column value level is outside max_level",
                    column, source_metadata[spec]
                  )
                end
                rows[level][name] = copy(value)
              end
            else
              return fail_at(
                "levels.column_handle", "unknown mixed column mode",
                column, source_metadata[spec]
              )
            end
          end
        else
          if raw_type(spec.rows) ~= "table" then
            return fail_at("levels.contract", "levels rows must be a table", spec, definition.__studio_source)
          end
          local count, max_level = 0, 0
          for key, _ in raw_pairs(spec.rows) do
            if raw_math.type(key) ~= "integer" or key < 1 then
              return fail_at("levels.non_contiguous", "level keys must be positive contiguous integers", spec, definition.__studio_source)
            end
            count = count + 1
            if key > max_level then max_level = key end
          end
          if count == 0 or count ~= max_level then
            return fail_at("levels.non_contiguous", "level keys must be positive contiguous integers", spec, definition.__studio_source)
          end
          for index = 1, max_level do
            rows[index] = spec.rows[index]
          end
        end
        local provenance = {}
        for index, row in ipairs(rows) do
          provenance[index] = {}
          for name, _ in raw_pairs(row) do provenance[index][name] = spec.mode end
        end
        if spec.overrides ~= nil then
          local overrides = copy(spec.overrides)
          if raw_type(overrides) ~= "table" then
            return fail_at("levels.overrides", "overrides must be a table", spec, definition.__studio_source)
          end
          for level, values in raw_pairs(overrides) do
            if raw_math.type(level) ~= "integer" or level < 1 or level > #rows
                or raw_type(values) ~= "table" then
              return fail_at("levels.overrides", "override levels must be in range with row tables", spec, definition.__studio_source)
            end
            for name, value in raw_pairs(values) do
              rows[level][name] = copy(value)
              provenance[level][name] = "override"
            end
          end
        end
        for index, row in ipairs(rows) do
          if raw_type(row) ~= "table" then
            return fail_at("levels.contract", "each level row must be a table", spec, definition.__studio_source)
          end
          active_formula_source = source_metadata[spec]
          for name, value in raw_pairs(row) do
            if raw_type(value) == "number" and raw_math.type(value) == "integer" then
              row[name] = checked(value)
            end
          end
          active_formula_source = nil
          if index == 1 and row.cost_to_reach ~= nil then
            return fail_at("levels.level_one_transition", "level 1 cannot have cost_to_reach", spec, definition.__studio_source)
          end
          if index >= 2 and row.cost_to_reach == nil then
            return fail_at("levels.transition_required", "level 2+ requires paid or explicit free cost_to_reach", spec, definition.__studio_source)
          end
          if row.cost_to_reach ~= nil then
            row.cost_to_reach = normalize_cost(row.cost_to_reach, definition.__studio_source)
          end
        end
        definition.authoring_mode = spec.mode
        definition.levels = { mode = spec.mode, provenance = provenance, rows = rows }
      end
      if raw_type(definition.acquire) == "table" and definition.acquire.cost ~= nil then
        definition.acquire.cost = normalize_cost(definition.acquire.cost, definition.__studio_source)
      end
    end

    local requirement_results, requirement_sources = {}, {}
    local waiver_by_id, waiver_sources = {}, {}
    for _, waiver in ipairs(waiver_declarations) do
      local id, reason, reviewed_by = waiver.requirement, waiver.reason, waiver.reviewed_by
      if not valid_dotted_id(id)
          or raw_type(reason) ~= "string" or reason == ""
          or raw_type(reviewed_by) ~= "string" or reviewed_by == "" then
        return fail_at("waiver.contract", "waiver requires requirement, reason, and reviewed_by", waiver, waiver.__studio_source)
      end
      if waiver_by_id[id] ~= nil then
        return fail_at("waiver.duplicate", "duplicate waiver: " .. id, waiver, waiver.__studio_source)
      end
      waiver_by_id[id] = { reason = reason, reviewed_by = reviewed_by }
      waiver_sources[id] = waiver.__studio_source
    end

    table.sort(requirement_declarations, function(a, b)
      local a_id = raw_type(a.id) == "string" and a.id or ""
      local b_id = raw_type(b.id) == "string" and b.id or ""
      return a_id < b_id
    end)
    local seen_requirements = {}
    for _, requirement in ipairs(requirement_declarations) do
      local id, severity, check = requirement.id, requirement.severity, requirement.check
      if not valid_dotted_id(id) then
        return fail_at("requirement.id", "requirement id must use stable lowercase segments", requirement, requirement.__studio_source)
      end
      if seen_requirements[id] then
        return fail_at("requirement.duplicate", "duplicate requirement: " .. id, requirement, requirement.__studio_source)
      end
      if severity ~= "warning" and severity ~= "error" then
        return fail_at("requirement.severity", "severity must be warning or error", requirement, requirement.__studio_source)
      end
      seen_requirements[id] = true
      local dependencies = {}
      local query = {}
      function query.level(item_ref, field_ref, level)
        if raw_type(item_ref) ~= "table" or authentic_item_refs[item_ref] ~= true
            or raw_type(field_ref) ~= "table" or authentic_fields[field_ref] ~= true then
          return fail_at("requirement.query", "level requires authentic item and field handles", requirement, requirement.__studio_source)
        end
        if raw_math.type(level) ~= "integer" or level < 1 then
          return fail_at("requirement.query", "level must be a positive integer", requirement, requirement.__studio_source)
        end
        local item, registered_field = registered[item_ref.id], registered_field_by_id[field_ref.id]
        if item == nil or registered_field == nil then
          return fail_at("requirement.query", "requirement handle is not registered", requirement, requirement.__studio_source)
        end
        local rows = raw_type(item.levels) == "table" and item.levels.rows or nil
        local row = raw_type(rows) == "table" and rows[level] or nil
        local value = raw_type(row) == "table" and row[registered_field.member] or nil
        if raw_math.type(value) ~= "integer" then
          return fail_at("requirement.query", "requested level field is unavailable", requirement, requirement.__studio_source)
        end
        dependencies[item_ref.id] = true
        return value
      end
      local function validate_evidence(value, seen, depth, budget)
        budget.count = budget.count + 1
        if budget.count > 1000 or depth > 32 then
          return fail_at("requirement.evidence", "evidence exceeds structural bounds", requirement, requirement.__studio_source)
        end
        local value_type = raw_type(value)
        if value_type == "nil" or value_type == "boolean" or value_type == "string" then return end
        if value_type == "number" then
          if value ~= value or value == raw_math.huge or value == -raw_math.huge then
            return fail_at("requirement.evidence", "evidence numbers must be finite", requirement, requirement.__studio_source)
          end
          return
        end
        if value_type ~= "table" or value.__studio_kind ~= nil then
          return fail_at("requirement.evidence", "evidence must contain only JSON-safe values", requirement, requirement.__studio_source)
        end
        if seen[value] then
          return fail_at("requirement.evidence", "evidence cannot contain cycles", requirement, requirement.__studio_source)
        end
        seen[value] = true
        local key_mode, count, max_index = nil, 0, 0
        for key, child in raw_pairs(value) do
          local key_type = raw_type(key)
          local mode = key_type == "string" and "object" or "array"
          if key_type ~= "string" and (raw_math.type(key) ~= "integer" or key < 1) then
            return fail_at("requirement.evidence", "evidence keys must be strings or contiguous positive integers", requirement, requirement.__studio_source)
          end
          if key_mode ~= nil and key_mode ~= mode then
            return fail_at("requirement.evidence", "evidence cannot mix object and array keys", requirement, requirement.__studio_source)
          end
          key_mode, count = mode, count + 1
          if mode == "array" and key > max_index then max_index = key end
          validate_evidence(child, seen, depth + 1, budget)
        end
        if key_mode == "array" and count ~= max_index then
          return fail_at("requirement.evidence", "evidence arrays must be contiguous", requirement, requirement.__studio_source)
        end
        seen[value] = nil
      end
      local function make_result(pass, expected, actual)
        if raw_type(pass) ~= "boolean" or raw_type(expected) ~= "table" or raw_type(actual) ~= "table" then
          return fail_at("requirement.result", "result requires boolean pass and expected/actual tables", requirement, requirement.__studio_source)
        end
        local budget = { count = 0 }
        validate_evidence(expected, {}, 0, budget)
        validate_evidence(actual, {}, 0, budget)
        return tagged("requirement_result", {
          pass = pass, expected = copy(expected), actual = copy(actual),
        }, requirement.__studio_source)
      end
      local result = check(freeze(query), make_result)
      if raw_type(result) ~= "table" or authentic_requirement_results[result] ~= true then
        return fail_at("requirement.result", "check must return the provided result handle", requirement, requirement.__studio_source)
      end
      for index, expected in ipairs(requirement_upvalues[check]) do
        local _, current = raw_debug.getupvalue(check, index)
        if current ~= expected then
          return fail_at("requirement.mutable_upvalue", "requirement changed a captured upvalue", requirement, requirement.__studio_source)
        end
      end
      local dependency_ids = {}
      for dependency, _ in raw_pairs(dependencies) do dependency_ids[#dependency_ids + 1] = dependency end
      table.sort(dependency_ids)
      local normalized_result = {
        id = id,
        severity = severity,
        status = result.pass and "pass" or "fail",
        evidence = { expected = copy(result.expected), actual = copy(result.actual) },
        dependencies = dependency_ids,
      }
      if waiver_by_id[id] ~= nil then normalized_result.waiver = waiver_by_id[id] end
      requirement_results[#requirement_results + 1] = normalized_result
      requirement_sources[id] = requirement.__studio_source
    end
    for waiver_id, _ in raw_pairs(waiver_by_id) do
      if not seen_requirements[waiver_id] then
        return fail_at("waiver.unknown", "waiver names unknown requirement: " .. waiver_id, nil, waiver_sources[waiver_id])
      end
    end
    local kinds = {}
    for kind, _ in raw_pairs(registered_kinds) do kinds[#kinds + 1] = kind end
    table.sort(kinds)
    return {
      fields = fields,
      field_sources = field_sources,
      items = declarations,
      kinds = kinds,
      requirements = requirement_results,
      requirement_sources = requirement_sources,
      waiver_sources = waiver_sources,
    }
  end

  local function lock_string_surface()
    raw_debug.setmetatable("", {
      __index = function()
        return fail("sandbox.string_surface", "the Lua string library is unavailable")
      end,
      __metatable = false,
    })
  end

  return items, levels, field, requirements, math_view, finalize, freeze, setup_limits,
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


def _output_rows(
    items: list[dict[str, Any]], fields: list[dict[str, Any]], requirements: list[dict[str, Any]],
) -> int:
    rows = len(items) + len(fields) + len(requirements)
    for item in items:
        levels = item.get("levels")
        if isinstance(levels, dict) and isinstance(levels.get("rows"), list):
            rows += len(levels["rows"])
    return rows


def _raw_arithmetic(source: str) -> tuple[str, int] | None:
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
        if source.startswith(("//", "<<", ">>"), index):
            return source[index:index + 2], line
        if char in "+-*/%^#&|" or (char == "~" and not source.startswith("~=", index)):
            return char, line
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
        items, levels, field, requirements, studio_math, finalize, freeze, setup_limits,
        safe_assert, safe_error, lock_string_surface,
    ) = runtime.execute(
        PRELUDE, name="@studio/sandbox.lua", mode="t",
    )(raise_internal)
    builtins = {
        "studio.items": freeze(items),
        "studio.levels": freeze(levels),
        "studio.field": freeze(field),
        "studio.requirements": freeze(requirements),
        "studio.math": studio_math,
    }
    setup_limits(
        int(request.get("instructionLimit", DEFAULT_INSTRUCTION_LIMIT)),
        int(request.get("recursionLimit", DEFAULT_RECURSION_LIMIT)),
    )
    cache: dict[str, Any] = {}
    loading: list[str] = []
    source_texts: dict[str, str] = {}
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
            source_texts[rel] = source
            raw_arithmetic = _raw_arithmetic(source)
            if raw_arithmetic is not None:
                operator, operator_line = raw_arithmetic
                raise _failure(
                    "source.raw_arithmetic",
                    f"raw arithmetic operator {operator!r} is unavailable; use studio.math",
                    file=rel, line=operator_line, path=f"$.modules.{name}",
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
        normalized = _convert(lupa.lua_type, finalize())
    except Exception as error:
        raise _normalize_lua_failure(
            error, fallback, "$.items", {rel for _path, rel in modules.values()},
        ) from error
    if not isinstance(normalized, dict):
        raise _failure("output.shape", "evaluator finalization must return an object")
    normalized_items = normalized.get("items")
    fields = normalized.get("fields")
    field_sources = normalized.get("field_sources")
    kinds = normalized.get("kinds")
    requirement_results = normalized.get("requirements")
    requirement_sources = normalized.get("requirement_sources")
    waiver_sources = normalized.get("waiver_sources")
    if not isinstance(normalized_items, list):
        normalized_items = [] if normalized_items == {} else normalized_items
    if not isinstance(fields, list):
        fields = [] if fields == {} else fields
    if not isinstance(field_sources, dict):
        field_sources = {} if field_sources == [] else field_sources
    if not isinstance(kinds, list):
        kinds = [] if kinds == {} else kinds
    if not isinstance(requirement_results, list):
        requirement_results = [] if requirement_results == {} else requirement_results
    if isinstance(requirement_results, list):
        for requirement_result in requirement_results:
            if isinstance(requirement_result, dict) and requirement_result.get("dependencies") == {}:
                requirement_result["dependencies"] = []
    if not isinstance(requirement_sources, dict):
        requirement_sources = {} if requirement_sources == [] else requirement_sources
    if not isinstance(waiver_sources, dict):
        waiver_sources = {} if waiver_sources == [] else waiver_sources
    sources: dict[str, dict[str, Any]] = {}

    def source_span(source: dict[str, Any]) -> dict[str, Any]:
        file, line = source.get("file"), source.get("line")
        if not isinstance(file, str):
            raise _failure("source.span", "source file must name a loaded module", file=fallback)
        lines = re.split(r"\r\n|\n|\r", source_texts.get(file, ""))
        if not isinstance(line, int) or line < 1 or line > len(lines):
            raise _failure("source.span", "source line is outside loaded module", file=file or fallback)
        snippet = lines[line - 1]
        return {
            **source,
            "end_line": line,
            "end_column": len(snippet) + 1,
            "snippet": snippet,
        }

    for item in normalized_items if isinstance(normalized_items, list) else []:
        if isinstance(item, dict):
            source = item.pop("__studio_source", None)
            item_id = item.get("id")
            if isinstance(item_id, str) and isinstance(source, dict):
                sources[item_id] = source_span(source)
    if isinstance(field_sources, dict):
        enriched_field_sources = {}
        for field_id, source in field_sources.items():
            if not isinstance(source, dict):
                raise _failure("source.span", "field source must be an object", file=fallback)
            enriched_field_sources[field_id] = source_span(source)
        field_sources = enriched_field_sources
    if isinstance(requirement_sources, dict):
        requirement_sources = {
            requirement_id: source_span(source)
            for requirement_id, source in requirement_sources.items()
        }
    if isinstance(waiver_sources, dict):
        waiver_sources = {
            requirement_id: source_span(source)
            for requirement_id, source in waiver_sources.items()
        }
    max_rows = int(request.get("maxOutputRows", DEFAULT_MAX_OUTPUT_ROWS))
    if _output_rows(normalized_items, fields, requirement_results) > max_rows:
        raise _failure(
            "output.row_limit", f"output exceeds {max_rows} rows",
            file=fallback, path="$.items",
        )
    payload = {
        "schema": "items.lua.evaluation.v1",
        "backend": {
            "package": f"lupa@{lupa_package.__version__}",
            "module": BACKEND_MODULE,
            "implementation": runtime.lua_implementation,
            "version": ".".join(map(str, runtime.lua_version)),
        },
        "fields": fields,
        "field_sources": field_sources,
        "items": normalized_items,
        "kinds": kinds,
        "requirements": requirement_results,
        "requirement_sources": requirement_sources,
        "waiver_sources": waiver_sources,
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
