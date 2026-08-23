import pathlib
import tempfile
import unittest

from scripts import package_kevin_box2_release


class KevinBoxReleasePackageTest(unittest.TestCase):
    def test_package_contains_hashes_offsets_and_validation_status(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = root / "build"
            (build / "bootloader").mkdir(parents=True)
            (build / "partition_table").mkdir()
            for index, (_name, (source, _offset, _purpose)) in enumerate(
                package_kevin_box2_release.ARTIFACTS.items(), start=1
            ):
                path = build / source
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(bytes([index]) * index)

            output = root / "release"
            manifest = package_kevin_box2_release.package_release(
                build,
                output,
                "kevin-box2-local-v0.1.0-rc1",
                "source-sha",
                "upstream-sha",
                "6.0.2",
                "hardware-validation-pending",
            )

            self.assertEqual(manifest["validation_status"], "hardware-validation-pending")
            self.assertEqual(len(manifest["artifacts"]), 6)
            self.assertEqual(manifest["artifacts"][0]["flash_offset"], "0x20000")
            checksums = (output / "SHA256SUMS").read_text(encoding="utf-8")
            self.assertIn("  xiaozhi.bin", checksums)
            self.assertIn("  manifest.json", checksums)

    def test_refuses_to_mix_with_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            output = root / "release"
            output.mkdir()
            (output / "old.bin").write_bytes(b"old")
            with self.assertRaisesRegex(ValueError, "not empty"):
                package_kevin_box2_release.package_release(
                    root / "build",
                    output,
                    "release",
                    "source",
                    "upstream",
                    "6.0.2",
                    "hardware-validation-pending",
                )


if __name__ == "__main__":
    unittest.main()
