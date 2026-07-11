from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path


def c_ident(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value)
    ident = re.sub(r"[^a-zA-Z0-9]+", "_", value).strip("_").lower()
    if not ident:
        raise SystemExit(f"cannot make C identifier from {value!r}")
    if ident[0].isdigit():
        ident = "_" + ident
    return ident


def c_macro(value: str) -> str:
    return c_ident(value).upper()


def pascal(ident: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in ident.split("_") if part)


@dataclass(frozen=True)
class Ns:
    id: str

    @property
    def ident(self) -> str: return self.id
    @property
    def upper(self) -> str: return self.id.upper()
    @property
    def pascal(self) -> str: return pascal(self.id)
    @property
    def type(self) -> str: return f"{self.pascal}State"
    @property
    def fn(self) -> str: return f"{self.id}_state_"
    @property
    def macro(self) -> str: return f"{self.upper}_STATE_"
    @property
    def inst(self) -> str: return f"{self.id}_state"
    @property
    def frag(self) -> str: return f"{self.id}_state_fragment"


def provenance_label(path: Path, repo_root: Path) -> str:
    path = path.resolve()
    repo_root = repo_root.resolve()
    try:
        relative = path.relative_to(repo_root)
    except ValueError:
        if path.parent.name == "state":
            return f"state/{path.name}"
        return path.name
    parts = relative.parts
    if len(parts) >= 3 and parts[0] in {"games", "templates"}:
        return Path(*parts[2:]).as_posix()
    return relative.as_posix()
