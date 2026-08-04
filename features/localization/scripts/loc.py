#!/usr/bin/env python3
"""Localization codegen: content/loc/strings.json -> loc_strings.gen.{h,c} + loc_keys.gen.json.

    node ai_studio/dev_environment/python_run.mjs features/localization/scripts/loc.py \
        generate --strings content/loc/strings.json --out-dir src/generated

Universal: this script knows nothing about any game, any key vocabulary, or the
jam dump. Everything game-specific is data in strings.json.

The generated C emits one accessor per key with explicitly typed parameters, so
a raw literal or a wrong argument type is a compile error rather than a runtime
surprise. `str` arguments are the one-field `LocStr` wrapper for the same
reason. Idempotent (write_if_changed); key order is sorted, so the generated
diff does not depend on where a machine-run import happened to append.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

KEY_RE = re.compile(r"^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$")
ARG_RE = re.compile(r"^[a-z][a-z0-9_]*$")
LANG_RE = re.compile(r"^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$")

# CLDR category order == loc_plural_cat_t slot order in loc.h.
PLURAL_CATS = ("zero", "one", "two", "few", "many", "other")

# Exactly the categories loc.c's plural_select() can return for this rule --
# no more, no less. A missing form is an error because the runtime would have
# nothing to render; an extra one is an error because it would be copy nobody
# can reach and nobody notices is dead.
#
# Note what that means for the Slavic rules: they have NO `other`. Russian
# selects one/few/many for every integer, and a fractional value cannot occur
# here (a float `plural_on` is required to declare exactly `other`, checked
# below). Requiring `other` for Russian -- the first cut did -- mandated a form
# the runtime never selects, which is the very thing the extra-form rule
# forbids everywhere else.
#
# This table is the Python half of ONE rule whose other half is
# plural_select() in loc.c. features/localization/tests/ walks every rule
# through the generated code so the halves cannot drift apart silently.
PLURAL_RULE_CATS: dict[str, tuple[str, ...]] = {
    "other_only": ("other",),
    "one_other": ("one", "other"),
    "french": ("one", "other"),
    "slavic": ("one", "few", "many"),
    "polish": ("one", "few", "many"),
    "czech": ("one", "few", "other"),
}

PLURAL_RULE_ENUM = {
    "other_only": "LOC_PLURAL_RULE_OTHER_ONLY",
    "one_other": "LOC_PLURAL_RULE_ONE_OTHER",
    "french": "LOC_PLURAL_RULE_FRENCH",
    "slavic": "LOC_PLURAL_RULE_SLAVIC",
    "polish": "LOC_PLURAL_RULE_POLISH",
    "czech": "LOC_PLURAL_RULE_CZECH",
}

# Base language code -> CLDR rule. A code that is not here needs an explicit
# `plural_rule` in its language block; the generator never guesses.
LANG_PLURAL_RULE: dict[str, str] = {}
for _code in ("ja", "zh", "ko", "th", "vi", "id", "ms", "my", "km", "lo"):
    LANG_PLURAL_RULE[_code] = "other_only"
for _code in (
    "en", "de", "es", "it", "nl", "pt", "sv", "da", "nb", "no", "fi", "el",
    "he", "hu", "tr", "ca", "et", "eu", "af", "bg", "sw", "ur", "fa", "az", "ka",
):
    LANG_PLURAL_RULE[_code] = "one_other"
for _code in ("fr", "hy"):
    LANG_PLURAL_RULE[_code] = "french"
for _code in ("ru", "uk", "be", "hr", "sr", "bs"):
    LANG_PLURAL_RULE[_code] = "slavic"
LANG_PLURAL_RULE["pl"] = "polish"
LANG_PLURAL_RULE["cs"] = "czech"
LANG_PLURAL_RULE["sk"] = "czech"

ARG_TYPES: dict[str, tuple[str, str]] = {
    # json type -> (generated C parameter type, loc_arg_kind_t)
    "int": ("int64_t", "LOC_ARG_INT"),
    "int:group": ("int64_t", "LOC_ARG_INT_GROUP"),
    "float:1": ("float", "LOC_ARG_FLOAT1"),
    "float:2": ("float", "LOC_ARG_FLOAT2"),
    "float:g": ("float", "LOC_ARG_FLOAT_G"),
    "str": ("LocStr", "LOC_ARG_STR"),
}

FLOAT_ARG_TYPES = frozenset({"float:1", "float:2", "float:g"})

# Mirrors LOC_MAX_ARGS in loc.h. The generated TU carries a _Static_assert
# against the real macro, so this constant only buys a better message than a
# compiler diagnostic -- it can never be the thing that silently disagrees.
MAX_ARGS = 8

# Newline and tab are the only control characters a UI string may carry.
ALLOWED_CONTROL = frozenset({"\n", "\t"})


class LocError(SystemExit):
    def __init__(self, message: str) -> None:
        super().__init__(f"localization validation: {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise LocError(message)


# ---------------------------------------------------------------------------
# Text: placeholders, escaping, and the runtime template
# ---------------------------------------------------------------------------


def split_text(text: str, where: str) -> list[tuple[bool, str]]:
    """(is_placeholder, value) chunks. Raises on a malformed or unbalanced brace
    -- a stray `{` would otherwise ship to the player verbatim."""
    chunks: list[tuple[bool, str]] = []
    literal: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char == "{":
            if index + 1 < length and text[index + 1] == "{":
                literal.append("{")
                index += 2
                continue
            close = text.find("}", index + 1)
            require(close != -1, f"{where}: unclosed '{{' in {text!r} (write '{{{{' for a literal brace)")
            name = text[index + 1 : close]
            require(
                bool(ARG_RE.match(name)),
                f"{where}: placeholder {{{name}}} in {text!r} is not a valid argument name "
                f"(lower_snake_case; write '{{{{' for a literal brace)",
            )
            if literal:
                chunks.append((False, "".join(literal)))
                literal = []
            chunks.append((True, name))
            index = close + 1
            continue
        if char == "}":
            require(
                index + 1 < length and text[index + 1] == "}",
                f"{where}: stray '}}' in {text!r} (write '}}}}' for a literal brace)",
            )
            literal.append("}")
            index += 2
            continue
        require(
            char >= " " or char in ALLOWED_CONTROL,
            f"{where}: control character U+{ord(char):04X} in {text!r}",
        )
        require(char != "\x7f", f"{where}: control character U+007F in {text!r}")
        literal.append(char)
        index += 1
    if literal:
        chunks.append((False, "".join(literal)))
    return chunks


def collect_placeholders(text: str, where: str) -> set[str]:
    return {value for is_ph, value in split_text(text, where) if is_ph}


def render_template(text: str, arg_order: list[str], where: str) -> str:
    """Emit what the runtime stores.

    With arguments: the interpolation template -- `{name}` -> `{index}`, literal
    braces stay doubled. loc.c walks exactly this grammar, so `%` is literal and
    no format string ever reaches a call site.

    Without arguments: the finished text. A key with no args has no placeholders
    by validation, and loc_get() hands its pointer straight to the caller
    without expanding anything -- so the escapes must already be resolved here or
    a literal brace would reach the player doubled.
    """
    out: list[str] = []
    for is_ph, value in split_text(text, where):
        if is_ph:
            out.append("{" + str(arg_order.index(value)) + "}")
        elif arg_order:
            out.append(value.replace("{", "{{").replace("}", "}}"))
        else:
            out.append(value)
    return "".join(out)


# ---------------------------------------------------------------------------
# C emission helpers
# ---------------------------------------------------------------------------


def c_string(value: str) -> str:
    out: list[str] = []
    for char in value:
        if char == "\\":
            out.append("\\\\")
        elif char == '"':
            out.append('\\"')
        elif char == "\n":
            out.append("\\n")
        elif char == "\t":
            out.append("\\t")
        else:
            out.append(char)
    return '"' + "".join(out) + '"'


def c_comment(value: str) -> str:
    """One safe C-comment line. Keeps the source text greppable from the header."""
    out: list[str] = []
    for char in value:
        if char == "\n":
            out.append("\\n")
        elif char == "\t":
            out.append("\\t")
        elif char == "\r":
            out.append("\\r")
        elif char < " " or char == "\x7f":
            out.append(f"\\u{ord(char):04x}")
        else:
            out.append(char)
    return "".join(out).replace("*/", "* /")


def mangle(key: str) -> str:
    return key.replace(".", "_")


# ---------------------------------------------------------------------------
# Model
# ---------------------------------------------------------------------------


class Lang:
    def __init__(self, code: str, block: dict[str, Any]) -> None:
        self.code = code
        require(bool(LANG_RE.match(code)), f"language code {code!r} must look like 'ru' or 'pt-BR'")
        require(isinstance(block, dict), f"language {code!r} must be an object")
        unknown = set(block) - {"group_separator", "group_min_digits", "plural_rule"}
        require(not unknown, f"language {code!r} has unknown field(s) {sorted(unknown)}")
        separator = block.get("group_separator")
        require(
            isinstance(separator, str) and len(separator) <= 4,
            f"language {code!r} group_separator must be a string of at most 4 bytes, got {separator!r}",
        )
        self.group_separator = separator
        min_digits = block.get("group_min_digits")
        require(
            isinstance(min_digits, int) and not isinstance(min_digits, bool) and 1 <= min_digits <= 18,
            f"language {code!r} group_min_digits must be an integer in [1, 18], got {min_digits!r}",
        )
        self.group_min_digits = min_digits
        rule = block.get("plural_rule")
        if rule is None:
            base = code.split("-", 1)[0]
            rule = LANG_PLURAL_RULE.get(base)
            require(
                rule is not None,
                f"language {code!r} has no built-in CLDR plural rule; declare one explicitly with "
                f"\"plural_rule\": one of {sorted(PLURAL_RULE_CATS)}",
            )
        require(
            rule in PLURAL_RULE_CATS,
            f"language {code!r} plural_rule {rule!r} is unknown (expected one of {sorted(PLURAL_RULE_CATS)})",
        )
        self.plural_rule = rule

    @property
    def cats(self) -> tuple[str, ...]:
        return PLURAL_RULE_CATS[self.plural_rule]


class Entry:
    def __init__(self, key: str, raw: dict[str, Any], langs: list[Lang], fallback: str) -> None:
        self.key = key
        require(KEY_RE.match(key) is not None, f"key {key!r} must match ^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+$")
        require(isinstance(raw, dict), f"key {key!r} must map to an object")

        lang_codes = [lang.code for lang in langs]
        unknown = set(raw) - {"args", "plural_on", "note"} - set(lang_codes)
        require(
            not unknown,
            f"key {key!r} has unknown field(s) {sorted(unknown)} "
            f"(languages declared: {lang_codes})",
        )

        args_raw = raw.get("args", {})
        require(isinstance(args_raw, dict), f"key {key!r} 'args' must be an object")
        self.arg_order: list[str] = []
        self.arg_types: dict[str, str] = {}
        for name, type_name in args_raw.items():
            require(bool(ARG_RE.match(name)), f"key {key!r} argument name {name!r} must be lower_snake_case")
            require(
                type_name in ARG_TYPES,
                f"key {key!r} argument {name!r} has unknown type {type_name!r} "
                f"(expected one of {sorted(ARG_TYPES)})",
            )
            self.arg_order.append(name)
            self.arg_types[name] = type_name
        require(
            len(self.arg_order) <= MAX_ARGS,
            f"key {key!r} declares {len(self.arg_order)} arguments; the runtime renders at most "
            f"{MAX_ARGS} (LOC_MAX_ARGS in features/localization/include/features/localization/loc.h)",
        )

        self.plural_on = raw.get("plural_on")
        if self.plural_on is not None:
            require(
                isinstance(self.plural_on, str) and self.plural_on in self.arg_types,
                f"key {key!r} plural_on {self.plural_on!r} is not a declared argument "
                f"(declared: {self.arg_order})",
            )
        self.is_plural = self.plural_on is not None
        self.plural_is_float = self.is_plural and self.arg_types[self.plural_on] in FLOAT_ARG_TYPES

        # forms[lang_code][category] -> source text
        self.forms: dict[str, dict[str, str]] = {}
        for lang in langs:
            value = raw.get(lang.code)
            if value is None:
                continue
            where = f"key {key!r} language {lang.code!r}"
            if self.is_plural:
                require(
                    isinstance(value, dict),
                    f"{where}: plural_on is set, so the value must be an object of CLDR forms, got a string",
                )
                cats = lang.cats
                present = set(value)
                if self.plural_is_float:
                    require(
                        present == {"other"},
                        f"{where}: plural_on {self.plural_on!r} is a float type, so only 'other' is ever "
                        f"selected -- declare exactly {{\"other\": ...}}, got {sorted(present)}",
                    )
                else:
                    missing = [cat for cat in cats if cat not in present]
                    extra = sorted(present - set(cats))
                    require(
                        not missing,
                        f"{where}: missing plural form(s) {missing} required by the "
                        f"{lang.plural_rule!r} rule (required: {list(cats)})",
                    )
                    require(
                        not extra,
                        f"{where}: plural form(s) {extra} are unreachable under the "
                        f"{lang.plural_rule!r} rule (required: {list(cats)})",
                    )
                forms: dict[str, str] = {}
                for cat, text in value.items():
                    require(cat in PLURAL_CATS, f"{where}: {cat!r} is not a CLDR category {list(PLURAL_CATS)}")
                    require(isinstance(text, str), f"{where}: form {cat!r} must be a string")
                    forms[cat] = text
                self.forms[lang.code] = forms
            else:
                require(
                    isinstance(value, str),
                    f"{where}: no plural_on is declared, so the value must be a string, got an object",
                )
                self.forms[lang.code] = {"other": value}

        require(
            fallback in self.forms,
            f"key {key!r} has no text for the fallback language {fallback!r} -- the fallback language "
            "must be complete",
        )

        # Placeholder parity is checked per LANGUAGE (union over that language's
        # forms), not per form: an English 'one' form may legitimately read
        # "need a coin" as long as some form in English still uses {n}.
        per_lang: dict[str, set[str]] = {}
        used: set[str] = set()
        for code, forms in self.forms.items():
            names: set[str] = set()
            for cat, text in forms.items():
                names |= collect_placeholders(text, f"key {key!r} language {code!r} form {cat!r}")
            per_lang[code] = names
            used |= names

        undeclared = sorted(used - set(self.arg_types))
        require(
            not undeclared,
            f"key {key!r} uses placeholder(s) {undeclared} that are not declared in 'args' "
            f"(declared: {self.arg_order})",
        )
        unused = sorted(set(self.arg_types) - used)
        require(
            not unused,
            f"key {key!r} declares argument(s) {unused} that no form uses",
        )
        reference = per_lang[fallback]
        for code, names in sorted(per_lang.items()):
            if names != reference:
                only_here = sorted(names - reference)
                only_there = sorted(reference - names)
                raise LocError(
                    f"key {key!r}: placeholder mismatch between {fallback!r} and {code!r} -- "
                    f"{code!r} adds {only_here}, misses {only_there}"
                )

        self.ident = mangle(key)

    def missing_langs(self, langs: list[Lang]) -> list[str]:
        return [lang.code for lang in langs if lang.code not in self.forms]

    def comment(self, langs: list[Lang]) -> str:
        parts: list[str] = []
        for lang in langs:
            forms = self.forms.get(lang.code)
            if not forms:
                continue
            if self.is_plural:
                shown = forms.get("other") or next(iter(forms.values()))
                parts.append(f'{lang.code} "{c_comment(shown)}" (+plural)')
            else:
                parts.append(f'{lang.code} "{c_comment(forms["other"])}"')
        return "  ".join(parts)

    def signature(self) -> str:
        if not self.arg_order:
            return "void"
        return ", ".join(f"{ARG_TYPES[self.arg_types[name]][0]} {name}" for name in self.arg_order)


class Table:
    def __init__(self, doc: Any, source: str) -> None:
        require(isinstance(doc, dict), "document must be an object")
        unknown = set(doc) - {"languages", "fallback", "strings", "$schema"}
        require(not unknown, f"document has unknown top-level field(s) {sorted(unknown)}")

        languages = doc.get("languages")
        require(isinstance(languages, dict) and languages, "'languages' must be a non-empty object")
        self.langs = [Lang(code, block) for code, block in languages.items()]

        fallback = doc.get("fallback")
        require(
            isinstance(fallback, str) and fallback in languages,
            f"'fallback' must name a declared language (declared: {list(languages)}), got {fallback!r}",
        )
        self.fallback = fallback
        self.fallback_index = [lang.code for lang in self.langs].index(fallback)

        strings = doc.get("strings")
        require(isinstance(strings, dict), "'strings' must be an object")
        self.entries = [Entry(key, raw, self.langs, fallback) for key, raw in sorted(strings.items())]

        seen: dict[str, str] = {}
        for entry in self.entries:
            previous = seen.get(entry.ident)
            require(
                previous is None,
                f"keys {previous!r} and {entry.key!r} both mangle to the C identifier {entry.ident!r} "
                "-- rename one",
            )
            seen[entry.ident] = entry.key

        self.source = source
        self.warnings: list[str] = []
        for entry in self.entries:
            for code in entry.missing_langs(self.langs):
                self.warnings.append(f"no {code!r} translation for {entry.key!r} (falls back to {fallback!r})")

    @property
    def key0(self) -> list[Entry]:
        return [entry for entry in self.entries if not entry.arg_order]


# ---------------------------------------------------------------------------
# Codegen
# ---------------------------------------------------------------------------


def render_header(table: Table) -> str:
    provenance = c_comment(table.source)
    lines = [
        "#ifndef LOC_STRINGS_GEN_H",
        "#define LOC_STRINGS_GEN_H",
        "",
        "/* Generated by features/localization/scripts/loc.py",
        f"   from {provenance}. Do not edit by hand. */",
        "",
        "#include <stdint.h>",
        "",
        '#include "features/localization/loc.h"',
        "",
        "typedef enum LocLang {",
    ]
    for index, lang in enumerate(table.langs):
        lines.append(f"    LOC_LANG_{lang.code.upper().replace('-', '_')} = {index},")
    lines.append(f"    LOC_LANG_COUNT = {len(table.langs)}")
    lines.append("} LocLang;")
    lines.append("")
    lines.append(f"#define LOC_FALLBACK_LANG LOC_LANG_{table.fallback.upper().replace('-', '_')}")
    lines.append("")
    lines.append("/* The corpus's widest key, checked against the runtime's cap so a lowered")
    lines.append("   LOC_MAX_ARGS breaks the build instead of degrading a string at runtime.")
    lines.append("   The cap itself lives only in loc.h. */")
    widest = max((len(entry.arg_order) for entry in table.entries), default=0)
    lines.append(f"#define LOC_GENERATED_MAX_ARGS {widest}")
    lines.append(
        '_Static_assert(LOC_GENERATED_MAX_ARGS <= LOC_MAX_ARGS, "corpus needs more arguments than loc.h allows");'
    )
    lines.append("")
    lines.append("/* Every key. The enumerator IS the table index. */")
    lines.append("typedef enum LocKey {")
    for index, entry in enumerate(table.entries):
        lines.append(f"    LOC_{entry.ident.upper()} = {index}, /* {entry.comment(table.langs)} */")
    lines.append(f"    LOC_KEY_COUNT = {len(table.entries)}")
    lines.append("} LocKey;")
    lines.append("")
    lines.append("/* Zero-argument keys -- the only ones loc_by_key may take, and the only")
    lines.append("   ones a data table may carry. A key with arguments would paint its own")
    lines.append("   placeholders on screen, so it is not representable here. */")
    lines.append("typedef enum LocKey0 {")
    for index, entry in enumerate(table.key0):
        lines.append(f"    LOC0_{entry.ident.upper()} = {index}, /* {entry.comment(table.langs)} */")
    lines.append(f"    LOC0_COUNT = {len(table.key0)}")
    lines.append("} LocKey0;")
    lines.append("")
    lines.append("/* Binds the generated table into the runtime. Call once at startup,")
    lines.append("   before any accessor. */")
    lines.append("void loc_init(void);")
    lines.append("")
    lines.append("LocStr loc_by_key(LocKey0 key);")
    lines.append("")
    lines.append("static inline void loc_set_lang(LocLang lang) { loc_set_lang_index((int)lang); }")
    lines.append("static inline LocLang loc_lang(void) { return (LocLang)loc_lang_index(); }")
    lines.append("")
    for entry in table.entries:
        lines.append(f"/* {entry.comment(table.langs)} */")
        lines.append(f"LocStr loc_{entry.ident}({entry.signature()});")
    lines.append("")
    lines.append("#endif /* LOC_STRINGS_GEN_H */")
    lines.append("")
    return "\n".join(lines)


def render_source(table: Table) -> str:
    provenance = c_comment(table.source)
    lines = [
        '#include "loc_strings.gen.h"',
        "",
        "#include <stddef.h> /* NULL */",
        "",
        "/* Generated by features/localization/scripts/loc.py",
        f"   from {provenance}. Do not edit by hand. */",
        "",
        "static const loc_lang_def_t k_loc_langs[] = {",
    ]
    for lang in table.langs:
        lines.append(
            f"    {{ {c_string(lang.code)}, {c_string(lang.group_separator)}, "
            f"{lang.group_min_digits}, {PLURAL_RULE_ENUM[lang.plural_rule]} }},"
        )
    lines.append("};")
    lines.append("")

    for entry in table.entries:
        symbol = f"k_loc_forms_{entry.ident}"
        lines.append(f"/* {entry.key} */")
        lines.append(f"static const char *const {symbol}[] = {{")
        for lang in table.langs:
            forms = entry.forms.get(lang.code)
            if entry.is_plural:
                lines.append(f"    /* {lang.code} */")
                for cat in PLURAL_CATS:
                    text = forms.get(cat) if forms else None
                    if text is None:
                        lines.append(f"    NULL, /* {cat} */")
                    else:
                        rendered = render_template(text, entry.arg_order, f"key {entry.key!r}")
                        lines.append(f"    {c_string(rendered)}, /* {cat} */")
            else:
                text = forms.get("other") if forms else None
                if text is None:
                    lines.append(f"    NULL, /* {lang.code} */")
                else:
                    rendered = render_template(text, entry.arg_order, f"key {entry.key!r}")
                    lines.append(f"    {c_string(rendered)}, /* {lang.code} */")
        lines.append("};")
        lines.append("")

    lines.append("static const loc_string_def_t k_loc_strings[] = {")
    for entry in table.entries:
        plural = "true" if entry.is_plural else "false"
        lines.append(f"    {{ {c_string(entry.key)}, k_loc_forms_{entry.ident}, {plural} }},")
    lines.append("};")
    lines.append("")
    lines.append("/* One bit per key: the fallback warning fires once per key per session.")
    lines.append("   The UI rebuilds every frame, so an undeduplicated warning would emit")
    lines.append("   sixty lines a second. */")
    lines.append("static uint8_t s_loc_fallback_logged[(LOC_KEY_COUNT + 7) / 8];")
    lines.append("")
    lines.append("static const loc_table_t k_loc_table = {")
    lines.append("    k_loc_langs,")
    lines.append("    LOC_LANG_COUNT,")
    lines.append("    k_loc_strings,")
    lines.append("    LOC_KEY_COUNT,")
    lines.append("    LOC_FALLBACK_LANG,")
    lines.append("    s_loc_fallback_logged,")
    lines.append("    sizeof s_loc_fallback_logged,")
    lines.append("};")
    lines.append("")
    lines.append("void loc_init(void) { loc_bind_table(&k_loc_table); }")
    lines.append("")

    if table.key0:
        lines.append("static const uint16_t k_loc_key0_index[] = {")
        for entry in table.key0:
            lines.append(f"    LOC_{entry.ident.upper()},")
        lines.append("};")
        lines.append("")
        lines.append("LocStr loc_by_key(LocKey0 key) {")
        lines.append(
            '    NT_ASSERT((int)key >= 0 && (int)key < LOC0_COUNT && "loc_by_key: key out of range");'
        )
        lines.append("    return loc_get((int)k_loc_key0_index[key]);")
        lines.append("}")
    else:
        lines.append("/* The corpus has no zero-argument keys, so nothing is addressable by key. */")
        lines.append("LocStr loc_by_key(LocKey0 key) {")
        lines.append("    (void)key;")
        lines.append('    NT_ASSERT(false && "loc_by_key: the corpus declares no zero-argument keys");')
        lines.append('    return loc_raw("");')
        lines.append("}")
    lines.append("")

    for entry in table.entries:
        lines.append(f"LocStr loc_{entry.ident}({entry.signature()}) {{")
        if not entry.arg_order:
            lines.append(f"    return loc_get(LOC_{entry.ident.upper()});")
        else:
            lines.append(f"    const loc_arg_t args[{len(entry.arg_order)}] = {{")
            for name in entry.arg_order:
                type_name = entry.arg_types[name]
                kind = ARG_TYPES[type_name][1]
                if type_name == "str":
                    lines.append(f"        {{ {kind}, 0, 0.0, {name}.s }},")
                elif type_name in FLOAT_ARG_TYPES:
                    lines.append(f"        {{ {kind}, 0, (double){name}, NULL }},")
                else:
                    lines.append(f"        {{ {kind}, {name}, 0.0, NULL }},")
            lines.append("    };")
            plural_index = entry.arg_order.index(entry.plural_on) if entry.is_plural else -1
            lines.append(
                f"    return loc_format(LOC_{entry.ident.upper()}, {plural_index}, args, "
                f"{len(entry.arg_order)});"
            )
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


def render_index(table: Table) -> str:
    keys: list[dict[str, Any]] = []
    key0_ids = {entry.key: index for index, entry in enumerate(table.key0)}
    for index, entry in enumerate(table.entries):
        keys.append(
            {
                "key": entry.key,
                "ident": f"loc_{entry.ident}",
                "enum": f"LOC_{entry.ident.upper()}",
                "enum0": f"LOC0_{entry.ident.upper()}" if entry.key in key0_ids else None,
                "index": index,
                "args": [{"name": name, "type": entry.arg_types[name]} for name in entry.arg_order],
                "plural_on": entry.plural_on,
                "languages": sorted(entry.forms),
            }
        )
    doc = {
        "schema": "localization.keys.v1",
        "source": table.source,
        "fallback": table.fallback,
        "languages": [lang.code for lang in table.langs],
        "keys": keys,
    }
    return json.dumps(doc, ensure_ascii=False, indent=2, sort_keys=False) + "\n"


def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_name(path.name + ".tmp")  # atomic: a crash mid-write leaves the old file intact
    tmp_path.write_text(text, encoding="utf-8", newline="\n")
    os.replace(tmp_path, path)
    return True


SCHEMA_PATH = Path(__file__).resolve().parent.parent / "schema" / "strings.schema.json"


def validate_against_schema(doc: Any) -> None:
    """Shape pass: types, enums, patterns, caps -- everything expressible as a
    document grammar. The semantic pass below owns what a grammar cannot say
    (placeholder parity, per-language plural sets, fallback completeness,
    identifier collisions). Split that way the two do not overlap, so neither
    can quietly contradict the other; test_schema_matches_generator pins the
    handful of values that appear in both."""
    try:
        import jsonschema
    except ImportError as exc:  # pragma: no cover - the studio venv ships it
        raise LocError(
            "jsonschema is missing from the studio venv; run "
            "node ai_studio/dev_environment/python_setup.mjs"
        ) from exc
    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    validator = jsonschema.Draft7Validator(schema)
    errors = sorted(validator.iter_errors(doc), key=lambda e: list(e.absolute_path))
    if errors:
        first = errors[0]
        where = "/".join(str(part) for part in first.absolute_path) or "<document>"
        raise LocError(f"{where}: {first.message} (strings.schema.json)")


def load_table(strings_path: Path) -> Table:
    try:
        with strings_path.open("r", encoding="utf-8") as handle:
            doc = json.load(handle)
    except json.JSONDecodeError as exc:
        raise LocError(f"{strings_path}: {exc}") from exc
    validate_against_schema(doc)
    return Table(doc, strings_path.as_posix())


def cmd_generate(args: argparse.Namespace) -> int:
    strings_path = Path(args.strings).resolve()
    out_dir = Path(args.out_dir).resolve()
    source_label = args.source_label or strings_path.name
    table = load_table(strings_path)
    table.source = source_label

    for warning in table.warnings:
        print(f"loc: warning: {warning}", file=sys.stderr)

    changed: list[Path] = []
    index_path = out_dir / "loc_keys.gen.json"
    if write_if_changed(index_path, render_index(table)):
        changed.append(index_path)
    if not args.index_only:
        header_path = out_dir / "loc_strings.gen.h"
        source_path = out_dir / "loc_strings.gen.c"
        if write_if_changed(header_path, render_header(table)):
            changed.append(header_path)
        if write_if_changed(source_path, render_source(table)):
            changed.append(source_path)
    if changed:
        print(f"loc generate: wrote {', '.join(str(path) for path in changed)}", file=sys.stderr)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="loc.py", description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    generate = sub.add_parser("generate", help="strings.json -> loc_strings.gen.{h,c} + loc_keys.gen.json")
    generate.add_argument("--strings", required=True, help="Path to the game's content/loc/strings.json.")
    generate.add_argument("--out-dir", required=True, help="Directory for the generated files.")
    generate.add_argument(
        "--index-only",
        action="store_true",
        help="Emit only loc_keys.gen.json (what a key-consuming port tool needs).",
    )
    generate.add_argument(
        "--source-label",
        default=None,
        help="Provenance string baked into the generated comments (defaults to the file name).",
    )
    generate.set_defaults(func=cmd_generate)

    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
