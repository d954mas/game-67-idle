from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .model import GenerationModel
from .render_events import EventRenderer
from .render_state import StateRenderer


def default_out_dir(schema_path: Path, repo_root: Path) -> Path:
    schema = schema_path.resolve()
    repo_root = repo_root.resolve()
    try:
        parts = schema.relative_to(repo_root).parts
    except ValueError:
        return schema.parent.parent / "build" / "generated" / "game-state"
    if len(parts) >= 4 and parts[0] in {"templates", "games"} and parts[2] == "state":
        return repo_root / parts[0] / parts[1] / "build" / "generated" / "game-state"
    if len(parts) >= 2 and parts[0] == "state":
        return repo_root / "build" / "generated" / "game-state"
    return schema.parent.parent / "build" / "generated" / "game-state"


def write_if_changed(path: Path, text: str) -> bool:
    data = text.encode("utf-8")
    if path.exists() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def render_bundle(model: GenerationModel) -> dict[str, str]:
    state = StateRenderer(model.ns)
    events = EventRenderer(model.ns)
    prefix = f"{model.ns.id}_state"
    return {
        f"{prefix}.h": state.render_header(model.schema, model.schema_label),
        f"{prefix}.c": state.render_source(model.schema, model.schema_label),
        f"{prefix}_schema.gen.h": state.render_schema_header(model.schema, model.schema_label),
        f"{prefix}_events.gen.h": events.render_events_header(model.schema, model.schema_label),
        f"{prefix}_events.gen.c": events.render_events_source(model.schema, model.schema_label),
    }


def write_bundle(model: GenerationModel, out_dir: Path) -> list[Path]:
    changed = []
    for name, text in render_bundle(model).items():
        path = out_dir / name
        if write_if_changed(path, text):
            changed.append(path)
    return changed
