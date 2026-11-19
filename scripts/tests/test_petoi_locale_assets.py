"""Board-private locale assets must retain upstream inputs and unique sound symbols."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('petoi_locale_generator', ROOT / 'scripts/gen_petoi_locale.py')
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class PetoiLocaleAssetTests(unittest.TestCase):
    def test_real_translations_and_private_sound_copies(self):
        assets = ROOT / 'main/assets'
        original = (assets / 'lang_config.h').read_bytes() if (assets / 'lang_config.h').exists() else None
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            count, sounds = generator.generate(assets, output)
            self.assertGreater(count, 40)
            self.assertEqual(sounds, 16)
            header = (output / 'petoi_locale_assets.h').read_text(encoding='utf-8')
            self.assertIn('"Say \\"%s\\" to wake me."', header)
            self.assertIn('_binary_petoi_en_activation_ogg_start', header)
            for sound in (assets / 'locales/en-US').glob('*.ogg'):
                self.assertEqual((output / ('petoi_en_' + sound.name)).read_bytes(), sound.read_bytes())
            generator.generate(assets, output)
            self.assertEqual(header, (output / 'petoi_locale_assets.h').read_text(encoding='utf-8'))
        if original is not None:
            self.assertEqual(original, (assets / 'lang_config.h').read_bytes())

    def test_mismatched_format_rejected_before_compilation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for language, value in [('en-US', 'code %d'), ('zh-CN', 'code %s')]:
                folder = root / 'assets/locales' / language
                folder.mkdir(parents=True)
                (folder / 'language.json').write_text(json.dumps({'strings': {'CODE': value}}))
            with self.assertRaisesRegex(ValueError, 'Incompatible translation format'):
                generator.generate(root / 'assets', root / 'output')


if __name__ == '__main__':
    unittest.main()
