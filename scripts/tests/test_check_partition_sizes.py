import pathlib
import struct
import tempfile
import unittest

from scripts import check_partition_sizes


def partition_entry(label: str, offset: int, size: int) -> bytes:
    return struct.pack(
        "<HBBLL16sL",
        check_partition_sizes.PARTITION_MAGIC,
        0,
        0,
        offset,
        size,
        label.encode("ascii").ljust(16, b"\0"),
        0,
    )


class PartitionSizeTest(unittest.TestCase):
    def test_reads_table_and_reports_remaining_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            table = root / "partition-table.bin"
            table.write_bytes(
                partition_entry("ota_0", 0x20000, 0x1000)
                + partition_entry("assets", 0x800000, 0x2000)
                + b"\xff" * 32
            )
            app = root / "xiaozhi.bin"
            assets = root / "generated_assets.bin"
            app.write_bytes(b"a" * 0x600)
            assets.write_bytes(b"b" * 0x1800)

            partitions = check_partition_sizes.read_partition_table(table)
            reports = check_partition_sizes.validate_artifacts(
                partitions, [("ota_0", app), ("assets", assets)]
            )

            self.assertEqual(partitions["assets"].offset, 0x800000)
            self.assertIn("free=0xa00", reports[0])
            self.assertIn("free=0x800", reports[1])

    def test_rejects_artifact_larger_than_partition(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            artifact = root / "too-large.bin"
            artifact.write_bytes(b"x" * 17)
            partitions = {
                "ota_0": check_partition_sizes.Partition("ota_0", 0x20000, 16)
            }

            with self.assertRaisesRegex(ValueError, "exceeding ota_0"):
                check_partition_sizes.validate_artifacts(
                    partitions, [("ota_0", artifact)]
                )


if __name__ == "__main__":
    unittest.main()
