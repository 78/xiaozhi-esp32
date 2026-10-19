import importlib.util
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/spiffs_assets/spiffs_assets_gen.py"


@unittest.skipUnless(
    all(importlib.util.find_spec(name) for name in ("numpy", "PIL", "packaging")),
    "requires numpy, Pillow and packaging",
)
class SpiffsAssetsEncodingTests(unittest.TestCase):
    def build_assets(self, directory, asset_name="assets", ensure_ascii=False):
        root = Path(directory)
        root.mkdir(exist_ok=True)
        assets = root / asset_name
        assets.mkdir()
        (assets / "tone.bin").write_bytes(b"\x01\x02\x03")
        output = root / "output/assets.bin"
        config = {
            "assets_path": str(assets),
            "image_file": str(output),
            "include_path": str(root / "include"),
            "name_length": "32",
            "split_height": "0",
            "support_format": ".bin",
            "support_spng": False,
            "support_sjpg": False,
            "support_qoi": False,
            "support_sqoi": False,
            "support_raw": False,
            "assets_size": "0x400000",
        }
        config_path = root / "config.json"
        config_path.write_text(
            json.dumps(config, ensure_ascii=ensure_ascii), encoding="utf-8"
        )
        # Change the text locale after startup so filesystem paths retain their
        # Unicode support, while implicit open() uses a non-UTF-8 encoding.
        runner = (
            "import locale, runpy, sys; "
            "locale.setlocale(locale.LC_CTYPE, 'C'); "
            "sys.argv = sys.argv[1:]; "
            "runpy.run_path(sys.argv[0], run_name='__main__')"
        )
        result = subprocess.run(
            [sys.executable, "-X", "utf8=0", "-c", runner,
             str(GENERATOR), "--config", str(config_path)],
            env={**os.environ, "PYTHONIOENCODING": "utf-8"},
            capture_output=True, text=True, encoding="utf-8", timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = output.read_bytes()
        count, checksum, length = struct.unpack("<III", data[:12])
        self.assertEqual((count, length), (1, 49))
        self.assertEqual(checksum, sum(data[12:]) & 0xFFFF)
        self.assertEqual(data[12:44], b"tone.bin" + b"\0" * 24)
        self.assertEqual(data[44:56], struct.pack("<IIHH", 3, 0, 0, 0))
        self.assertEqual(data[56:], b"\x5a\x5a\x01\x02\x03")
        return root / "include" / f"mmap_generate_{asset_name}.h"

    def test_cli_reads_utf8_config_under_non_utf8_locale(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertTrue(self.build_assets(Path(directory) / "项目").is_file())

    def test_cli_writes_utf8_header_under_non_utf8_locale(self):
        with tempfile.TemporaryDirectory() as directory:
            # Escaping the JSON isolates header writing from config decoding.
            header = self.build_assets(directory, "资源", ensure_ascii=True)
            self.assertIn("MMAP_资源_TONE_BIN", header.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
