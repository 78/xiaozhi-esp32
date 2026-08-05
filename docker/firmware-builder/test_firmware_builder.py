import importlib.util
import json
import os
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("firmware_builder.py")
SPEC = importlib.util.spec_from_file_location("firmware_builder", MODULE_PATH)
assert SPEC and SPEC.loader
firmware_builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(firmware_builder)


class FirmwareBuilderTest(unittest.TestCase):
    def create_source(self, directory: Path, *, exit_code: int = 0) -> Path:
        source = directory / "source"
        scripts = source / "scripts"
        scripts.mkdir(parents=True)
        board = source / "main" / "boards" / "xmini" / "c3"
        board.mkdir(parents=True)
        (source / "CMakeLists.txt").write_text(
            'set(PROJECT_VER "9.8.7")\n', encoding="utf-8"
        )
        (board / "config.json").write_text(
            json.dumps(
                {
                    "type": "xmini-c3",
                    "target": "esp32c3",
                    "builds": [{"name": "xmini-c3"}],
                }
            ),
            encoding="utf-8",
        )
        (scripts / "build.py").write_text(
            """
import pathlib
import sys

print("fake compiler output")
if %d == 0:
    build = pathlib.Path("build")
    build.mkdir(exist_ok=True)
    (build / "xiaozhi.bin").write_bytes(b"ota")
    (build / "merged-binary.bin").write_bytes(b"full")
sys.exit(%d)
"""
            % (exit_code, exit_code),
            encoding="utf-8",
        )
        return source

    def test_success_writes_artifacts_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.create_source(root)
            output = root / "output"
            with patch.dict(os.environ, {"FIRMWARE_SOURCE_REVISION": "abc123"}):
                exit_code = firmware_builder.main(
                    [
                        "--board-dir",
                        "xmini/c3",
                        "--board-name",
                        "xmini-c3",
                        "--language",
                        "zh-CN",
                        "--wake-word",
                        "nihaoxiaozhi",
                        "--build-options-json",
                        '{"wifi_provisioning":"blufi","multiline_chat":false}',
                        "--source-dir",
                        str(source),
                        "--output-dir",
                        str(output),
                    ]
                )

            self.assertEqual(exit_code, 0)
            self.assertEqual((output / "xiaozhi.bin").read_bytes(), b"ota")
            self.assertEqual((output / "merged-binary.bin").read_bytes(), b"full")
            self.assertIn("fake compiler output", (output / "build.log").read_text())
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["schema_version"], 1)
            self.assertEqual(manifest["status"], "succeeded")
            self.assertEqual(manifest["firmware_version"], "9.8.7")
            self.assertEqual(manifest["firmware_source_revision"], "abc123")
            self.assertEqual(manifest["board_dir"], "xmini/c3")
            self.assertEqual(manifest["board_type"], "xmini-c3")
            self.assertEqual(manifest["board_name"], "xmini-c3")
            self.assertEqual(
                manifest["build_options"],
                {"wifi_provisioning": "blufi", "multiline_chat": False},
            )
            build_log = (output / "build.log").read_text(encoding="utf-8")
            self.assertIn("--build-options-json", build_log)
            self.assertIn(
                '{\\"multiline_chat\\":false,\\"wifi_provisioning\\":\\"blufi\\"}',
                build_log,
            )
            self.assertNotIn("variant", manifest)
            self.assertTrue(manifest["runtime_architecture"])
            self.assertGreaterEqual(manifest["runtime_cpu_count"], 1)
            self.assertEqual(
                {item["kind"] for item in manifest["artifacts"]}, {"ota", "full"}
            )

    def test_failed_build_preserves_log_and_failed_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.create_source(root, exit_code=7)
            output = root / "output"
            exit_code = firmware_builder.main(
                [
                    "--board-dir",
                    "xmini/c3",
                    "--board-name",
                    "xmini-c3",
                    "--language",
                    "en-US",
                    "--wake-word",
                    "disabled",
                    "--source-dir",
                    str(source),
                    "--output-dir",
                    str(output),
                ]
            )

            self.assertEqual(exit_code, 7)
            self.assertTrue((output / "build.log").is_file())
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["status"], "failed")
            self.assertEqual(manifest["exit_code"], 7)

    def test_success_uploads_outputs_and_manifest_last(self) -> None:
        uploads: list[tuple[str, str]] = []

        class FakeBucket:
            def __init__(self, auth: object, endpoint: str, bucket: str) -> None:
                self.auth = auth
                self.endpoint = endpoint
                self.bucket = bucket

            def put_object_from_file(self, key: str, path: str) -> None:
                uploads.append((key, path))

        fake_oss2 = types.SimpleNamespace(
            Auth=lambda access_key_id, access_key_secret: (
                access_key_id,
                access_key_secret,
            ),
            Bucket=FakeBucket,
        )
        upload_env = {
            "FIRMWARE_OSS_UPLOAD": "true",
            "FIRMWARE_OSS_ENDPOINT": "oss-cn-shenzhen.aliyuncs.com",
            "FIRMWARE_OSS_PREFIX": "custom_firmwares",
            "OSS_BUCKET_NAME": "test-bucket",
            "OSS_ACCESS_KEY_ID": "test-id",
            "OSS_ACCESS_KEY_SECRET": "test-secret",
        }

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.create_source(root)
            output = root / "output"
            with (
                patch.dict(os.environ, upload_env),
                patch.dict(sys.modules, {"oss2": fake_oss2}),
            ):
                exit_code = firmware_builder.main(
                    [
                        "--board-dir",
                        "xmini/c3",
                        "--board-name",
                        "xmini-c3",
                        "--language",
                        "zh-CN",
                        "--wake-word",
                        "nihaoxiaozhi",
                        "--job-id",
                        "test-job",
                        "--source-dir",
                        str(source),
                        "--output-dir",
                        str(output),
                    ]
                )

            self.assertEqual(exit_code, 0)
            self.assertEqual(len(uploads), 4)
            self.assertEqual(uploads[-1][0], "custom_firmwares/test-job/manifest.json")
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["delivery_status"], "succeeded")
            self.assertNotIn("test-secret", json.dumps(manifest))

    def test_rejects_path_traversal_board(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = self.create_source(Path(temporary))
            exit_code = firmware_builder.main(
                [
                    "--board-dir",
                    "../xmini-c3",
                    "--board-name",
                    "xmini-c3",
                    "--language",
                    "en-US",
                    "--wake-word",
                    "disabled",
                    "--source-dir",
                    str(source),
                ]
            )
            self.assertEqual(exit_code, 2)

    def test_rejects_unknown_board_name(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = self.create_source(Path(temporary))
            exit_code = firmware_builder.main(
                [
                    "--board-dir",
                    "xmini/c3",
                    "--board-name",
                    "wrong-name",
                    "--language",
                    "en-US",
                    "--wake-word",
                    "disabled",
                    "--source-dir",
                    str(source),
                ]
            )
            self.assertEqual(exit_code, 2)

    def test_rejects_invalid_build_options(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = self.create_source(Path(temporary))
            exit_code = firmware_builder.main(
                [
                    "--board-dir",
                    "xmini/c3",
                    "--board-name",
                    "xmini-c3",
                    "--language",
                    "en-US",
                    "--wake-word",
                    "disabled",
                    "--build-options-json",
                    '["not-an-object"]',
                    "--source-dir",
                    str(source),
                ]
            )
            self.assertEqual(exit_code, 2)


if __name__ == "__main__":
    unittest.main()
