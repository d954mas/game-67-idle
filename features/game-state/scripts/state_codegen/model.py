from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from .naming import Ns


@dataclass(frozen=True)
class GenerationModel:
    schema: dict[str, Any]
    ns: Ns
    schema_label: str


def build_model(schema: dict[str, Any], schema_label: str) -> GenerationModel:
    return GenerationModel(schema=schema, ns=Ns(schema["fragment"]), schema_label=schema_label)
