#!/usr/bin/env python3
"""Generate the supported per-fragment game state C API from a v2 state schema."""

from __future__ import annotations

import argparse
from pathlib import Path

from state_codegen.model import build_model
from state_codegen.naming import Ns, c_ident, c_macro, pascal, provenance_label
from state_codegen.output import default_out_dir as _default_out_dir, render_bundle, write_bundle, write_if_changed
from state_codegen.render_events import EventRenderer
from state_codegen.render_state import StateRenderer
from state_codegen.schema import load_schema, validate_field_names

ROOT = Path(__file__).resolve().parents[3]
TOOL_LABEL = "features/game-state/scripts/generate_state.py"
_pascal = pascal


def default_out_dir(schema_path: Path) -> Path:
    return _default_out_dir(schema_path, ROOT)


def relative_label(path: Path) -> str:
    return provenance_label(path, ROOT)


def render_header(schema, schema_label):
    return StateRenderer(Ns(schema["fragment"])).render_header(schema, schema_label)

def render_source(schema, schema_label="state/game_state.schema.json"):
    return StateRenderer(Ns(schema["fragment"])).render_source(schema, schema_label)

def render_schema_header(schema, schema_label):
    return StateRenderer(Ns(schema["fragment"])).render_schema_header(schema, schema_label)

def render_events_header(schema, schema_label):
    return EventRenderer(Ns(schema["fragment"])).render_events_header(schema, schema_label)

def render_events_source(schema, schema_label):
    return EventRenderer(Ns(schema["fragment"])).render_events_source(schema, schema_label)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", required=True, help="Schema JSON to generate from.")
    parser.add_argument("--out-dir", default=None, help="Directory for generated fragment state files.")
    parser.add_argument("--fragment", default=None, help="Expected fragment id (asserted against schema.fragment).")
    args = parser.parse_args(argv)

    schema_path = Path(args.schema).resolve()
    schema = load_schema(schema_path)
    fragment = schema["fragment"]
    if args.fragment is not None and args.fragment != fragment:
        raise SystemExit(f"--fragment {args.fragment!r} does not match schema.fragment {fragment!r}")
    out_dir = Path(args.out_dir).resolve() if args.out_dir else default_out_dir(schema_path).resolve()
    model = build_model(schema, relative_label(schema_path))
    changed = write_bundle(model, out_dir)
    if changed:
        for path in changed:
            print(f"generated {relative_label(path)}")
    else:
        print("state generated files are up to date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
