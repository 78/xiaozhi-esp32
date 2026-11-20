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
        for base, alternate, prefix in [('zh-CN', 'en-US', 'petoi_en_'),
                                        ('en-US', 'zh-CN', 'petoi_zh_')]:
            with self.subTest(base=base), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                count, sounds = generator.generate(assets, output, base)
                self.assertGreater(count, 40)
                self.assertEqual(sounds, 16)
                generated = output / 'petoi_locale_assets.h'
                header = generated.read_text(encoding='utf-8')
                strings = [json.loads((assets / 'locales' / lang / 'language.json').read_text(
                    encoding='utf-8'))['strings']['WAKE_WORD_HINT'] for lang in (base, alternate)]
                text_table = header.split('kTextPairs[] = {\n', 1)[1].split('\n};', 1)[0]
                pairs = [json.loads('[' + line.strip()[1:-2] + ']')
                         for line in text_table.splitlines()]
                self.assertIn(strings, pairs)
                self.assertIn(f'_binary_{prefix}activation_ogg_start', header)
                for sound in (assets / 'locales' / alternate).glob('*.ogg'):
                    self.assertEqual((output / (prefix + sound.name)).read_bytes(), sound.read_bytes())
                generator.generate(assets, output, base)
                self.assertEqual(header, generated.read_text(encoding='utf-8'))
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
                generator.generate(root / 'assets', root / 'output', 'en-US')


if __name__ == '__main__':
    unittest.main()
