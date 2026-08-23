import importlib.util
import json
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
    def test_local_commands_are_detected_and_use_multinet_pinyin(self):
        with tempfile.TemporaryDirectory() as directory:
            sdkconfig = Path(directory) / "sdkconfig"
            sdkconfig.write_text(
                "CONFIG_ENABLE_LOCAL_COMMANDS=y\n"
                "CONFIG_SR_MN_CN_MULTINET6_QUANT=y\n",
                encoding="utf-8",
            )
            self.assertTrue(BUILD.read_local_commands_enabled(sdkconfig))
            self.assertEqual(len(BUILD.LOCAL_COMMANDS), 7)
            self.assertEqual(
                BUILD.LOCAL_COMMANDS[2],
                {
                    "command": "shi yong si ji wang luo",
                    "text": "使用四G网络",
                    "action": "network_cellular",
                },
            )

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


if __name__ == "__main__":
    unittest.main()
