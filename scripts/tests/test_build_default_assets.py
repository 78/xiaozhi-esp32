import importlib.util
import json
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_default_assets", ROOT / "scripts" / "build_default_assets.py"
)
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class BuildDefaultAssetsTest(unittest.TestCase):
    def test_text_font_metadata_uses_bundle_charset_size_and_bpp(self):
        with tempfile.TemporaryDirectory() as directory:
            assets = Path(directory)
            BUILD.generate_index_json(
                str(assets),
                None,
                "font_noto_sans_common_20_4.bin",
                None,
                font_bundle_id="noto-v1",
            )
            index = json.loads((assets / "index.json").read_text(encoding="utf-8"))
            self.assertEqual(
                index["text_font_meta"],
                {"charset": "common", "size": 20, "bpp": 4, "bundle": "noto-v1"},
            )

    def test_text_font_requires_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                BUILD.generate_index_json(
                    directory, None, "font_noto_sans_common_20_4.bin", None
                )

    def test_max_size_rejects_oversized_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            extra = root / "extra"
            extra.mkdir()
            (extra / "big.bin").write_bytes(b"\x5a" * 4096)
            output = root / "assets.bin"

            ok = BUILD.build_assets_integrated(
                None,
                None,
                None,
                None,
                str(extra),
                str(output),
                max_size=100,
            )
            self.assertFalse(ok)

    def test_max_size_allows_fitting_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            extra = root / "extra"
            extra.mkdir()
            (extra / "small.bin").write_bytes(b"hello")
            output = root / "assets.bin"

            ok = BUILD.build_assets_integrated(
                None,
                None,
                None,
                None,
                str(extra),
                str(output),
                max_size=64 * 1024,
            )
            self.assertTrue(ok)
            self.assertTrue(output.exists())
            self.assertLessEqual(output.stat().st_size, 64 * 1024)

    def test_esp_hi_emoji_pack_matches_runtime_assets(self):
        """ESP-HI must only pack AAF files that emoji_display actually loads."""
        cmake = (ROOT / "main" / "CMakeLists.txt").read_text(encoding="utf-8")
        display = (
            ROOT / "main" / "boards" / "espressif" / "esp-hi" / "emoji_display.cc"
        ).read_text(encoding="utf-8")

        # Extract FILES_TO_DOWNLOAD block for ESP_HI.
        match = re.search(
            r"if\(CONFIG_BOARD_TYPE_ESP_HI\).*?set\(FILES_TO_DOWNLOAD(.*?)\)\n",
            cmake,
            re.S,
        )
        self.assertIsNotNone(match, "ESP_HI FILES_TO_DOWNLOAD not found in CMakeLists")
        cmake_files = set(re.findall(r'"([^"]+\.aaf)"', match.group(1)))

        runtime_files = set(re.findall(r'"([^"]+\.aaf)"', display))
        self.assertTrue(runtime_files, "No AAF names found in emoji_display.cc")
        self.assertEqual(
            cmake_files,
            runtime_files,
            "ESP-HI CMake AAF pack list must match emoji_display runtime assets",
        )
        self.assertRegex(
            cmake,
            r"image_player/raw/[0-9a-f]{40}/test_apps/test_8bit",
            "ESP-HI emoji URL should be pinned to a full image_player commit SHA",
        )


if __name__ == "__main__":
    unittest.main()
