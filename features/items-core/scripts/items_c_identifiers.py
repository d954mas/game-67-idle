"""Shared C identifier rules for generated Items APIs."""

from __future__ import annotations

import re


_MEMBER_RE = re.compile(r"^[a-z][a-z0-9_]*$")
_RESERVED = {
    "alignas", "alignof", "asm", "auto", "bool", "break", "case", "char", "const",
    "continue", "default", "do", "double", "else", "enum", "extern", "false",
    "float", "for", "goto", "if", "inline", "int", "linux", "long", "noreturn",
    "register", "restrict", "return", "short", "signed", "sizeof", "static",
    "static_assert", "struct", "switch", "thread_local", "true", "typedef",
    "typeof", "typeof_unqual", "union", "unix", "unsigned", "void", "volatile", "while",
}


def is_c_member_name(value: object) -> bool:
    return (
        isinstance(value, str)
        and _MEMBER_RE.fullmatch(value) is not None
        and value not in _RESERVED
    )
