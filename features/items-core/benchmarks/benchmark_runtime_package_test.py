#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("benchmark_runtime_package.py")
SPEC = importlib.util.spec_from_file_location("benchmark_runtime_package", SCRIPT)
BENCHMARK = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(BENCHMARK)


class RuntimePackageBenchmarkTests(unittest.TestCase):
    def test_candidates_describe_the_same_catalog(self):
        arrays, blob = BENCHMARK.fixture_signatures()
        self.assertEqual(arrays, blob)

    def test_blob_fixture_hash_is_current(self):
        fixture = json.loads(BENCHMARK.BLOB_FIXTURE.read_text(encoding="utf-8"))
        self.assertEqual(fixture["content_hash"], BENCHMARK._snapshot_hash(fixture))

    def test_cmake_cache_reader_exposes_release_toolchain(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeCache.txt").write_text(
                "CMAKE_BUILD_TYPE:STRING=Release\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                "CMAKE_C_COMPILER:FILEPATH=/tool/clang\n",
                encoding="utf-8",
            )
            self.assertEqual(BENCHMARK._cmake_cache(root), {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_GENERATOR": "Ninja",
                "CMAKE_C_COMPILER": "/tool/clang",
            })


if __name__ == "__main__":
    unittest.main()
