import importlib.util
import json
import re
import struct
import subprocess
import sys
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

    def test_wakenet10_copy_keeps_s3_p1_slice(self):
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / "wn10_nihaoxiaozhi"
            dst = Path(directory) / "out"
            src.mkdir()
            (src / "_MODEL_INFO_p1").write_text("p1-info", encoding="utf-8")
            (src / "_MODEL_INFO_p2").write_text("p2-info", encoding="utf-8")
            (src / "wn10_data_p1").write_bytes(b"s3-data")
            (src / "wn10_data_p2").write_bytes(b"p4-data")
            (src / "extra.bin").write_bytes(b"keep")

            self.assertTrue(BUILD.copy_wakenet_model(str(src), str(dst), "esp32s3"))
            self.assertEqual((dst / "wn10_data").read_bytes(), b"s3-data")
            self.assertEqual((dst / "_MODEL_INFO_").read_text(encoding="utf-8"), "p1-info")
            self.assertEqual((dst / "extra.bin").read_bytes(), b"keep")
            self.assertFalse((dst / "wn10_data_p1").exists())
            self.assertFalse((dst / "wn10_data_p2").exists())

    def test_wakenet10_copy_keeps_p4_p2_slice(self):
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / "wn10_nihaoxiaozhi"
            dst = Path(directory) / "out"
            src.mkdir()
            (src / "_MODEL_INFO_p2").write_text("p2-info", encoding="utf-8")
            (src / "wn10_data_p2").write_bytes(b"p4-data")

            self.assertTrue(BUILD.copy_wakenet_model(str(src), str(dst), "esp32p4"))
            self.assertEqual((dst / "wn10_data").read_bytes(), b"p4-data")
            self.assertEqual((dst / "_MODEL_INFO_").read_text(encoding="utf-8"), "p2-info")

    def test_wakenet10_copy_rejects_unknown_target(self):
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / "wn10_nihaoxiaozhi"
            src.mkdir()
            (src / "wn10_data_p1").write_bytes(b"s3-data")
            with self.assertRaises(ValueError):
                BUILD.copy_wakenet_model(str(src), str(Path(directory) / "out"), "esp32c3")

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


class ConfiguredAssetsTest(unittest.TestCase):
    def base_pack(self, root, name_length=32):
        index = {"version": 1, "srmodels": "old-speech.bin", "text_font": "font.bin",
                 "emoji_collection": [{"name": "happy", "file": "image.bin"}],
                 "layout": "layout.json", "multinet_model": {"commands": ["old"]}}
        files = {
            "index.json": (json.dumps(index).encode(), 0, 0),
            "old-speech.bin": (b"old Chinese model", 0, 0),
            "font.bin": (b"opaque font bytes", 0, 0),
            "image.bin": (b"opaque image bytes", 284, 240),
            "layout.json": (b'{"keep":"layout"}', 0, 0),
        }
        table, data = bytearray(), bytearray()
        for name, (contents, width, height) in files.items():
            table.extend(struct.pack(f"<{name_length}sIIHH", name.encode(), len(contents),
                                     len(data), width, height))
            data.extend(b"ZZ" + contents)
        payload = table + data
        base = root / "base.bin"
        base.write_bytes(struct.pack("<III", len(files), sum(payload) & 0xFFFF, len(payload)) + payload)
        return base, files

    def run_builder(self, root, model=None, name_length=32, max_size=None):
        sdkconfig = root / "sdkconfig"
        sdkconfig.write_text(
            'CONFIG_IDF_TARGET="esp32s3"\n' + (
                f"CONFIG_USE_AFE_WAKE_WORD=y\nCONFIG_SR_WN_{model.upper()}=y\n"
                if model else "CONFIG_WAKE_WORD_DISABLED=y\n"
            ), encoding="utf-8",
        )
        if model:
            model_dir = root / "models/wakenet_model" / model
            model_dir.mkdir(parents=True)
            (model_dir / "_MODEL_INFO_").write_bytes(b"model info")
            (model_dir / "weights").write_bytes(b"selected model weights")
        args = [sys.executable, str(ROOT / "scripts/build_default_assets.py"),
                "--sdkconfig", str(sdkconfig), "--base_assets", str(root / "base.bin"),
                "--output", str(root / "configured.bin"),
                "--esp_sr_model_path", str(root / "models"),
                "--name_length", str(name_length)]
        if max_size is not None:
            args.extend(["--max_size", str(max_size)])
        return subprocess.run(args, text=True, capture_output=True)

    def test_cli_replaces_models_and_preserves_custom_ui(self):
        for name_length, model in ((16, "wn9s_hiesp"), (32, "wn9_hiesp"),
                                   (64, "wn9s_nihaoxiaozhi")):
            with self.subTest(name_length=name_length), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                base, original = self.base_pack(root, name_length)
                before = base.read_bytes()
                result = self.run_builder(root, model, name_length)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                files = BUILD.read_assets_entries(root / "configured.bin", name_length)
                self.assertEqual(base.read_bytes(), before)
                for resource in ("font.bin", "image.bin", "layout.json"):
                    self.assertEqual(files[resource], original[resource])
                self.assertNotIn("old-speech.bin", files)
                index = json.loads(files["index.json"][0])
                original_index = json.loads(original["index.json"][0])
                self.assertEqual(index["emoji_collection"], original_index["emoji_collection"])
                self.assertEqual(index["text_font"], "font.bin")
                self.assertEqual(index["layout"], "layout.json")
                self.assertNotIn("multinet_model", index)
                models = files[index["srmodels"]][0]
                self.assertEqual(struct.unpack_from("<I", models)[0], 1)
                self.assertEqual(models[4:36].rstrip(b"\0").decode(), model)
                self.assertIn(b"selected model weights", models)
                self.assertNotIn(b"old Chinese model", models)

    def test_cli_disabled_removes_prebuilt_models_and_preserves_ui(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, original = self.base_pack(root)
            result = self.run_builder(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            files = BUILD.read_assets_entries(root / "configured.bin")
            self.assertNotIn("old-speech.bin", files)
            self.assertNotIn("srmodels.bin", files)
            self.assertNotIn("srmodels", json.loads(files["index.json"][0]))
            self.assertEqual(files["image.bin"], original["image.bin"])

    def test_cli_rejects_assets_that_exceed_partition_size(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.base_pack(root)
            result = self.run_builder(root, "wn9_hiesp", max_size=100)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exceeds partition limit", result.stderr)
            self.assertFalse((root / "configured.bin").exists())

    def test_reader_rejects_corrupt_and_truncated_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, _ = self.base_pack(root)
            valid = base.read_bytes()
            for corrupt in (b"short", valid[:-1], valid[:4] + b"\0" * 4 + valid[8:]):
                with self.subTest(corrupt=corrupt[:12]):
                    base.write_bytes(corrupt)
                    with self.assertRaises(ValueError):
                        BUILD.read_assets_entries(base)

    def test_cli_rejects_enabled_wake_word_without_a_selected_model(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.base_pack(root)
            self.run_builder(root)
            sdkconfig = root / "sdkconfig"
            sdkconfig.write_text("CONFIG_USE_AFE_WAKE_WORD=y\n", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "scripts/build_default_assets.py"),
                 "--sdkconfig", str(sdkconfig), "--output", str(root / "unused.bin")],
                text=True, capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires selected, available", result.stderr)
            self.assertFalse((root / "unused.bin").exists())


if __name__ == "__main__":
    unittest.main()
