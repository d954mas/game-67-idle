"""Locate the Studio checkout from any game repo, feature workspace or Blender process.

Games sit at different depths (games/<id>, games/private/<id>) and feature workspaces put the
Studio worktree somewhere else entirely, so a fixed `parents[N]` or absolute path breaks.
"""
import os
import sys
from pathlib import Path

MARKER = Path("ai_studio") / "assets" / "tools" / "blender" / "lib"


def is_studio_root(path):
    return (Path(path) / MARKER).is_dir()


def find_studio_root(start=None):
    """NT_STUDIO_ROOT, else the nearest ancestor of `start` (default: cwd) holding the Studio."""
    env = os.environ.get("NT_STUDIO_ROOT")
    if env:
        if not is_studio_root(env):
            raise FileNotFoundError(f"NT_STUDIO_ROOT={env} is not a Studio checkout")
        return Path(env).resolve()
    here = Path(start or os.getcwd()).resolve()
    for candidate in (here, *here.parents):
        if is_studio_root(candidate):
            return candidate
    # This file itself lives inside a Studio checkout.
    return Path(__file__).resolve().parents[5]


def ensure_importable(start=None):
    """Put the Studio root on sys.path so `ai_studio.assets.tools.blender.lib` imports work."""
    root = str(find_studio_root(start))
    if root not in sys.path:
        sys.path.insert(0, root)
    return Path(root)
