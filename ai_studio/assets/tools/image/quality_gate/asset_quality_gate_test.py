from __future__ import annotations

import hashlib
import io
import json
import subprocess
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from PIL import Image, ImageDraw

from ai_studio.assets.tools.image.quality_gate.asset_quality_gate import evaluate, main


THRESHOLDS = {
    "max_spill_edge_ratio": 0.05,
    "max_halo_edge_ratio": 0.05,
    "max_alpha_noise_ratio": 0.02,
    "max_empty_margin_ratio": 0.50,
    "aspect_ratio": {"width": 1, "height": 1, "max_relative_error": 0.05},
}

JAM_CORPUS = Path(__file__).with_name("fixtures") / "jam_corpus"
REPO_ROOT = Path(__file__).resolve().parents[5]


def tagged_fixture_bytes(tag: str, source_path: str) -> bytes:
    result = subprocess.run(
        [
            "git",
            "-c",
            f"safe.directory={REPO_ROOT.as_posix()}",
            "show",
            f"{tag}:{source_path}",
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"cannot load archived jam fixture {tag}:{source_path}: "
            f"{result.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return result.stdout


def clean_square(size: tuple[int, int] = (32, 32)) -> Image.Image:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(image).rectangle((4, 4, size[0] - 5, size[1] - 5), fill=(180, 110, 40, 255))
    return image


class AssetQualityGateTest(unittest.TestCase):
    def test_real_jam_corpus_rejects_known_spill_and_accepts_neighbor(self) -> None:
        manifest = json.loads((JAM_CORPUS / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema"], "ai_studio.asset_quality_gate.jam_corpus.v1")
        self.assertEqual(manifest["source_tag"], "rb-dark-rpg-vibejam1-2026-07-05")
        self.assertEqual({fixture["role"] for fixture in manifest["fixtures"]}, {"broken", "clean"})
        clean_fixture = next(fixture for fixture in manifest["fixtures"] if fixture["role"] == "clean")
        self.assertEqual(clean_fixture["acceptance"], "city-style v2 accepted")
        self.assertEqual(clean_fixture["origin"], "ai")

        reports: dict[str, dict[str, object]] = {}
        fixture_bytes: dict[str, bytes] = {}
        for fixture in manifest["fixtures"]:
            source_bytes = tagged_fixture_bytes(manifest["source_tag"], fixture["source_path"])
            fixture_bytes[fixture["role"]] = source_bytes
            git_object = f"blob {len(source_bytes)}\0".encode("ascii") + source_bytes
            self.assertEqual(hashlib.sha1(git_object).hexdigest(), fixture["source_git_blob"])
            self.assertEqual(hashlib.sha256(source_bytes).hexdigest().upper(), fixture["sha256"])
            with Image.open(io.BytesIO(source_bytes)) as opened:
                image = opened.convert("RGBA")
            self.assertEqual(image.size, (fixture["width"], fixture["height"]))
            thresholds = {
                **THRESHOLDS,
                "aspect_ratio": {
                    "width": fixture["intended_width"],
                    "height": fixture["intended_height"],
                    "max_relative_error": THRESHOLDS["aspect_ratio"]["max_relative_error"],
                },
            }
            report = evaluate(image, key_color=(0, 255, 0), thresholds=thresholds)
            reports[fixture["role"]] = report
            self.assertEqual(report["verdict"], fixture["expected_verdict"])
            self.assertTrue(
                set(fixture["expected_problem_codes"]).issubset(
                    problem["code"] for problem in report["problems"]
                )
            )

        self.assertEqual(reports["clean"]["problems"], [])
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            report_path = root / "report.json"
            thumbnail = root / "problem.png"
            broken = next(fixture for fixture in manifest["fixtures"] if fixture["role"] == "broken")
            broken_source = root / broken["file"]
            broken_source.write_bytes(fixture_bytes["broken"])
            with Image.open(broken_source) as image:
                thresholds = {
                    **THRESHOLDS,
                    "aspect_ratio": {
                        "width": broken["intended_width"],
                        "height": broken["intended_height"],
                        "max_relative_error": THRESHOLDS["aspect_ratio"]["max_relative_error"],
                    },
                }
            thresholds_path = root / "thresholds.json"
            thresholds_path.write_text(json.dumps(thresholds), encoding="utf-8")
            output = io.StringIO()
            with redirect_stdout(output):
                code = main([
                    "--source", str(broken_source),
                    "--key-color", "#00FF00",
                    "--thresholds", str(thresholds_path),
                    "--json-output", str(report_path),
                    "--problem-thumbnail", str(thumbnail),
                ])
            self.assertEqual(code, 1)
            output_lines = output.getvalue().splitlines()
            self.assertEqual(len(output_lines), 1)
            self.assertRegex(
                output_lines[0],
                r"^FAIL jam_broken_mill_scavenger_scene\.png: "
                r"key_spill=[0-9.]+>0\.05, edge_halo=[0-9.]+>0\.05$",
            )
            self.assertTrue(thumbnail.is_file())
            with Image.open(thumbnail) as proof:
                self.assertEqual(proof.format, "PNG")
                self.assertLessEqual(max(proof.size), 192)
                self.assertLess(proof.width * proof.height, broken["width"] * broken["height"])

    def test_clean_cutout_passes_with_stable_bbox_and_denominators(self) -> None:
        report = evaluate(clean_square(), key_color=(255, 0, 255), thresholds=THRESHOLDS)

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["metrics"]["content_bbox"], [4, 4, 24, 24])
        self.assertEqual(report["metrics"]["empty_margin_ratio"], 0.4375)
        self.assertGreater(report["metrics"]["edge_sample_px"], 0)
        self.assertGreater(report["metrics"]["alpha_transition_sample_px"], 0)
        self.assertEqual(report["problems"], [])

        low_alpha_speck = clean_square()
        low_alpha_speck.putpixel((0, 0), (255, 0, 255, 1))
        self.assertEqual(
            evaluate(low_alpha_speck, key_color=(255, 0, 255), thresholds=THRESHOLDS)["metrics"]["content_bbox"],
            [4, 4, 24, 24],
        )

    def test_key_spill_and_non_key_halo_are_separate_edge_failures(self) -> None:
        spill = clean_square()
        draw = ImageDraw.Draw(spill)
        draw.line((4, 4, 4, 27), fill=(255, 0, 255, 180), width=2)
        spill_report = evaluate(spill, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertEqual(spill_report["verdict"], "fail")
        self.assertIn("key_spill", [problem["code"] for problem in spill_report["problems"]])
        self.assertGreater(spill_report["metrics"]["spill_edge_ratio"], THRESHOLDS["max_spill_edge_ratio"])

        halo = clean_square()
        ImageDraw.Draw(halo).line((4, 4, 4, 27), fill=(100, 0, 100, 180), width=2)
        halo_report = evaluate(halo, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertEqual(halo_report["verdict"], "fail")
        self.assertIn("edge_halo", [problem["code"] for problem in halo_report["problems"]])
        self.assertGreater(halo_report["metrics"]["halo_edge_ratio"], THRESHOLDS["max_halo_edge_ratio"])

        green_under_magenta = clean_square()
        ImageDraw.Draw(green_under_magenta).line((4, 4, 4, 27), fill=(0, 180, 0, 255), width=2)
        self.assertEqual(evaluate(green_under_magenta, key_color=(255, 0, 255), thresholds=THRESHOLDS)["verdict"], "pass")

        magenta_under_green = clean_square()
        ImageDraw.Draw(magenta_under_green).line((4, 4, 4, 27), fill=(180, 0, 180, 255), width=2)
        self.assertEqual(evaluate(magenta_under_green, key_color=(0, 255, 0), thresholds=THRESHOLDS)["verdict"], "pass")

    def test_isolated_alpha_fragments_and_excess_crop_margin_fail(self) -> None:
        noisy = clean_square()
        for point in ((1, 1), (1, 30), (30, 1), (30, 30), (1, 16), (30, 16)):
            noisy.putpixel(point, (180, 110, 40, 255))
        noisy_report = evaluate(noisy, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertIn("alpha_noise", [problem["code"] for problem in noisy_report["problems"]])

        padded = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
        ImageDraw.Draw(padded).rectangle((10, 10, 21, 21), fill=(180, 110, 40, 255))
        padded_report = evaluate(padded, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertIn("empty_margin", [problem["code"] for problem in padded_report["problems"]])

        opaque = Image.new("RGBA", (32, 32), (180, 110, 40, 255))
        opaque_report = evaluate(opaque, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertIn("no_transparency", [problem["code"] for problem in opaque_report["problems"]])

        wrong_aspect = clean_square((48, 32))
        aspect_report = evaluate(wrong_aspect, key_color=(255, 0, 255), thresholds=THRESHOLDS)
        self.assertIn("aspect_ratio", [problem["code"] for problem in aspect_report["problems"]])

    def test_transparent_sources_disable_key_color_metrics_fail_closed(self) -> None:
        transparent_thresholds = {**THRESHOLDS, "max_spill_edge_ratio": None, "max_halo_edge_ratio": None}
        self.assertEqual(evaluate(clean_square(), key_color=None, thresholds=transparent_thresholds)["verdict"], "pass")
        with self.assertRaisesRegex(ValueError, "keyed gates require"):
            evaluate(clean_square(), key_color=(255, 0, 255), thresholds=transparent_thresholds)
        with self.assertRaisesRegex(ValueError, "transparent gates require"):
            evaluate(clean_square(), key_color=None, thresholds=THRESHOLDS)
        with self.assertRaisesRegex(ValueError, "canonical magenta.*green"):
            evaluate(Image.new("RGBA", (32, 32), (180, 110, 40, 255)), key_color=(18, 52, 86), thresholds=THRESHOLDS)

    def test_cli_writes_json_and_problem_thumbnail_with_one_line_verdict(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "broken.png"
            report_path = root / "report.json"
            thumbnail = root / "problem.png"
            thresholds_path = root / "thresholds.json"
            broken = clean_square()
            ImageDraw.Draw(broken).line((4, 4, 4, 27), fill=(255, 0, 255, 180), width=2)
            broken.save(source)
            thresholds_path.write_text(json.dumps(THRESHOLDS), encoding="utf-8")

            output = io.StringIO()
            with redirect_stdout(output):
                code = main([
                    "--source", str(source),
                    "--key-color", "#FF00FF",
                    "--thresholds", str(thresholds_path),
                    "--json-output", str(report_path),
                    "--problem-thumbnail", str(thumbnail),
                ])

            self.assertEqual(code, 1)
            self.assertRegex(output.getvalue().strip(), r"^FAIL broken\.png: key_spill=")
            self.assertEqual(json.loads(report_path.read_text(encoding="utf-8"))["verdict"], "fail")
            self.assertTrue(thumbnail.is_file())
            with Image.open(thumbnail) as proof:
                self.assertLessEqual(max(proof.size), 192)

    def test_cli_is_compact_on_invalid_input_and_does_not_mark_pass_as_problem(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "clean.png"
            report_path = root / "report.json"
            thumbnail = root / "problem.png"
            thresholds_path = root / "thresholds.json"
            clean_square().save(source)
            thresholds_path.write_text("{broken", encoding="utf-8")
            thumbnail.write_bytes(b"stale")

            output = io.StringIO()
            with redirect_stdout(output):
                code = main([
                    "--source", str(source),
                    "--key-color", "#FF00FF",
                    "--thresholds", str(thresholds_path),
                    "--json-output", str(report_path),
                    "--problem-thumbnail", str(thumbnail),
                ])
            self.assertEqual(code, 2)
            self.assertRegex(output.getvalue().strip(), r"^FAIL clean\.png: invalid_input=")
            self.assertEqual(json.loads(report_path.read_text(encoding="utf-8"))["problems"][0]["code"], "invalid_input")
            self.assertFalse(thumbnail.exists())

            thresholds_path.write_text(json.dumps(THRESHOLDS), encoding="utf-8")
            thumbnail.write_bytes(b"stale")
            output = io.StringIO()
            with redirect_stdout(output):
                code = main([
                    "--source", str(source),
                    "--key-color", "#FF00FF",
                    "--thresholds", str(thresholds_path),
                    "--json-output", str(report_path),
                    "--problem-thumbnail", str(thumbnail),
                ])
            self.assertEqual(code, 0)
            self.assertRegex(output.getvalue().strip(), r"^PASS clean\.png:")
            self.assertFalse(thumbnail.exists())


if __name__ == "__main__":
    unittest.main()
