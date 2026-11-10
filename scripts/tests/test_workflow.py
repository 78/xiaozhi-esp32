import importlib.util
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


check_workflow = load_module("check_workflow", "check_workflow.py")
sync_skills = load_module("sync_skills", "sync_skills.py")


class SecretScanTests(unittest.TestCase):
    def test_detects_telegram_bot_token(self):
        token = "123456789:" + "A" * 35
        findings = check_workflow.find_secrets(f"send to {token} now")
        self.assertEqual([kind for kind, _ in findings], ["Telegram bot token"])

    def test_detects_private_key_block(self):
        text = "-----BEGIN " + "PRIVATE KEY-----"
        findings = check_workflow.find_secrets(text)
        self.assertEqual([kind for kind, _ in findings], ["private key block"])

    def test_ignores_documentation_placeholders(self):
        text = "\n".join(
            [
                "LILA_TAVILY_API_KEY=",
                "tvly-...",
                "tvly-abc",
                "bot<token>",
                "ghp_<token>",
                "sk-...",
            ]
        )
        self.assertEqual(check_workflow.find_secrets(text), [])

    def test_scan_reports_the_file(self):
        token = "123456789:" + "B" * 35
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "app.txt").write_text(f"token={token}\n", encoding="utf-8")
            problems = check_workflow.scan_secrets(root)
        self.assertEqual(len(problems), 1)
        self.assertIn("app.txt", problems[0])
        self.assertIn("Telegram bot token", problems[0])

    def test_scan_skips_binary_files(self):
        token = "123456789:" + "C" * 35
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "firmware.bin").write_bytes(b"\0" + token.encode("ascii"))
            self.assertEqual(check_workflow.scan_secrets(root), [])


class EnvPolicyTests(unittest.TestCase):
    def test_repository_env_example_is_complete(self):
        self.assertEqual(check_workflow.check_env_example(ROOT), [])

    def test_missing_key_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".env.example").write_text(
                "LILA_BOARD=wk-esp32s3-dev\n", encoding="utf-8"
            )
            problems = check_workflow.check_env_example(root)
        self.assertTrue(any("LILA_WAKE_WORD" in problem for problem in problems))

    def test_secret_keys_must_stay_empty(self):
        keys = "\n".join(
            f"{key}=" for key in check_workflow.REQUIRED_ENV_KEYS
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / ".env.example").write_text(keys + "\n", encoding="utf-8")
            self.assertEqual(check_workflow.check_env_example(root), [])

            (root / ".env.example").write_text(
                keys.replace(
                    "LILA_TAVILY_API_KEY=", "LILA_TAVILY_API_KEY=not-empty"
                ),
                encoding="utf-8",
            )
            problems = check_workflow.check_env_example(root)
        self.assertTrue(any("LILA_TAVILY_API_KEY" in problem for problem in problems))

    def test_gitignore_covers_local_state(self):
        self.assertEqual(check_workflow.check_gitignore(ROOT), [])


class RepositoryPolicyTests(unittest.TestCase):
    def test_required_files_exist(self):
        self.assertEqual(check_workflow.check_required_files(ROOT), [])

    def test_markers_are_current(self):
        self.assertEqual(check_workflow.check_markers(ROOT), [])

    def test_documentation_links_resolve(self):
        self.assertEqual(check_workflow.check_links(ROOT), [])

    def test_missing_link_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "process").mkdir()
            (root / "process" / "README.md").write_text(
                "[missing](WORKFLOW.md)\n", encoding="utf-8"
            )
            problems = check_workflow.check_links(root)
        self.assertEqual(len(problems), 1)
        self.assertIn("WORKFLOW.md", problems[0])


class PinnedFormatterTests(unittest.TestCase):
    def test_ci_and_standard_pin_the_same_version(self):
        self.assertEqual(check_workflow.check_pinned_formatter(ROOT), [])

    def test_mismatched_pin_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "scripts").mkdir()
            (root / ".github" / "workflows").mkdir(parents=True)
            (root / "process" / "standards").mkdir(parents=True)
            (root / "scripts" / "check_format.py").write_text(
                'PINNED_VERSION = "23.1.5"\n', encoding="utf-8"
            )
            (root / ".github" / "workflows" / "ci.yml").write_text(
                "clang-format==18.1.8\n", encoding="utf-8"
            )
            (root / "process" / "standards" / "firmware-cpp.md").write_text(
                "clang-format 23.1.5\n", encoding="utf-8"
            )
            problems = check_workflow.check_pinned_formatter(root)
        self.assertEqual(len(problems), 1)
        self.assertIn("23.1.5", problems[0])


class SkillsMirrorTests(unittest.TestCase):
    def test_repository_mirror_is_in_sync(self):
        problems = sync_skills.compare_trees(
            ROOT / ".opencode" / "skills", ROOT / ".claude" / "skills"
        )
        self.assertEqual(problems, [])

    def test_missing_mirror_file_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            mirror = root / "mirror"
            (source / "demo").mkdir(parents=True)
            (source / "demo" / "SKILL.md").write_text("hello\n", encoding="utf-8")
            problems = sync_skills.compare_trees(source, mirror)
        self.assertTrue(any("missing in mirror" in item for item in problems))

    def test_write_mirror_copies_and_prunes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            mirror = root / "mirror"
            (source / "demo").mkdir(parents=True)
            (source / "demo" / "SKILL.md").write_text("hello\n", encoding="utf-8")
            (mirror / "old").mkdir(parents=True)
            (mirror / "old" / "SKILL.md").write_text("stale\n", encoding="utf-8")

            actions = sync_skills.write_mirror(source, mirror)

            self.assertEqual(
                sync_skills.compare_trees(source, mirror),
                [],
            )
            self.assertFalse((mirror / "old").exists())
            self.assertTrue(any("removed old/SKILL.md" in item for item in actions))

    def test_crlf_checkout_matches_lf_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            mirror = root / "mirror"
            (source / "demo").mkdir(parents=True)
            (mirror / "demo").mkdir(parents=True)
            (source / "demo" / "SKILL.md").write_bytes(b"hello\n")
            (mirror / "demo" / "SKILL.md").write_bytes(b"hello\r\n")
            self.assertEqual(sync_skills.compare_trees(source, mirror), [])

    def test_empty_source_is_an_error_for_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(SystemExit):
                sync_skills.write_mirror(root / "missing", root / "mirror")

    def test_content_drift_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            mirror = root / "mirror"
            for base, text in ((source, "new\n"), (mirror, "old\n")):
                (base / "demo").mkdir(parents=True)
                (base / "demo" / "SKILL.md").write_text(text, encoding="utf-8")
            problems = sync_skills.compare_trees(source, mirror)
        self.assertEqual(problems, ["content differs: demo/SKILL.md"])


if __name__ == "__main__":
    unittest.main()
