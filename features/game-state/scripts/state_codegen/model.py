from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from .naming import Ns


@dataclass(frozen=True)
class GenerationModel:
    schema: dict[str, Any]
    ns: Ns
    schema_label: str
    instance: bool = False


def build_model(schema: dict[str, Any], schema_label: str, instance: bool = False) -> GenerationModel:
    return GenerationModel(
        schema=schema, ns=Ns(schema["fragment"]), schema_label=schema_label, instance=instance,
    )
