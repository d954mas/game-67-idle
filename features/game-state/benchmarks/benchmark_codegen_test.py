import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().with_name("benchmark_codegen.py")
SPEC = importlib.util.spec_from_file_location("benchmark_codegen", MODULE_PATH)
benchmark_codegen = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(benchmark_codegen)


class BenchmarkCodegenTests(unittest.TestCase):
    def test_frozen_fixture_generates_all_fragments(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            schemas = benchmark_codegen.materialize_fixture(benchmark_codegen.DEFAULT_FIXTURE, root)
            benchmark_codegen.generate_fixture(schemas, root / "generated", root)
            self.assertEqual([path.stem.split(".")[0] for path in schemas], ["game", "settings", "items", "progression"])
            for fragment in ("game", "settings", "items", "progression"):
                self.assertTrue((root / "generated" / fragment / f"{fragment}_state.c").exists())

    def test_nearest_rank_p90(self):
        self.assertEqual(benchmark_codegen.percentile(list(range(1, 21)), 0.9), 18)

    def test_measurement_contract_is_fixed_and_advisory(self):
        self.assertEqual(benchmark_codegen.WARMUPS, 3)
        self.assertGreaterEqual(benchmark_codegen.WARM_RUNS, 20)
        self.assertGreaterEqual(benchmark_codegen.COLD_RUNS, 3)
        self.assertLessEqual(benchmark_codegen.COLD_RUNS, 5)
        self.assertEqual(benchmark_codegen.ADVISORY_REGRESSION, 0.15)

    def test_baseline_requires_matching_local_contract(self):
        expected = benchmark_codegen.baseline_contract(benchmark_codegen.DEFAULT_FIXTURE)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "baseline.json"
            path.write_text(json.dumps({**expected, "warm_median_ms": 4.0}), encoding="utf-8")
            self.assertEqual(benchmark_codegen.load_baseline(path, expected), (4.0, "compatible"))
            path.write_text(
                json.dumps({**expected, "platform": "another-machine", "warm_median_ms": 4.0}),
                encoding="utf-8",
            )
            self.assertEqual(benchmark_codegen.load_baseline(path, expected), (None, "incompatible"))


if __name__ == "__main__":
    unittest.main()
