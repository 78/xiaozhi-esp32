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
        self.assertIn(
            'BOARD_TYPE MATCHES "^[a-z0-9.-]+$"',
            cmake,
        )
        self.assertIn(
            'BOARD_NAME MATCHES "^[a-z0-9.-]+$"',
            cmake,
        )
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

    def test_reported_types_and_names_are_valid_and_unique(self):
        type_owners = {}
        name_owners = {}
        pair_owners = {}

        for config_path in sorted(
            (ROOT / "main/boards").rglob("config*.json")
        ):
            config = json.loads(config_path.read_text(encoding="utf-8"))
            board = config_path.parent.relative_to(
                ROOT / "main/boards"
            ).as_posix()
            board_type = build._get_reported_type(config)

            previous_board = type_owners.setdefault(board_type, board)
            self.assertEqual(
                previous_board,
                board,
                f"BOARD_TYPE {board_type!r} is used by multiple boards",
            )

            for build_config in config.get("builds", []):
                board_name = build._get_reported_name(build_config)
                self.assertNotIn(
                    board_name,
                    name_owners,
                    f"BOARD_NAME {board_name!r} is used by "
                    f"{name_owners.get(board_name)} and {config_path}",
                )
                name_owners[board_name] = config_path

                pair = (board_type, board_name)
                self.assertNotIn(
                    pair,
                    pair_owners,
                    f"reported board identity {pair!r} is duplicated",
                )
                pair_owners[pair] = config_path

    def test_language_and_wake_word_are_not_board_config_options(self):
        for config_path in sorted(
            (ROOT / "main/boards").rglob("config*.json")
        ):
            config = json.loads(config_path.read_text(encoding="utf-8"))
            for build_config in config.get("builds", []):
                for option in build_config.get("sdkconfig_append", []):
                    self.assertFalse(
                        option.startswith("CONFIG_LANGUAGE_")
                        or option.startswith("CONFIG_SR_WN_"),
                        f"{config_path}: configure language and wake word "
                        f"through menuconfig or build parameters, not {option}",
                    )

    def test_default_flash_options_are_not_repeated(self):
        def read_defaults(path):
            values = {}
            if not path.exists():
                return values
            for line in path.read_text(encoding="utf-8").splitlines():
                if line.startswith("CONFIG_") and "=" in line:
                    key, value = line.split("=", 1)
                    values[key] = value
            return values

        base_defaults = read_defaults(ROOT / "sdkconfig.defaults")
        for config_path in sorted(
            (ROOT / "main/boards").rglob("config*.json")
        ):
            config = json.loads(config_path.read_text(encoding="utf-8"))
            defaults = dict(base_defaults)
            defaults.update(
                read_defaults(
                    ROOT / f"sdkconfig.defaults.{config['target']}"
                )
            )
            selected_flash = next(
                (
                    key
                    for key, value in reversed(defaults.items())
                    if key.startswith("CONFIG_ESPTOOLPY_FLASHSIZE_")
                    and value == "y"
                ),
                None,
            )
            for build_config in config.get("builds", []):
                for option in build_config.get("sdkconfig_append", []):
                    if "=" not in option:
                        continue
                    key, value = option.split("=", 1)
                    if key.startswith("CONFIG_ESPTOOLPY_FLASHSIZE_"):
                        self.assertNotEqual(
                            (key, value),
                            (selected_flash, "y"),
                            f"{config_path}: {option} repeats the effective "
                            "project default",
                        )
                    elif key == "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME":
                        self.assertNotEqual(
                            defaults.get(key),
                            value,
                            f"{config_path}: {option} repeats the effective "
                            "project default",
                        )

    def test_chip_defaults_only_contain_target_overrides(self):
        def read_defaults(path):
            values = {}
            for line in path.read_text(encoding="utf-8").splitlines():
                if line.startswith("CONFIG_") and "=" in line:
                    key, value = line.split("=", 1)
                    values[key] = value
            return values

        base_defaults = read_defaults(ROOT / "sdkconfig.defaults")
        self.assertEqual(
            base_defaults["CONFIG_ESPTOOLPY_FLASHSIZE_16MB"],
            "y",
        )
        self.assertEqual(
            base_defaults["CONFIG_PARTITION_TABLE_CUSTOM_FILENAME"],
            '"partitions/v2/16m.csv"',
        )

        expected_partitions = {
            "esp32": '"partitions/v2/4m.csv"',
            "esp32c3": '"partitions/v2/16m_c3.csv"',
            "esp32c5": '"partitions/v2/16m.csv"',
            "esp32c6": '"partitions/v2/16m_c3.csv"',
            "esp32p4": '"partitions/v2/16m.csv"',
            "esp32s3": '"partitions/v2/16m.csv"',
            "esp32s31": '"partitions/v2/16m.csv"',
        }
        for target, expected_partition in expected_partitions.items():
            target_defaults = read_defaults(
                ROOT / f"sdkconfig.defaults.{target}"
            )
            duplicate_values = {
                key
                for key, value in target_defaults.items()
                if base_defaults.get(key) == value
            }
            self.assertFalse(
                duplicate_values,
                f"sdkconfig.defaults.{target} repeats base defaults: "
                f"{sorted(duplicate_values)}",
            )

            effective_defaults = dict(base_defaults)
            effective_defaults.update(target_defaults)
            self.assertEqual(
                effective_defaults[
                    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME"
                ],
                expected_partition,
            )

            if target == "esp32":
                self.assertEqual(
                    target_defaults[
                        "CONFIG_ESPTOOLPY_FLASHSIZE_4MB"
                    ],
                    "y",
                )
            else:
                self.assertFalse(
                    any(
                        key.startswith("CONFIG_ESPTOOLPY_FLASHSIZE_")
                        for key in target_defaults
                    ),
                    f"sdkconfig.defaults.{target} should inherit 16MB",
                )


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
    def test_duplicate_reported_identifiers_fail_collection(self):
        cases = (
            (
                ("type-a", "same-name"),
                ("type-b", "same-name"),
                "duplicate reported board name",
            ),
            (
                ("same-type", "name-a"),
                ("same-type", "name-b"),
                "duplicate reported board type",
            ),
        )
        for first, second, expected_error in cases:
            with self.subTest(expected_error=expected_error):
                with tempfile.TemporaryDirectory() as temp_dir:
                    boards = Path(temp_dir)
                    for directory, (board_type, board_name) in zip(
                        ("board-a", "board-b"),
                        (first, second),
                    ):
                        board_dir = boards / directory
                        board_dir.mkdir()
                        (board_dir / "config.json").write_text(
                            json.dumps({
                                "type": board_type,
                                "target": "esp32s3",
                                "builds": [{"name": board_name}],
                            }),
                            encoding="utf-8",
                        )
                    with mock.patch.object(build, "_BOARDS_DIR", boards):
                        with self.assertRaisesRegex(
                            ValueError,
                            expected_error,
                        ):
                            build._collect_variants(
                                idf_version=(6, 0, 1)
                            )

    def test_invalid_reported_identifiers_are_rejected(self):
        for value in (
            "bad_board",
            "Bad-Board",
            "bad board",
            "坏板子",
        ):
            with self.subTest(type=value):
                with self.assertRaisesRegex(ValueError, "only lowercase"):
                    build._get_reported_type({"type": value})
            with self.subTest(name=value):
                with self.assertRaisesRegex(ValueError, "only lowercase"):
                    build._get_reported_name({"name": value})

        for value in ("board", "board-1", "board.1", "1.2-3"):
            with self.subTest(valid=value):
                self.assertEqual(
                    build._get_reported_type({"type": value}),
                    value,
                )
                self.assertEqual(
                    build._get_reported_name({"name": value}),
                    value,
                )

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


class BuildOptionTests(unittest.TestCase):
    def test_supported_languages_match_kconfig_and_cmake(self):
        kconfig = (ROOT / "main/Kconfig.projbuild").read_text(
            encoding="utf-8"
        )
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        supported_languages = build._collect_languages()
        kconfig_languages = set(
            re.findall(r"^\s+config LANGUAGE_([A-Z_]+)$", kconfig, re.MULTILINE)
        )
        expected_symbols = {
            language.replace("-", "_").upper()
            for language in supported_languages
        }
        self.assertEqual(kconfig_languages, expected_symbols)
        for language in supported_languages:
            symbol = language.replace("-", "_").upper()
            self.assertIn(f"CONFIG_LANGUAGE_{symbol}", cmake)
            self.assertTrue(
                (ROOT / "main/assets/locales" / language).is_dir(),
                language,
            )

    def test_languages_are_discovered_from_build_configuration(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            cmake = root / "CMakeLists.txt"
            kconfig = root / "Kconfig.projbuild"
            locales = root / "locales"
            (locales / "xx-YY").mkdir(parents=True)
            cmake.write_text(
                'if(CONFIG_LANGUAGE_XX_YY)\n'
                '    set(LANG_DIR "xx-YY")\n'
                'endif()\n',
                encoding="utf-8",
            )
            kconfig.write_text(
                "config LANGUAGE_XX_YY\n"
                '    bool "Test language"\n',
                encoding="utf-8",
            )

            self.assertEqual(
                build._collect_languages(cmake, kconfig, locales),
                ["xx-YY"],
            )

    def test_language_normalization_and_option(self):
        self.assertEqual(
            build._language_sdkconfig_option("EN_us"),
            ("en-US", "CONFIG_LANGUAGE_EN_US=y"),
        )
        with self.assertRaisesRegex(ValueError, "Unsupported language"):
            build._language_sdkconfig_option("xx-YY")

    def test_wake_words_are_read_from_esp_sr_kconfig(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            kconfig = Path(temp_dir) / "Kconfig.projbuild"
            kconfig.write_text(
                'config SR_WN_WN9S_HELLO\n'
                '    bool "Hello (wn9s_hello)"\n'
                '\n'
                'config SR_WN_WN9_JARVIS_TTS\n'
                '    bool "Jarvis (wn9_jarvis_tts)"\n',
                encoding="utf-8",
            )
            wake_words = build._collect_wake_words(kconfig)

        self.assertEqual(
            [item["model"] for item in wake_words],
            ["wn9s_hello", "wn9_jarvis_tts"],
        )
        self.assertEqual(
            [item["phrase"] for item in wake_words],
            ["Hello", "Jarvis"],
        )
        self.assertIn("esp32c5", wake_words[0]["targets"])
        self.assertNotIn("esp32c5", wake_words[1]["targets"])

    def test_missing_esp_sr_kconfig_has_actionable_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            missing = Path(temp_dir) / "missing-kconfig"
            with self.assertRaisesRegex(
                RuntimeError,
                "idf.py reconfigure",
            ):
                build._collect_wake_words(missing)

    def test_wake_word_alias_selects_target_engine(self):
        model, options, symbols = build._wake_word_sdkconfig_options(
            "nihaoxiaozhi",
            "esp32c5",
        )
        self.assertEqual(model, "wn9s_nihaoxiaozhi")
        self.assertIn("CONFIG_USE_ESP_WAKE_WORD=y", options)
        self.assertIn("CONFIG_SR_WN_WN9S_NIHAOXIAOZHI=y", options)
        self.assertEqual(
            symbols,
            [
                "CONFIG_USE_ESP_WAKE_WORD",
                "CONFIG_SR_WN_WN9S_NIHAOXIAOZHI",
            ],
        )

        model, options, symbols = build._wake_word_sdkconfig_options(
            "nihaoxiaozhi",
            "esp32s3",
        )
        self.assertEqual(model, "wn9_nihaoxiaozhi_tts")
        self.assertIn("CONFIG_USE_AFE_WAKE_WORD=y", options)
        self.assertIn("CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y", options)
        self.assertEqual(
            symbols,
            [
                "CONFIG_USE_AFE_WAKE_WORD",
                "CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS",
            ],
        )

    def test_wake_word_disabled_removes_target_default_model(self):
        model, options, symbols = build._wake_word_sdkconfig_options(
            "disabled",
            "esp32s3",
        )
        self.assertEqual(model, "disabled")
        self.assertIn("CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=n", options)
        self.assertIn("CONFIG_USE_AFE_WAKE_WORD=n", options)
        self.assertIn("CONFIG_USE_ESP_WAKE_WORD=n", options)
        self.assertIn("CONFIG_WAKE_WORD_DISABLED=y", options)
        self.assertEqual(symbols, ["CONFIG_WAKE_WORD_DISABLED"])

    def test_incompatible_wake_word_model_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "WakeNet9s"):
            build._wake_word_sdkconfig_options(
                "wn9_jarvis_tts",
                "esp32c6",
            )
        with self.assertRaisesRegex(ValueError, "Invalid wake word"):
            build._wake_word_sdkconfig_options("jarvis", "esp32s3")

    def test_user_options_override_board_options(self):
        merged = build._merge_sdkconfig_options(
            [
                "CONFIG_BOARD_TYPE_TEST=y",
                "CONFIG_USE_ESP_WAKE_WORD=n",
            ],
            [
                "CONFIG_USE_ESP_WAKE_WORD=y",
                "CONFIG_SR_WN_WN9S_NIHAOXIAOZHI=y",
            ],
        )
        self.assertEqual(
            merged,
            [
                "CONFIG_BOARD_TYPE_TEST=y",
                "CONFIG_USE_ESP_WAKE_WORD=y",
                "CONFIG_SR_WN_WN9S_NIHAOXIAOZHI=y",
            ],
        )

    def test_configured_build_options_are_verified(self):
        previous_cwd = Path.cwd()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                os.chdir(temp_dir)
                Path("sdkconfig").write_text(
                    "CONFIG_LANGUAGE_EN_US=y\n",
                    encoding="utf-8",
                )
                build._validate_configured_symbols(
                    ["CONFIG_LANGUAGE_EN_US"],
                    "--language",
                )
                with self.assertRaisesRegex(ValueError, "Kconfig rejected"):
                    build._validate_configured_symbols(
                        ["CONFIG_SR_WN_UNKNOWN"],
                        "--wake-word",
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

    def test_no_arguments_prints_help_without_building(self):
        output = io.StringIO()
        with (
            mock.patch.object(build, "build_board") as build_board,
            contextlib.redirect_stdout(output),
        ):
            build.main([])

        build_board.assert_not_called()
        self.assertIn("usage:", output.getvalue())
        self.assertIn("--list-boards", output.getvalue())
        self.assertIn("--list-languages", output.getvalue())
        self.assertIn("--list-wake-words", output.getvalue())

    def test_list_boards_prints_boards_and_multi_variants(self):
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
            contextlib.redirect_stdout(output),
        ):
            build.main(["--list-boards"])

        self.assertEqual(
            output.getvalue(),
            "bread-compact-wifi\n"
            "multi-board\n"
            "  - variant-a\n"
            "  - variant-b\n",
        )

    def test_list_languages_supports_json(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            build.main(["--list-languages", "--json"])

        self.assertEqual(
            json.loads(output.getvalue()),
            build._collect_languages(),
        )

    def test_list_wake_words_supports_json(self):
        wake_words = [{
            "model": "wn9_jarvis_tts",
            "phrase": "Jarvis",
            "targets": ["esp32s3"],
        }]
        output = io.StringIO()
        with (
            mock.patch.object(
                build,
                "_collect_wake_words",
                return_value=wake_words,
            ),
            contextlib.redirect_stdout(output),
        ):
            build.main(["--list-wake-words", "--json"])

        self.assertEqual(json.loads(output.getvalue()), wake_words)

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
            language=None,
            wake_word=None,
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

    def test_language_and_wake_word_are_forwarded(self):
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
            build.main([
                "bread-compact-wifi",
                "--language",
                "en-US",
                "--wake-word",
                "wn9_jarvis_tts",
            ])

        self.assertEqual(build_board.call_args.kwargs["language"], "en-US")
        self.assertEqual(
            build_board.call_args.kwargs["wake_word"],
            "wn9_jarvis_tts",
        )


class BoardSourceTests(unittest.TestCase):
    def test_relative_board_includes_exist(self):
        missing = []
        boards_dir = ROOT / "main/boards"
        for source in boards_dir.rglob("*"):
            if source.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
                continue
            for line_number, line in enumerate(
                source.read_text(encoding="utf-8", errors="replace").splitlines(),
                1,
            ):
                match = re.match(
                    r'\s*#\s*include\s+"(\.\./[^"]+)"',
                    line,
                )
                if match and not (source.parent / match.group(1)).resolve().exists():
                    missing.append(
                        f"{source.relative_to(ROOT)}:{line_number}: {match.group(1)}"
                    )

        self.assertEqual(missing, [])


class CameraResolutionConfigTests(unittest.TestCase):
    def test_camera_interface_exposes_set_frame_size(self):
        camera_h = (ROOT / "main/boards/common/camera.h").read_text(encoding="utf-8")
        self.assertIn("SetFrameSize", camera_h)

        esp32_camera_h = (ROOT / "main/boards/common/esp32_camera.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("SetFrameSize", esp32_camera_h)
        self.assertIn("ParseFrameSize", esp32_camera_h)

        esp32_camera_cc = (ROOT / "main/boards/common/esp32_camera.cc").read_text(
            encoding="utf-8"
        )
        for name in ("QVGA", "VGA", "SVGA", "UXGA", "HD"):
            self.assertIn(f'"{name}"', esp32_camera_cc)

        mcp = (ROOT / "main/mcp_server.cc").read_text(encoding="utf-8")
        self.assertIn('Property("resolution", kPropertyTypeString', mcp)
        self.assertIn("SetFrameSize(resolution)", mcp)

    def test_atoms3r_board_default_frame_size_name(self):
        config_h = (
            ROOT
            / "main/boards/m5stack/atoms3r-cam-m12-echo-base/config.h"
        ).read_text(encoding="utf-8")
        self.assertIn("CAMERA_FRAME_SIZE_NAME", config_h)
        self.assertIn('"SVGA"', config_h)

        board_cc = (
            ROOT
            / "main/boards/m5stack/atoms3r-cam-m12-echo-base/atoms3r_cam_m12_echo_base.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("SetFrameSize(CAMERA_FRAME_SIZE_NAME)", board_cc)
        self.assertIn('SetFrameSize("VGA")', board_cc)


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
