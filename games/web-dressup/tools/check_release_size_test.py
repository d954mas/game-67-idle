import json
import tempfile
import unittest
from pathlib import Path

from check_release_size import ReleaseGateError, build_manifest


REQUIRED_FILES = {
    "index.html": b"<html></html>",
    "game.js": b"console.log('release')",
    "game.wasm": b"\x00asm",
    "assets/game.ntpack": b"pack",
    "platform-sdk.js": b"sdk",
    "platform-sdk-core.js": b"core",
    "platform-sdk-adapter.js": b"poki",
}


class ReleaseSizeGateTests(unittest.TestCase):
    def make_payload(self, root: Path, files=None) -> Path:
        bin_dir = root / "wasm-release-poki" / "bin"
        for relative, data in (files or REQUIRED_FILES).items():
            path = bin_dir / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (bin_dir.parent / "CMakeCache.txt").write_text(
            "CMAKE_BUILD_TYPE:STRING=Release\n"
            "GAME_DEVAPI_ENABLED:BOOL=OFF\n"
            "GAME_PLATFORM_SDK_DEBUG_UI:BOOL=OFF\n"
            "GAME_PUBLISH_TARGET:STRING=poki\n",
            encoding="utf-8",
        )
        return bin_dir

    def test_accepts_complete_payload_under_limit(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest = build_manifest(self.make_payload(Path(temp)), limit_bytes=1024)
        self.assertEqual(manifest["status"], "pass")
        self.assertEqual(manifest["total_bytes"], sum(map(len, REQUIRED_FILES.values())))

    def test_rejects_payload_over_limit(self):
        with tempfile.TemporaryDirectory() as temp:
            files = dict(REQUIRED_FILES)
            files["assets/game.ntpack"] = b"x" * 1025
            with self.assertRaisesRegex(ReleaseGateError, "exceeds"):
                build_manifest(self.make_payload(Path(temp), files), limit_bytes=1024)

    def test_rejects_missing_entrypoint_or_pack(self):
        for missing in ("index.html", "assets/game.ntpack"):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as temp:
                files = dict(REQUIRED_FILES)
                del files[missing]
                with self.assertRaisesRegex(ReleaseGateError, "missing required"):
                    build_manifest(self.make_payload(Path(temp), files))

    def test_rejects_debug_or_devapi_payloads(self):
        for build_name in ("wasm-debug-poki", "wasm-devapi-release-poki"):
            with self.subTest(build_name=build_name), tempfile.TemporaryDirectory() as temp:
                bin_dir = self.make_payload(Path(temp))
                renamed = bin_dir.parent.parent / build_name / "bin"
                renamed.parent.mkdir(parents=True)
                bin_dir.rename(renamed)
                with self.assertRaisesRegex(ReleaseGateError, "release Poki build"):
                    build_manifest(renamed)

        with tempfile.TemporaryDirectory() as temp:
            files = dict(REQUIRED_FILES)
            files["game.pdb"] = b"debug"
            with self.assertRaisesRegex(ReleaseGateError, "forbidden release file"):
                build_manifest(self.make_payload(Path(temp), files))

        with tempfile.TemporaryDirectory() as temp:
            bin_dir = self.make_payload(Path(temp))
            (bin_dir.parent / "CMakeCache.txt").write_text(
                "CMAKE_BUILD_TYPE:STRING=Debug\n"
                "GAME_DEVAPI_ENABLED:BOOL=ON\n"
                "GAME_PLATFORM_SDK_DEBUG_UI:BOOL=ON\n"
                "GAME_PUBLISH_TARGET:STRING=poki\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ReleaseGateError, "not release-safe"):
                build_manifest(bin_dir)

    def test_manifest_is_deterministic_and_sorted(self):
        with tempfile.TemporaryDirectory() as temp:
            bin_dir = self.make_payload(Path(temp))
            first = build_manifest(bin_dir)
            second = build_manifest(bin_dir)
        self.assertEqual(
            json.dumps(first, sort_keys=True, separators=(",", ":")),
            json.dumps(second, sort_keys=True, separators=(",", ":")),
        )
        self.assertEqual([item["path"] for item in first["files"]], sorted(REQUIRED_FILES))


if __name__ == "__main__":
    unittest.main()
