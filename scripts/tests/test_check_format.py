import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / filename)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


check_format = load_module("check_format", "check_format.py")


def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        capture_output=True,
        check=True,
        text=True,
    )
    return result.stdout.strip()


class MergeSpansTests(unittest.TestCase):
    def test_disjoint_ranges_are_kept(self):
        self.assertEqual(
            check_format.merge_spans([(1, 2), (10, 12)]),
            [(1, 2), (10, 12)],
        )

    def test_overlapping_ranges_are_merged(self):
        self.assertEqual(
            check_format.merge_spans([(1, 5), (3, 8), (20, 20)]),
            [(1, 8), (20, 20)],
        )

    def test_adjacent_ranges_are_merged(self):
        self.assertEqual(
            check_format.merge_spans([(5, 7), (8, 9)]),
            [(5, 9)],
        )

    def test_unsorted_input_is_normalised(self):
        self.assertEqual(
            check_format.merge_spans([(20, 21), (1, 3)]),
            [(1, 3), (20, 21)],
        )


class ChangedSpansTests(unittest.TestCase):
    def test_added_and_modified_lines_are_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            git(root, "init", "-q")
            (root / "sample.cc").write_text(
                "\n".join(f"line {index}" for index in range(1, 11)) + "\n",
                encoding="utf-8",
            )
            (root / "notes.md").write_text("ignored\n", encoding="utf-8")
            git(root, "add", ".")
            git(root, "-c", "user.email=ci@example.com", "-c", "user.name=CI",
                "commit", "-q", "-m", "base")

            (root / "sample.cc").write_text(
                "\n".join(
                    [
                        "line 1",
                        "changed 2",
                        "changed 3",
                        "line 4",
                        "line 5",
                        "line 6",
                        "line 7",
                        "line 8",
                        "line 9",
                        "added 10",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            git(root, "add", ".")
            git(root, "-c", "user.email=ci@example.com", "-c", "user.name=CI",
                "commit", "-q", "-m", "change")

            spans = check_format.changed_spans(root, "HEAD~1", "HEAD")

        self.assertEqual(list(spans), ["sample.cc"])
        self.assertEqual(spans["sample.cc"], [(2, 3), (10, 10)])

    def test_repository_files_are_not_empty(self):
        spans = check_format.changed_spans(ROOT, "origin/main", "HEAD")
        self.assertIsInstance(spans, dict)


class BinaryResolutionTests(unittest.TestCase):
    def test_missing_binary_fails_closed(self):
        with self.assertRaises(SystemExit):
            check_format.resolve_binary("clang-format-does-not-exist")

    def test_missing_file_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            failure = check_format.check_file(
                root, "missing.cc", [(1, 1)], "clang-format-does-not-exist"
            )
        self.assertIsNotNone(failure)


if __name__ == "__main__":
    unittest.main()
