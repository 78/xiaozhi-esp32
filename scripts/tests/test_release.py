import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("release", ROOT / "scripts/release.py")
release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(release)


class VersionTests(unittest.TestCase):
    def test_parse_and_match(self):
        self.assertEqual(release._parse_version("ESP-IDF v6.0.1"), (6, 0, 1))
        self.assertTrue(release._version_matches((5, 5, 4), "<6.0"))
        self.assertTrue(release._version_matches((6, 0, 1), ">=6.0"))
        with self.assertRaises(ValueError):
            release._version_matches((6, 0, 1), "~=6.0")

    def test_current_matrix_counts_and_uniqueness(self):
        idf5 = release._collect_variants(idf_version=(5, 5, 4))
        idf6 = release._collect_variants(idf_version=(6, 0, 1))
        self.assertEqual(len(idf5), 171)
        self.assertEqual(len(idf6), 157)
        for variants in (idf5, idf6):
            names = [variant["full_name"] for variant in variants]
            self.assertEqual(len(names), len(set(names)))


class BoardSelectionTests(unittest.TestCase):
    def setUp(self):
        self.variants = [
            {"board": "bread-compact-wifi", "name": "bread-compact-wifi", "full_name": "bread-compact-wifi"},
            {
                "board": "waveshare/esp32-c6-touch-amoled-2.06",
                "name": "esp32-c6-touch-amoled-2.06",
                "full_name": "waveshare-esp32-c6-touch-amoled-2.06",
            },
        ]

    def test_nested_manufacturer_board_path(self):
        selected = release._select_variants_for_changes(
            self.variants,
            ["main/boards/waveshare/esp32-c6-touch-amoled-2.06/config.h"],
        )
        self.assertEqual([item["board"] for item in selected], [self.variants[1]["board"]])

    def test_common_and_core_changes_select_all(self):
        for path in (
            "main/boards/common/board.cc",
            "main/application.cc",
            "components/esp-ml307/src/at_modem.cc",
            "scripts/build_default_assets.py",
            "scripts/release.py",
        ):
            with self.subTest(path=path):
                self.assertEqual(
                    release._select_variants_for_changes(self.variants, [path]),
                    self.variants,
                )

    def test_docs_only_selects_none(self):
        self.assertEqual(
            release._select_variants_for_changes(self.variants, ["docs/readme.md"]),
            [],
        )


class InvalidConfigTests(unittest.TestCase):
    def test_invalid_version_rule_fails_collection(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            boards = Path(temp_dir)
            board_dir = boards / "bad-board"
            board_dir.mkdir()
            (board_dir / "config.json").write_text(json.dumps({
                "target": "esp32s3",
                "builds": [{
                    "name": "bad-board",
                    "idf_version": "~=6.0",
                }],
            }), encoding="utf-8")
            with mock.patch.object(release, "_BOARDS_DIR", boards):
                with self.assertRaisesRegex(ValueError, "Invalid ESP-IDF version expression"):
                    release._collect_variants(idf_version=(6, 0, 1))


if __name__ == "__main__":
    unittest.main()
