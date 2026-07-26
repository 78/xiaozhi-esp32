import importlib.util
import json
import os
import contextlib
import io
import re
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("build", ROOT / "scripts/build.py")
build = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(build)


class VersionTests(unittest.TestCase):
    def test_parse_and_match(self):
        self.assertEqual(build._parse_version("ESP-IDF v6.0.1"), (6, 0, 1))
        self.assertTrue(build._version_matches((5, 5, 4), "<6.0"))
        self.assertTrue(build._version_matches((6, 0, 1), ">=6.0"))
        with self.assertRaises(ValueError):
            build._version_matches((6, 0, 1), "~=6.0")

    def test_current_matrix_uniqueness_and_p4_variants(self):
        idf5 = build._collect_variants(idf_version=(5, 5, 4))
        idf6 = build._collect_variants(idf_version=(6, 0, 1))
        idf61 = build._collect_variants(idf_version=(6, 1, 0))
        for variants in (idf5, idf6, idf61):
            names = [variant["full_name"] for variant in variants]
            self.assertEqual(len(names), len(set(names)))

        idf6_names = {variant["full_name"] for variant in idf6}
        self.assertIn("espressif-esp32-p4-function-ev-board", idf6_names)
        self.assertIn("espressif-esp32-p4x-function-ev-board", idf6_names)
        self.assertNotIn("espressif-esp-p4-function-ev-board", idf6_names)
        self.assertNotIn("espressif-esp-p4-function-ev-board-p4x", idf6_names)
        self.assertIn("waveshare-esp32-p4x-nano-10.1-a", idf6_names)
        self.assertIn("waveshare-esp32-p4x-wifi6-touch-lcd-10.1", idf6_names)
        self.assertNotIn("waveshare-esp32-p4-nano-10.1-a-p4x", idf6_names)
        self.assertNotIn("espressif-esp32-s31-function-coreboard-1", idf6_names)
        self.assertIn("alientek-atk-dnesp32s3", idf6_names)
        self.assertNotIn("atk-dnesp32s3", idf6_names)
        self.assertNotIn("alientek-alientek-atk-dnesp32s3", idf6_names)
        self.assertIn("m5stack-atom-echos3r", idf6_names)
        self.assertIn("nologo-xingzhi-abs-2.0", idf6_names)
        self.assertIn("spotpear-sp-esp32-s3-1.28-box", idf6_names)
        self.assertIn("dfrobot-df-k10", idf6_names)
        self.assertIn("xorigin-aipi-lite", idf6_names)
        self.assertIn("kevin-box-2", idf6_names)
        self.assertNotIn("kevin-kevin-box-2", idf6_names)
        self.assertIn("labplus-ledong-v2", idf6_names)
        self.assertNotIn("labplus-labplus-ledong-v2", idf6_names)
        self.assertIn("lckfb-lichuang-dev", idf6_names)
        self.assertIn("lckfb-lichuang-c3-dev", idf6_names)
        self.assertIn("wdmomo-esp32-cgc", idf6_names)
        self.assertIn("wdmomo-esp32-cgc-144", idf6_names)
        self.assertNotIn("esp32-cgc", idf6_names)
        self.assertNotIn("esp32-cgc-144", idf6_names)

        idf61_names = {variant["full_name"] for variant in idf61}
        self.assertIn("espressif-esp32-s31-function-coreboard-1", idf61_names)
        self.assertIn("rymcu-bigsmart", idf61_names)
        self.assertNotIn("rymcu-rymcu-bigsmart", idf61_names)
        aipi_en = json.loads(
            (
                ROOT / "main/boards/xorigin/aipi-lite/config_en.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(aipi_en["manufacturer"], "xorigin")
        self.assertEqual(aipi_en["type"], "aipi-lite")
        self.assertEqual(
            build._get_release_full_name(
                aipi_en["manufacturer"],
                aipi_en["builds"][0],
            ),
            "xorigin-aipi-lite_en",
        )
        self.assertEqual(
            build._get_release_full_name(
                "espressif",
                {"name": "esp-box-3"},
            ),
            "espressif-esp-box-3",
        )
        self.assertEqual(
            build._get_release_full_name(
                "espressif",
                {"name": "esp32-p4x-function-ev-board"},
            ),
            "espressif-esp32-p4x-function-ev-board",
        )
        self.assertEqual(
            build._normalize_p4x_release_name("m5stack-tab5-p4x"),
            "m5stack-tab5-p4x",
        )

        for config_path in (ROOT / "main/boards").rglob("config.json"):
            config = json.loads(config_path.read_text(encoding="utf-8"))
            self.assertEqual(build._get_reported_type(config), config["type"])
            self.assertNotIn("board_name", config, config_path)
            self.assertNotIn("release_name", config, config_path)
            board = config_path.parent.relative_to(ROOT / "main/boards").as_posix()
            self.assertTrue(build._board_type_exists(board), config_path)
            for build_config in config.get("builds", []):
                self.assertNotIn("board_name", build_config, config_path)
                self.assertNotIn("release_name", build_config, config_path)

        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("set(MANUFACTURER", cmake)
        self.assertIn('BOARD_MANUFACTURER=\\"${BOARD_MANUFACTURER}\\"', cmake)
        for source_name in (
            "wifi_board.cc",
            "ml307_board.cc",
            "nt26_board.cc",
            "rndis_board.cc",
            "ethernet_board.cc",
        ):
            source = (
                ROOT / "main/boards/common" / source_name
            ).read_text(encoding="utf-8")
            self.assertIn("manufacturer", source, source_name)
            self.assertIn("BOARD_MANUFACTURER", source, source_name)


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
        selected = build._select_variants_for_changes(
            self.variants,
            ["main/boards/waveshare/esp32-c6-touch-amoled-2.06/config.h"],
        )
        self.assertEqual([item["board"] for item in selected], [self.variants[1]["board"]])

    def test_official_directory_can_keep_legacy_board_type(self):
        board = "espressif/esp32-s3-box-3"
        self.assertTrue(build._board_type_exists(board))
        self.assertEqual(
            build._resolve_board_config(board, "esp32s3", []),
            "CONFIG_BOARD_TYPE_ESP32_S3_BOX_3",
        )

    def test_m5stack_directory_can_omit_manufacturer_prefix(self):
        board = "m5stack/cardputer-adv"
        self.assertTrue(build._board_type_exists(board))
        self.assertEqual(
            build._resolve_board_config(board, "esp32s3", []),
            "CONFIG_BOARD_TYPE_M5STACK_CARDPUTER_ADV",
        )

    def test_new_manufacturer_directories_keep_existing_board_types(self):
        cases = {
            "xorigin/aipi-lite": "CONFIG_BOARD_TYPE_XORIGIN_AIPI_LITE",
            "kevin/box-2": "CONFIG_BOARD_TYPE_KEVIN_BOX_2",
            "labplus/ledong-v2": "CONFIG_BOARD_TYPE_LABPLUS_LEDONG_V2",
            "lckfb/szpi-esp32s3": "CONFIG_BOARD_TYPE_LICHUANG_DEV_S3",
            "lckfb/szpi-esp32c3": "CONFIG_BOARD_TYPE_LICHUANG_DEV_C3",
            "wdmomo/esp32-cgc": "CONFIG_BOARD_TYPE_WDMOMO_CGC",
            "wdmomo/esp32-cgc-144": "CONFIG_BOARD_TYPE_WDMOMO_CGC_144",
        }
        for board, expected in cases.items():
            with self.subTest(board=board):
                self.assertTrue(build._board_type_exists(board))
                config = json.loads(
                    (ROOT / "main/boards" / board / "config.json").read_text(
                        encoding="utf-8"
                    )
                )
                self.assertEqual(
                    build._resolve_board_config(
                        board,
                        config["target"],
                        config["builds"][0].get("sdkconfig_append", []),
                    ),
                    expected,
                )

    def test_alientek_directory_keeps_atk_board_types(self):
        cases = {
            "alientek/atk-dnesp32s3": (
                "CONFIG_BOARD_TYPE_ATK_DNESP32S3",
                "atk-dnesp32s3",
            ),
            "alientek/atk-dnesp32s3m-wifi": (
                "CONFIG_BOARD_TYPE_ATK_DNESP32S3M_WIFI",
                "atk-dnesp32s3m-wifi",
            ),
            "alientek/atk-dnesp32s3m-4g": (
                "CONFIG_BOARD_TYPE_ATK_DNESP32S3M_4G",
                "atk-dnesp32s3m-4g",
            ),
        }
        for board, (expected_config, expected_type) in cases.items():
            with self.subTest(board=board):
                config = json.loads(
                    (ROOT / "main/boards" / board / "config.json").read_text(
                        encoding="utf-8"
                    )
                )
                self.assertEqual(config["manufacturer"], "alientek")
                self.assertEqual(config["type"], expected_type)
                self.assertEqual(
                    build._resolve_board_config(
                        board,
                        config["target"],
                        config.get("sdkconfig_append", []),
                    ),
                    expected_config,
                )

    def test_same_leaf_names_are_scoped_by_manufacturer(self):
        self.assertEqual(
            build._resolve_board_config("magiclick/c3", "esp32c3", []),
            "CONFIG_BOARD_TYPE_MAGICLICK_C3",
        )
        self.assertEqual(
            build._resolve_board_config("xmini/c3", "esp32c3", []),
            "CONFIG_BOARD_TYPE_XMINI_C3",
        )

    def test_common_and_core_changes_select_all(self):
        for path in (
            "main/boards/common/board.cc",
            "main/application.cc",
            "components/esp-ml307/src/at_modem.cc",
            "scripts/build_default_assets.py",
            "scripts/build.py",
        ):
            with self.subTest(path=path):
                self.assertEqual(
                    build._select_variants_for_changes(self.variants, [path]),
                    self.variants,
                )

    def test_docs_only_selects_none(self):
        self.assertEqual(
            build._select_variants_for_changes(self.variants, ["docs/readme.md"]),
            [],
        )


class BoardMenuTests(unittest.TestCase):
    def test_board_menu_is_sorted_and_matches_cmake(self):
        kconfig = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        choice = kconfig.split("choice BOARD_TYPE\n", 1)[1].split(
            "endchoice\n", 1
        )[0]
        entries = re.findall(
            r'^    config (BOARD_TYPE_[A-Za-z0-9_]+)\n'
            r'        bool "([^"]+)"\n'
            r'        depends on (IDF_TARGET_[A-Za-z0-9_]+)$',
            choice,
            re.MULTILINE,
        )
        symbols = [symbol for symbol, _, _ in entries]
        labels = [label for _, label, _ in entries]

        self.assertEqual(len(symbols), len(set(symbols)))
        self.assertEqual(len(labels), len(set(labels)))
        self.assertEqual(
            set(symbols),
            set(
                re.findall(
                    r"(?:if|elseif)\(CONFIG_(BOARD_TYPE_[A-Za-z0-9_]+)\)",
                    cmake,
                )
            ),
        )

        def natural_key(label):
            label = re.sub(
                r"\s+\([^)]*[\u3400-\u9fff][^)]*\)$",
                "",
                label.casefold(),
            )
            return tuple(
                (1, float(part))
                if re.fullmatch(r"\d+(?:\.\d+)?", part)
                else (0, part)
                for part in re.split(r"(\d+(?:\.\d+)?)", label)
            )

        self.assertEqual(labels, sorted(labels, key=natural_key))
        for label in labels:
            if re.search(r"[\u3400-\u9fff]", label):
                self.assertRegex(
                    label,
                    r"^[^\u3400-\u9fff]+\([^)]*[\u3400-\u9fff][^)]*\)$",
                )

        for stale_label in (
            "ALIENTEK",
            "AMOLOED",
            "AIPI-Lite",
            "NOLOGO",
            "NULLLAB-AI-VOX3",
            "StopWatch",
            "electronBot",
            "ottoRobot",
        ):
            self.assertNotIn(stale_label, choice)

    def test_board_menu_has_explicit_target_defaults(self):
        kconfig = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
        choice = kconfig.split("choice BOARD_TYPE\n", 1)[1].split(
            "endchoice\n", 1
        )[0]
        expected = {
            "IDF_TARGET_ESP32": "BOARD_TYPE_BREAD_COMPACT_ESP32",
            "IDF_TARGET_ESP32C3": "BOARD_TYPE_XMINI_C3_V3",
            "IDF_TARGET_ESP32C5": "BOARD_TYPE_ESP_SENSAIRSHUTTLE",
            "IDF_TARGET_ESP32C6": (
                "BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_AMOLED_2_06"
            ),
            "IDF_TARGET_ESP32S3": "BOARD_TYPE_BREAD_COMPACT_WIFI",
            "IDF_TARGET_ESP32P4": (
                "BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD"
            ),
            "IDF_TARGET_ESP32S31": (
                "BOARD_TYPE_ESP32_S31_FUNCTION_COREBOARD_1"
            ),
        }
        for target, symbol in expected.items():
            self.assertIn(f"default {symbol} if {target}", choice)


class InvalidConfigTests(unittest.TestCase):
    def test_missing_reported_type_fails_collection(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            boards = Path(temp_dir)
            board_dir = boards / "bad-board"
            board_dir.mkdir()
            (board_dir / "config.json").write_text(json.dumps({
                "target": "esp32s3",
                "builds": [{"name": "bad-board"}],
            }), encoding="utf-8")
            with mock.patch.object(build, "_BOARDS_DIR", boards):
                with self.assertRaisesRegex(ValueError, 'top-level "type"'):
                    build._collect_variants(idf_version=(6, 0, 1))

    def test_invalid_version_rule_fails_collection(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            boards = Path(temp_dir)
            board_dir = boards / "bad-board"
            board_dir.mkdir()
            (board_dir / "config.json").write_text(json.dumps({
                "type": "bad-board",
                "target": "esp32s3",
                "builds": [{
                    "name": "bad-board",
                    "idf_version": "~=6.0",
                }],
            }), encoding="utf-8")
            with mock.patch.object(build, "_BOARDS_DIR", boards):
                with self.assertRaisesRegex(ValueError, "Invalid ESP-IDF version expression"):
                    build._collect_variants(idf_version=(6, 0, 1))


class PreviewTargetTests(unittest.TestCase):
    def test_merge_bin_enables_preview_mode(self):
        with mock.patch.object(build, "_run_idf") as run_idf:
            build.merge_bin(preview=True)

        run_idf.assert_called_once_with("merge-bin", preview=True)


class TargetConfigurationTests(unittest.TestCase):
    def test_same_target_skips_set_target(self):
        with (
            mock.patch.object(build, "_configured_target", return_value="esp32s3"),
            mock.patch.object(build, "_run_idf") as run_idf,
        ):
            changed = build._ensure_target("esp32s3", preview=False)

        run_idf.assert_not_called()
        self.assertFalse(changed)

    def test_changed_target_calls_set_target(self):
        with (
            mock.patch.object(build, "_configured_target", return_value="esp32c3"),
            mock.patch.object(build, "_run_idf") as run_idf,
        ):
            changed = build._ensure_target("esp32s3", preview=True)

        run_idf.assert_called_once_with(
            "set-target",
            "esp32s3",
            preview=True,
        )
        self.assertTrue(changed)

    def test_regenerate_sdkconfig_uses_clean_variant_fragment(self):
        previous_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                os.chdir(temp_dir)
                Path("sdkconfig").write_text(
                    'CONFIG_IDF_TARGET="esp32s3"\nCONFIG_OLD_VARIANT=y\n',
                    encoding="utf-8",
                )
                Path("sdkconfig.defaults").write_text(
                    "CONFIG_PROJECT_DEFAULT=y\n",
                    encoding="utf-8",
                )

                with mock.patch.object(build, "_run_idf") as run_idf:
                    build._regenerate_sdkconfig(
                        "esp32s3",
                        ["CONFIG_BOARD_TYPE_TEST=y", "CONFIG_FEATURE=y"],
                        preview=False,
                    )

                self.assertFalse(Path("sdkconfig").exists())
                self.assertIn(
                    "CONFIG_OLD_VARIANT=y",
                    Path("sdkconfig.old").read_text(encoding="utf-8"),
                )
                fragment = Path("build/xiaozhi-build.sdkconfig.defaults")
                self.assertEqual(
                    fragment.read_text(encoding="utf-8"),
                    "# Generated by scripts/build.py\n"
                    "CONFIG_BOARD_TYPE_TEST=y\n"
                    "CONFIG_FEATURE=y\n",
                )
                run_idf.assert_called_once_with(
                    "-DIDF_TARGET=esp32s3",
                    "-DSDKCONFIG_DEFAULTS="
                    "sdkconfig.defaults;build/xiaozhi-build.sdkconfig.defaults",
                    "reconfigure",
                    preview=False,
                )
        finally:
            os.chdir(previous_cwd)

    def test_target_change_preserves_set_target_backup(self):
        previous_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                os.chdir(temp_dir)
                Path("sdkconfig").write_text(
                    'CONFIG_IDF_TARGET="esp32s3"\n',
                    encoding="utf-8",
                )
                Path("sdkconfig.old").write_text(
                    "CONFIG_USER_PREVIOUS_VALUE=y\n",
                    encoding="utf-8",
                )

                with mock.patch.object(build, "_run_idf"):
                    build._regenerate_sdkconfig(
                        "esp32s3",
                        ["CONFIG_BOARD_TYPE_TEST=y"],
                        preview=False,
                        target_changed=True,
                    )

                self.assertFalse(Path("sdkconfig").exists())
                self.assertEqual(
                    Path("sdkconfig.old").read_text(encoding="utf-8"),
                    "CONFIG_USER_PREVIOUS_VALUE=y\n",
                )
        finally:
            os.chdir(previous_cwd)


class VariantSelectionTests(unittest.TestCase):
    def setUp(self):
        self.variants = [
            {"board": "test-board", "name": "variant-a", "full_name": "variant-a"},
            {"board": "test-board", "name": "variant-b", "full_name": "variant-b"},
        ]

    def test_non_interactive_selection_requires_name(self):
        stdin = mock.Mock()
        stdin.isatty.return_value = False
        with (
            mock.patch.object(build.sys, "stdin", stdin),
            self.assertRaisesRegex(SystemExit, "2"),
        ):
            build._select_variant("test-board", self.variants)

    def test_interactive_selection_accepts_number(self):
        stdin = mock.Mock()
        stdin.isatty.return_value = True
        with (
            mock.patch.object(build.sys, "stdin", stdin),
            mock.patch("builtins.input", return_value="2"),
        ):
            selected = build._select_variant("test-board", self.variants)

        self.assertEqual(selected, "variant-b")


class CliTests(unittest.TestCase):
    def setUp(self):
        self.variants = [
            {
                "board": "bread-compact-wifi",
                "name": "bread-compact-wifi",
                "full_name": "bread-compact-wifi",
            },
            {
                "board": "multi-board",
                "name": "variant-a",
                "full_name": "variant-a",
            },
            {
                "board": "multi-board",
                "name": "variant-b",
                "full_name": "variant-b",
            },
        ]

    def test_no_arguments_lists_boards_without_building(self):
        output = io.StringIO()
        with (
            mock.patch.object(
                build,
                "_detect_idf_version_for_listing",
                return_value=(6, 0, 2),
            ),
            mock.patch.object(
                build,
                "_collect_variants",
                return_value=self.variants,
            ),
            mock.patch.object(build, "build_board") as build_board,
            contextlib.redirect_stdout(output),
        ):
            build.main([])

        build_board.assert_not_called()
        self.assertEqual(
            output.getvalue(),
            "bread-compact-wifi\n"
            "multi-board\n"
            "  - variant-a\n"
            "  - variant-b\n",
        )

    def test_build_does_not_create_zip_by_default(self):
        with (
            mock.patch.object(build, "_detect_idf_version", return_value=(6, 0, 2)),
            mock.patch.object(build, "_board_type_exists", return_value=True),
            mock.patch.object(
                build,
                "_collect_variants",
                return_value=self.variants,
            ),
            mock.patch.object(build, "build_board") as build_board,
        ):
            build.main(["bread-compact-wifi"])

        build_board.assert_called_once_with(
            "bread-compact-wifi",
            config_filename="config.json",
            name_filter="bread-compact-wifi",
            create_zip=False,
            idf_version=(6, 0, 2),
        )

    def test_zip_flag_is_forwarded(self):
        with (
            mock.patch.object(build, "_detect_idf_version", return_value=(6, 0, 2)),
            mock.patch.object(build, "_board_type_exists", return_value=True),
            mock.patch.object(
                build,
                "_collect_variants",
                return_value=self.variants,
            ),
            mock.patch.object(build, "build_board") as build_board,
        ):
            build.main(["bread-compact-wifi", "--zip"])

        self.assertTrue(build_board.call_args.kwargs["create_zip"])


class ZipTests(unittest.TestCase):
    def test_zip_is_always_recreated(self):
        previous_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                os.chdir(temp_dir)
                Path("build").mkdir()
                Path("build/merged-binary.bin").write_bytes(b"new firmware")
                Path("releases").mkdir()
                output = Path("releases/v1.2.3_test-board.zip")
                output.write_bytes(b"stale zip")

                build.zip_bin("test-board", "1.2.3")

                with build.zipfile.ZipFile(output) as archive:
                    self.assertEqual(
                        archive.read("merged-binary.bin"),
                        b"new firmware",
                    )
        finally:
            os.chdir(previous_cwd)


if __name__ == "__main__":
    unittest.main()
