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

# ---------------------------------------------------------------------------
# Plural rules, ported verbatim from the lead's Defold project
# (C:/projects/defold-localization/localization/i18n/plural.lua) -- the
# reference implementation this studio already ships in a released game. Its
# 24 partitions and its locale lists are carried over unchanged, including
# where it takes an older, simpler CLDR snapshot than cldr-latest (French has
# no `many` at 10^6; Portuguese and Hebrew are plain one/other). Matching the
# reference is the point: it is the spec, not an approximation of one.
#
# ONE deliberate divergence. The reference selects on the raw number, so
# French 1.5 is `one`. Here a fractional value always takes `other` (ratified:
# a `plural_on` bound to a float must declare exactly `other`), so these
# functions only ever see integers. Every isInteger guard below is therefore a
# no-op that is kept for diffability against the reference.
#
# `plural_select()` in loc.c is the C half of these. It is not trusted to
# agree: `loc.py plural-cases` emits every (rule, n) -> category pair from THIS
# table as a C array, and test_loc_e2e walks it through the compiled
# loc_plural_category(). The halves are machine-checked, not eyeballed.
# ---------------------------------------------------------------------------


def _one_other(n: int) -> str:
    return "one" if n == 1 else "other"


def _one_zero_other(n: int) -> str:
    return "one" if n in (0, 1) else "other"


def _arabic(n: int) -> str:
    n100 = n % 100
    if n == 0:
        return "zero"
    if n == 1:
        return "one"
    if n == 2:
        return "two"
    if 3 <= n100 <= 10:
        return "few"
    if 11 <= n100 <= 99:
        return "many"
    return "other"


def _other_only(n: int) -> str:
    return "other"


def _slavic(n: int) -> str:
    n10, n100 = n % 10, n % 100
    if n10 == 1 and n100 != 11:
        return "one"
    if 2 <= n10 <= 4 and not 12 <= n100 <= 14:
        return "few"
    if n10 == 0 or 5 <= n10 <= 9 or 11 <= n100 <= 14:
        return "many"
    return "other"


def _breton(n: int) -> str:
    n10, n100 = n % 10, n % 100
    if n10 == 1 and n100 not in (11, 71, 91):
        return "one"
    if n10 == 2 and n100 not in (12, 72, 92):
        return "two"
    if n10 in (3, 4, 9) and not (10 <= n100 <= 19 or 70 <= n100 <= 79 or 90 <= n100 <= 99):
        return "few"
    if n != 0 and n % 1000000 == 0:
        return "many"
    return "other"


def _czech(n: int) -> str:
    if n == 1:
        return "one"
    return "few" if n in (2, 3, 4) else "other"


def _welsh(n: int) -> str:
    return {0: "zero", 1: "one", 2: "two", 3: "few", 6: "many"}.get(n, "other")


def _french(n: int) -> str:
    return "one" if 0 <= n < 2 else "other"


def _irish(n: int) -> str:
    if n == 1:
        return "one"
    if n == 2:
        return "two"
    if 3 <= n <= 6:
        return "few"
    return "many" if 7 <= n <= 10 else "other"


def _gaelic(n: int) -> str:
    if n in (1, 11):
        return "one"
    if n in (2, 12):
        return "two"
    return "few" if (3 <= n <= 10 or 13 <= n <= 19) else "other"


def _manx(n: int) -> str:
    n10 = n % 10
    return "one" if (n10 in (1, 2) or n % 20 == 0) else "other"


def _one_two_other(n: int) -> str:
    if n == 1:
        return "one"
    return "two" if n == 2 else "other"


def _zero_one_other(n: int) -> str:
    if n == 0:
        return "zero"
    return "one" if n == 1 else "other"


def _langi(n: int) -> str:
    if n == 0:
        return "zero"
    return "one" if 0 < n < 2 else "other"


def _lithuanian(n: int) -> str:
    if 11 <= n % 100 <= 19:
        return "other"
    n10 = n % 10
    if n10 == 1:
        return "one"
    return "few" if 2 <= n10 <= 9 else "other"


def _latvian(n: int) -> str:
    if n == 0:
        return "zero"
    return "one" if (n % 10 == 1 and n % 100 != 11) else "other"


def _macedonian(n: int) -> str:
    return "one" if (n % 10 == 1 and n != 11) else "other"


def _romanian(n: int) -> str:
    if n == 1:
        return "one"
    return "few" if (n == 0 or (n != 1 and 1 <= n % 100 <= 19)) else "other"


def _maltese(n: int) -> str:
    if n == 1:
        return "one"
    n100 = n % 100
    if n == 0 or 2 <= n100 <= 10:
        return "few"
    return "many" if 11 <= n100 <= 19 else "other"


def _polish(n: int) -> str:
    if n == 1:
        return "one"
    n10, n100 = n % 10, n % 100
    if 2 <= n10 <= 4 and not 12 <= n100 <= 14:
        return "few"
    if n10 in (0, 1) or 5 <= n10 <= 9 or 12 <= n100 <= 14:
        return "many"
    return "other"


def _tachelhit(n: int) -> str:
    return "one" if n in (0, 1) else "other"


def _slovenian(n: int) -> str:
    n100 = n % 100
    if n100 == 1:
        return "one"
    if n100 == 2:
        return "two"
    return "few" if n100 in (3, 4) else "other"


def _tamazight(n: int) -> str:
    return "one" if (n == 0 or n == 1 or 11 <= n <= 99) else "other"


# rule name -> (selector, C enumerator, locales). Order fixes the generated
# enum, so it is append-only.
PLURAL_RULES: dict[str, tuple[Any, str, tuple[str, ...]]] = {
    "one_other": (_one_other, "LOC_PLURAL_RULE_ONE_OTHER", (
        "af", "asa", "bem", "bez", "bg", "bn", "brx", "ca", "cgg", "chr", "da", "de", "dv", "ee",
        "el", "en", "eo", "es", "et", "eu", "fi", "fo", "fur", "fy", "gl", "gsw", "gu", "ha", "haw",
        "he", "is", "it", "jmc", "kaj", "kcg", "kk", "kl", "ksb", "ku", "lb", "lg", "mas", "ml",
        "mn", "mr", "nah", "nb", "nd", "ne", "nl", "nn", "no", "nr", "ny", "nyn", "om", "or", "pa",
        "pap", "ps", "pt", "rm", "rof", "rwk", "saq", "seh", "sn", "so", "sq", "ss", "ssy", "st",
        "sv", "sw", "syr", "ta", "te", "teo", "tig", "tk", "tn", "ts", "ur", "ve", "vun", "wae",
        "xh", "xog", "zu",
    )),
    "one_zero_other": (_one_zero_other, "LOC_PLURAL_RULE_ONE_ZERO_OTHER", (
        "ak", "am", "bh", "fil", "guw", "hi", "ln", "mg", "nso", "ti", "tl", "wa",
    )),
    "arabic": (_arabic, "LOC_PLURAL_RULE_ARABIC", ("ar",)),
    "other_only": (_other_only, "LOC_PLURAL_RULE_OTHER_ONLY", (
        "az", "bm", "bo", "dz", "fa", "hu", "id", "ig", "ii", "ja", "jv", "ka", "kde", "kea", "km",
        "kn", "ko", "lo", "ms", "my", "root", "sah", "ses", "sg", "th", "to", "tr", "vi", "wo",
        "yo", "zh",
    )),
    "slavic": (_slavic, "LOC_PLURAL_RULE_SLAVIC", ("be", "bs", "hr", "ru", "sh", "sr", "uk")),
    "breton": (_breton, "LOC_PLURAL_RULE_BRETON", ("br",)),
    "czech": (_czech, "LOC_PLURAL_RULE_CZECH", ("cs", "cz", "sk")),
    "welsh": (_welsh, "LOC_PLURAL_RULE_WELSH", ("cy",)),
    "french": (_french, "LOC_PLURAL_RULE_FRENCH", ("ff", "fr", "kab")),
    "irish": (_irish, "LOC_PLURAL_RULE_IRISH", ("ga",)),
    "gaelic": (_gaelic, "LOC_PLURAL_RULE_GAELIC", ("gd",)),
    "manx": (_manx, "LOC_PLURAL_RULE_MANX", ("gv",)),
    "one_two_other": (_one_two_other, "LOC_PLURAL_RULE_ONE_TWO_OTHER", (
        "iu", "kw", "naq", "se", "sma", "smi", "smj", "smn", "sms",
    )),
    "zero_one_other": (_zero_one_other, "LOC_PLURAL_RULE_ZERO_ONE_OTHER", ("ksh",)),
    "langi": (_langi, "LOC_PLURAL_RULE_LANGI", ("lag",)),
    "lithuanian": (_lithuanian, "LOC_PLURAL_RULE_LITHUANIAN", ("lt",)),
    "latvian": (_latvian, "LOC_PLURAL_RULE_LATVIAN", ("lv",)),
    "macedonian": (_macedonian, "LOC_PLURAL_RULE_MACEDONIAN", ("mk",)),
    "romanian": (_romanian, "LOC_PLURAL_RULE_ROMANIAN", ("mo", "ro")),
    "maltese": (_maltese, "LOC_PLURAL_RULE_MALTESE", ("mt",)),
    "polish": (_polish, "LOC_PLURAL_RULE_POLISH", ("pl",)),
    "tachelhit": (_tachelhit, "LOC_PLURAL_RULE_TACHELHIT", ("shi",)),
    "slovenian": (_slovenian, "LOC_PLURAL_RULE_SLOVENIAN", ("sl",)),
    "tamazight": (_tamazight, "LOC_PLURAL_RULE_TAMAZIGHT", ("tzm",)),
}

# Every n a rule is exercised over, for deriving its category set and for the
# generated cross-check. 0..399 walks four full periods of every n%100 and n%20
# window the table uses; the sparse tail catches a rule that is accidentally
# periodic in 1000, and the millions reach Breton's `many`.
PLURAL_PROBE = tuple(range(0, 400)) + (
    999, 1000, 1001, 1011, 1012, 1021, 1100, 1111, 1112, 2000, 2001, 2011, 2022,
    10**6, 10**6 + 1, 2 * 10**6, 3 * 10**6,
)

# DERIVED, never hand-written: exactly the categories a rule can return over
# the integers. A missing form is an error because the runtime would have
# nothing to render; an extra one is an error because it is copy nothing can
# reach. Deriving it means "which forms are required" can no longer disagree
# with the arithmetic that selects them.
PLURAL_RULE_CATS: dict[str, tuple[str, ...]] = {
    name: tuple(cat for cat in PLURAL_CATS if any(fn(n) == cat for n in PLURAL_PROBE))
    for name, (fn, _enum, _locales) in PLURAL_RULES.items()
}

PLURAL_RULE_ENUM = {name: enum for name, (_fn, enum, _locales) in PLURAL_RULES.items()}

# Base language code -> rule. A code that is not here needs an explicit
# `plural_rule` in its language block; the generator never guesses.
LANG_PLURAL_RULE: dict[str, str] = {
    code: name for name, (_fn, _enum, locales) in PLURAL_RULES.items() for code in locales
}

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
        elif char == "?":
            # `??!` and friends are trigraphs: -Wtrigraphs is an error under the
            # project's -Werror, and on a toolchain that still honours them the
            # text is silently rewritten instead. Ordinary copy contains "??!".
            out.append("\\?")
        elif char < " " or char == "\x7f":
            out.append(f"\\{ord(char):03o}")
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
    # Both directions: `*/` ends the comment early, `/*` is -Wcomment under -Werror.
    return "".join(out).replace("*/", "* /").replace("/*", "/ *")


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
        unknown = set(block) - {"group_separator", "group_min_digits", "plural_rule", "alphabet"}
        require(not unknown, f"language {code!r} has unknown field(s) {sorted(unknown)}")
        alphabet = block.get("alphabet", "")
        require(
            isinstance(alphabet, str),
            f"language {code!r} alphabet must be a string, got {alphabet!r}",
        )
        self.alphabet = alphabet
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
            # The runtime selects a category from a NUMBER. Bound to anything
            # else it takes `other` for every value -- which for a rule that has
            # no `other` (slavic, polish) silently renders one fixed form
            # forever, while the generator demands the full set the rule can
            # select. The two halves cancel out into wrong copy with no warning.
            require(
                self.arg_types[self.plural_on] != "str",
                f"key {key!r} plural_on {self.plural_on!r} is a 'str'; a plural form is selected "
                "by a number, so plural_on must name an int, int:group, or float argument",
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

        # A key does not only collide with other keys: it also lands in the same
        # namespace as loc.h and this generator's own output. `lang.ru` -> the
        # enumerator LOC_LANG_RU, which the language block already defines --
        # and a language-picker screen is exactly where someone writes it.
        reserved = self.reserved_names()
        for entry in self.entries:
            for produced, kind in (
                (f"LOC_{entry.ident.upper()}", "enumerator"),
                (f"LOC0_{entry.ident.upper()}", "enumerator"),
                (f"loc_{entry.ident}", "function"),
            ):
                require(
                    produced not in reserved,
                    f"key {entry.key!r} would emit the {kind} {produced!r}, which the runtime or "
                    "the generated table already defines -- rename the key",
                )

        self.source = source
        self.warnings: list[str] = []
        for entry in self.entries:
            for code in entry.missing_langs(self.langs):
                self.warnings.append(f"no {code!r} translation for {entry.key!r} (falls back to {fallback!r})")

    def reserved_names(self) -> set[str]:
        names = {
            "LOC_KEY_COUNT", "LOC_LANG_COUNT", "LOC0_COUNT", "LOC_FALLBACK_LANG",
            "LOC_MAX_ARGS", "LOC_NUMBER_BUF", "LOC_GENERATED_MAX_ARGS",
            "LOC_PLURAL_CAT_COUNT", "LOC_PLURAL_RULE_COUNT",
            "loc_init", "loc_by_key", "loc_set_lang", "loc_lang", "loc_raw",
            "loc_get", "loc_format", "loc_bind_table", "loc_group_i64", "loc_number",
            "loc_set_lang_index", "loc_lang_index", "loc_lang_count", "loc_lang_code",
            "loc_lang_from_code", "loc_fallback_log_count", "loc_plural_category",
        }
        names |= {f"LOC_PLURAL_{cat.upper()}" for cat in PLURAL_CATS}
        names |= set(PLURAL_RULE_ENUM.values())
        names |= {
            "LOC_ARG_INT", "LOC_ARG_INT_GROUP", "LOC_ARG_FLOAT1", "LOC_ARG_FLOAT2",
            "LOC_ARG_FLOAT_G", "LOC_ARG_STR",
        }
        names |= {f"LOC_LANG_{lang.code.upper().replace('-', '_')}" for lang in self.langs}
        return names

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
    lines.append("/* A plural row is one slot per CLDR category, emitted below in the order of")
    lines.append("   PLURAL_CATS and read back as forms[row + (int)cat]. So BOTH the count and")
    lines.append("   the ORDER are load-bearing, and both are pinned here:")
    lines.append("     - a category in loc_plural_cat_t but not in PLURAL_CATS leaves every row a")
    lines.append("       slot short and the runtime indexes past the end of it;")
    lines.append("     - a REORDER rotates every row instead, which the (rule, n) cross-check")
    lines.append("       cannot catch because it matches categories by NAME. Every plural key")
    lines.append("       would render the wrong grammatical form with the whole suite green. */")
    lines.append(
        f"_Static_assert(LOC_PLURAL_CAT_COUNT == {len(PLURAL_CATS)}, "
        '"loc.py emits a different number of plural slots than loc_plural_cat_t declares");'
    )
    for index, cat in enumerate(PLURAL_CATS):
        lines.append(
            f"_Static_assert(LOC_PLURAL_{cat.upper()} == {index}, "
            f'"loc.py writes the {cat} form at slot {index}; loc_plural_cat_t disagrees");'
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


def render_plural_cases() -> str:
    """Every (rule, n) -> category pair the Python table declares, as a C array.

    This is the whole defence against the two halves drifting: loc.c's
    loc_plural_category() is a hand transcription of the same 24 partitions,
    and test_loc_e2e walks this array through the compiled function. Neither
    half is trusted; they are compared."""
    cat_enum = {
        "zero": "LOC_PLURAL_ZERO",
        "one": "LOC_PLURAL_ONE",
        "two": "LOC_PLURAL_TWO",
        "few": "LOC_PLURAL_FEW",
        "many": "LOC_PLURAL_MANY",
        "other": "LOC_PLURAL_OTHER",
    }
    lines = [
        "/* Generated by features/localization/scripts/loc.py plural-cases.",
        "   Do not edit by hand. */",
        "",
        '#include "loc_plural_cases.gen.h"',
        "",
        "const loc_plural_case_t k_loc_plural_cases[] = {",
    ]
    count = 0
    for name, (fn, enum, _locales) in PLURAL_RULES.items():
        lines.append(f"    /* {name} */")
        for probe in PLURAL_PROBE:
            lines.append(f"    {{ {enum}, {probe}LL, {cat_enum[fn(probe)]} }},")
            count += 1
        # Negative magnitudes take the same partition as their absolute value.
        for probe in (1, 2, 5, 11, 21, 101):
            lines.append(f"    {{ {enum}, -{probe}LL, {cat_enum[fn(probe)]} }},")
            count += 1
    lines.append("};")
    lines.append("")
    lines.append(f"const int k_loc_plural_case_count = {count};")
    lines.append("")
    return "\n".join(lines)


def render_plural_cases_header() -> str:
    return "\n".join(
        [
            "#ifndef LOC_PLURAL_CASES_GEN_H",
            "#define LOC_PLURAL_CASES_GEN_H",
            "",
            "/* Generated by features/localization/scripts/loc.py plural-cases.",
            "   Do not edit by hand. */",
            "",
            "#include <stdint.h>",
            "",
            '#include "features/localization/loc.h"',
            "",
            "typedef struct loc_plural_case {",
            "    loc_plural_rule_t rule;",
            "    int64_t n;",
            "    loc_plural_cat_t expected;",
            "} loc_plural_case_t;",
            "",
            "extern const loc_plural_case_t k_loc_plural_cases[];",
            "extern const int k_loc_plural_case_count;",
            "",
            "#endif /* LOC_PLURAL_CASES_GEN_H */",
            "",
        ]
    )


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


# ---------------------------------------------------------------------------
# Charset: which codepoints the font atlas has to carry
# ---------------------------------------------------------------------------
#
# The consumer packs NT_CHARSET_ASCII plus what this emits, so only the
# non-ASCII half is generated. ASCII is unconditional on purpose: numbers,
# fallback markers and debug text are ASCII and never appear in the corpus.
#
# Nothing outside the corpus may render non-ASCII text. That is not a wish, it
# is what makes this generated set complete -- a literal drawn from C would have
# no way to reach the charset, and would ship as a blank box. The rule is the
# same one the whole feature exists for: player-visible text lives in
# strings.json.


def corpus_codepoints(table: Table) -> list[int]:
    """Every distinct codepoint the runtime can hand to a text widget, ASCII
    excluded. Four sources, and the last two exist because the first two were
    not enough:

    1. entry.forms -- every language, every plural form. The copy itself.

    2. The language block's number-formatting MARKS. Easy to forget because
       they are metadata rather than copy, but `loc_group_i64` memcpy's
       group_separator straight into drawn text. A language declaring U+00A0
       (the typographically correct Russian thousands separator, and the
       obvious "fix" for the plain space in use today) would otherwise tofu
       every price on screen with no build error and no failing test.

    3. CASE CLOSURE. If a character is in the corpus, so is its other case.
       Costs nothing, needs no table, and kills a whole failure class: the ru
       copy is authored already-uppercase (ЗАКРЫТЬ, НАСТРОЙКИ), so any style
       flag or `toupper` pass over corpus text would emit Ё, Ш, Ю -- none of
       which the corpus contains in that case.

    4. The language's declared `alphabet`, if it has one. This is the part that
       is NOT derived, and it is deliberate. The charset is otherwise exactly
       the characters the corpus happens to contain today, which makes the
       atlas correct-by-construction but brittle against the one hole the
       architecture cannot close: `loc_raw()`, where a formatted string can
       carry any byte. Declaring the alphabet costs a few glyphs and removes
       the class. A language that declares nothing degrades to 1-3, which is
       safe, not silent."""
    seen: set[int] = set()
    for entry in table.entries:
        for forms in entry.forms.values():
            for text in forms.values():
                seen.update(ord(ch) for ch in text)
    for lang in table.langs:
        for text in (lang.group_separator or "", lang.alphabet or ""):
            seen.update(ord(ch) for ch in text)
    # Case closure last, so it also covers the alphabet and the separators.
    for cp in list(seen):
        ch = chr(cp)
        for other in (ch.upper(), ch.lower()):
            # A one-character upper()/lower() can return MORE than one
            # character (German ß -> SS). Every codepoint of the result is
            # renderable text, so take them all.
            seen.update(ord(c) for c in other)
    return sorted(cp for cp in seen if cp > 0x7F)


def render_charset_header(table: Table) -> str:
    codepoints = corpus_codepoints(table)
    for cp in codepoints:
        # The escapes below are universal character names, not hex escapes, so
        # they never swallow a following digit. C forbids a UCN below U+00A0
        # (outside $ @ `), and 0x80-0x9F are C1 controls that cannot appear in
        # display copy anyway -- refuse rather than emit something that will not
        # compile.
        if cp < 0xA0:
            raise LocError(
                f"{table.source}: U+{cp:04X} is a C1 control character and cannot be "
                "written as a universal character name; remove it from the corpus"
            )
        if 0xD800 <= cp <= 0xDFFF:
            raise LocError(
                f"{table.source}: U+{cp:04X} is a lone surrogate, not a character; "
                "the corpus JSON is malformed"
            )
    lines: list[str] = []
    lines.append("#ifndef LOC_CHARSET_GEN_H")
    lines.append("#define LOC_CHARSET_GEN_H")
    lines.append("")
    lines.append("/* Generated by features/localization/scripts/loc.py")
    lines.append(f"   from {table.source}. Do not edit by hand.")
    lines.append("")
    lines.append("   The non-ASCII half of the font charset: every codepoint the corpus can")
    lines.append("   render. The consumer prepends NT_CHARSET_ASCII -- digits, the fallback")
    lines.append("   marker and debug text are ASCII and never pass through the corpus.")
    lines.append("")
    lines.append("   This is why nothing outside content/loc/strings.json may draw non-ASCII")
    lines.append("   text: a literal in C cannot reach this set, and would ship as a blank")
    lines.append("   box. The pack build depends on this header, so editing a string")
    lines.append("   repacks the atlas.")
    lines.append("")
    lines.append("   Written as universal character names, not raw UTF-8: the value survives")
    lines.append("   a compiler reading the file in some other source encoding, and a diff")
    lines.append("   shows which codepoint changed rather than a run of mojibake. */")
    lines.append("")
    lines.append("#define LOC_CHARSET_NON_ASCII \\")
    for index, cp in enumerate(codepoints):
        char = chr(cp)
        terminator = "" if index == len(codepoints) - 1 else " \\"
        # \u takes EXACTLY four hex digits, \U exactly eight. Formatting an
        # astral codepoint with "04x" (a MINIMUM width) produced five digits,
        # and C read the first four plus a stray character: "ὠ0" compiles
        # clean under -Wall -Wextra -Wpedantic and stores U+1F60 followed by
        # '0'. The requested glyph silently leaves the atlas and an unrelated
        # one takes its place -- exactly the silent-loss failure this generator
        # exists to prevent. Found in review 2026-08-05; reachable as soon as a
        # CJK language lands, since LilitaOne-RussianChineseKo already covers
        # 507 codepoints above the BMP.
        escape = f"\\u{cp:04x}" if cp <= 0xFFFF else f"\\U{cp:08x}"
        lines.append(f'    "{escape}" /* {char} */{terminator}')
    lines.append("")
    lines.append(f"#define LOC_CHARSET_NON_ASCII_COUNT {len(codepoints)}")
    lines.append("")
    lines.append("#endif /* LOC_CHARSET_GEN_H */")
    lines.append("")
    return "\n".join(lines)


# --- TrueType/OpenType cmap reader -----------------------------------------
#
# Just enough to answer "does this font map this codepoint to a real glyph".
# Deliberately stdlib-only rather than fontTools: this module is spawned by
# other tools (tools/port/gen_tree_content.mjs runs it for the key index) and
# runs in CI, and a third-party import would make every one of those callers
# depend on a provisioned venv.
#
# The pack builder already ABORTS on a missing codepoint
# (external/neotolis-engine/tools/builder/nt_builder_font.c: NT_BUILD_ASSERT
# "codepoint not in font", unconditional, no NDEBUG guard). So this check is
# not what makes a missing glyph safe -- it is what makes it LEGIBLE: which
# font, which codepoint, which character. Without it the failure is an
# assertion in engine code with no way to tell the four packed fonts apart.


def _u16(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 2], "big")


def _u32(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 4], "big")


def _cmap_format4(data: bytes, base: int) -> set[int]:
    seg_count = _u16(data, base + 6) // 2
    ends = base + 14
    starts = ends + seg_count * 2 + 2  # +2 skips reservedPad
    deltas = starts + seg_count * 2
    range_offsets = deltas + seg_count * 2
    mapped: set[int] = set()
    for seg in range(seg_count):
        end = _u16(data, ends + seg * 2)
        start = _u16(data, starts + seg * 2)
        if start > end or start == 0xFFFF:
            continue
        delta = _u16(data, deltas + seg * 2)
        range_offset = _u16(data, range_offsets + seg * 2)
        for cp in range(start, end + 1):
            if range_offset == 0:
                glyph = (cp + delta) & 0xFFFF
            else:
                glyph_addr = range_offsets + seg * 2 + range_offset + (cp - start) * 2
                if glyph_addr + 2 > len(data):
                    continue
                glyph = _u16(data, glyph_addr)
                if glyph != 0:
                    glyph = (glyph + delta) & 0xFFFF
            if glyph != 0:
                mapped.add(cp)
    return mapped


def _cmap_format12(data: bytes, base: int) -> set[int]:
    group_count = _u32(data, base + 12)
    mapped: set[int] = set()
    for i in range(group_count):
        off = base + 16 + i * 12
        start = _u32(data, off)
        end = _u32(data, off + 4)
        start_glyph = _u32(data, off + 8)
        if start_glyph == 0:
            start += 1  # codepoint mapping to glyph 0 is .notdef, i.e. absent
        if start <= end:
            mapped.update(range(start, end + 1))
    return mapped


def font_codepoints(path: Path) -> set[int]:
    """The set of codepoints `path` maps to a real (non-.notdef) glyph."""
    data = path.read_bytes()
    if len(data) < 12:
        raise LocError(f"{path}: not a font file (too short)")
    if data[:4] == b"ttcf":
        raise LocError(f"{path}: TrueType collections are not supported")
    table_count = _u16(data, 4)
    cmap_off = None
    for i in range(table_count):
        rec = 12 + i * 16
        if data[rec:rec + 4] == b"cmap":
            cmap_off = _u32(data, rec + 8)
            break
    if cmap_off is None:
        raise LocError(f"{path}: no cmap table")
    # Prefer a full-Unicode subtable; fall back to the BMP one. Both are
    # authoritative for the codepoints they cover, so unioning is not needed --
    # but a font that only ships (0,3) must still be read.
    best: tuple[int, int] | None = None  # (rank, subtable offset)
    for i in range(_u16(data, cmap_off + 2)):
        rec = cmap_off + 4 + i * 8
        platform = _u16(data, rec)
        encoding = _u16(data, rec + 2)
        sub = cmap_off + _u32(data, rec + 4)
        rank = {
            (3, 10): 3,
            (0, 4): 3,
            (0, 6): 3,
            (3, 1): 2,
            (0, 3): 2,
            (0, 1): 1,
            (0, 0): 1,
        }.get((platform, encoding), 0)
        if rank and (best is None or rank > best[0]):
            best = (rank, sub)
    if best is None:
        raise LocError(f"{path}: cmap has no Unicode subtable")
    sub = best[1]
    fmt = _u16(data, sub)
    if fmt == 4:
        return _cmap_format4(data, sub)
    if fmt == 12:
        return _cmap_format12(data, sub)
    raise LocError(f"{path}: unsupported cmap format {fmt}")


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
    can quietly contradict the other; loc_test.py's SchemaIsLoadBearing pins
    every value that appears in both."""
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
        charset_path = out_dir / "loc_charset.gen.h"
        if write_if_changed(header_path, render_header(table)):
            changed.append(header_path)
        if write_if_changed(source_path, render_source(table)):
            changed.append(source_path)
        if write_if_changed(charset_path, render_charset_header(table)):
            changed.append(charset_path)
    if changed:
        print(f"loc generate: wrote {', '.join(str(path) for path in changed)}", file=sys.stderr)
    return 0


def cmd_fonts(args: argparse.Namespace) -> int:
    """Per-font coverage. Deliberately NOT 'does the corpus fit the charset' --
    that is true by construction once the charset is generated from the corpus,
    and a tautological check is worse than none. What actually differs between
    the packed fonts is their own glyph coverage: the charset this game shipped
    by hand carried the comment "NO U+2192 arrow: Rubik lacks it"."""
    table = load_table(Path(args.strings).resolve())
    wanted = corpus_codepoints(table)
    failures: list[str] = []
    for spec in args.font:
        label, sep, raw_path = spec.partition("=")
        if not sep:
            label, raw_path = Path(spec).name, spec
        path = Path(raw_path).resolve()
        if not path.exists():
            failures.append(f"{label}: no such font file: {path}")
            continue
        covered = font_codepoints(path)
        missing = [cp for cp in wanted if cp not in covered]
        if missing:
            shown = ", ".join(f"U+{cp:04X} '{chr(cp)}'" for cp in missing[:12])
            more = f" (+{len(missing) - 12} more)" if len(missing) > 12 else ""
            failures.append(f"{label} ({path.name}) lacks {len(missing)} corpus codepoint(s): {shown}{more}")
    if failures:
        print("loc fonts: a packed font cannot render the corpus", file=sys.stderr)
        for line in failures:
            print(f"  - {line}", file=sys.stderr)
        print(
            "\nThe pack builder would abort on this anyway (nt_builder_font.c asserts "
            "'codepoint not in font'), but without naming the font or the character. "
            "Either change the copy, or replace the font.",
            file=sys.stderr,
        )
        return 1
    print(f"loc fonts: ok ({len(args.font)} font(s) cover all {len(wanted)} non-ASCII corpus codepoints)")
    return 0


def cmd_plural_cases(args: argparse.Namespace) -> int:
    out_dir = Path(args.out_dir).resolve()
    changed: list[Path] = []
    for name, text in (
        ("loc_plural_cases.gen.h", render_plural_cases_header()),
        ("loc_plural_cases.gen.c", render_plural_cases()),
    ):
        if write_if_changed(out_dir / name, text):
            changed.append(out_dir / name)
    if changed:
        print(f"loc plural-cases: wrote {', '.join(str(path) for path in changed)}", file=sys.stderr)
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

    fonts = sub.add_parser(
        "fonts",
        help="Check every packed font can render every non-ASCII codepoint the corpus uses.",
    )
    fonts.add_argument("--strings", required=True, help="Path to the game's content/loc/strings.json.")
    fonts.add_argument(
        "--font",
        action="append",
        required=True,
        metavar="LABEL=PATH",
        help="A packed font, named as the game names it (e.g. body=assets/fonts/Rubik-Regular.ttf). Repeatable.",
    )
    fonts.set_defaults(func=cmd_fonts)

    cases = sub.add_parser(
        "plural-cases",
        help="Emit every (rule, n) -> category pair as C, for the runtime cross-check.",
    )
    cases.add_argument("--out-dir", required=True, help="Directory for loc_plural_cases.gen.{h,c}.")
    cases.set_defaults(func=cmd_plural_cases)

    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
