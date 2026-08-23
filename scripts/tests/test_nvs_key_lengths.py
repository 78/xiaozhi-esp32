import pathlib
import re
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SETTINGS_KEY_CALL = re.compile(
    r"\.(?:Get|Set|Erase)(?:Bool|Int|String|Key)\(\s*\"([^\"]+)\""
)


class NvsKeyLengthTest(unittest.TestCase):
    def test_literal_settings_keys_fit_esp_idf_limit(self):
        invalid = []
        for source in (PROJECT_ROOT / "main").rglob("*"):
            if source.suffix not in {".cc", ".cpp", ".h", ".hpp"}:
                continue
            text = source.read_text(encoding="utf-8")
            for match in SETTINGS_KEY_CALL.finditer(text):
                key = match.group(1)
                if len(key.encode("utf-8")) > 15:
                    invalid.append(f"{source.relative_to(PROJECT_ROOT)}: {key}")
        self.assertEqual([], invalid, "NVS keys are limited to 15 bytes")


if __name__ == "__main__":
    unittest.main()
