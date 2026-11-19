"""Generate board-private English text/sound mappings without editing lang_config.h."""
import argparse
import json
from pathlib import Path
import re
import shutil

FORMAT = re.compile(r'%(?:\d+\$)?[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp%]')


def generate(assets: Path, output: Path):
    english = json.loads((assets / 'locales/en-US/language.json').read_text(encoding='utf-8'))['strings']
    chinese = json.loads((assets / 'locales/zh-CN/language.json').read_text(encoding='utf-8'))['strings']
    output.mkdir(parents=True, exist_ok=True)
    lines = ['// Generated board-private locale resources. Do not edit.', '#pragma once',
             '#include <string_view>', '#include "assets/lang_config.h"',
             'namespace petoi::locale_assets {',
             'struct TextPair { const char* source; const char* english; };',
             'inline constexpr TextPair kTextPairs[] = {']
    for key, value in sorted(english.items()):
        source = chinese.get(key, value)
        if FORMAT.findall(source) != FORMAT.findall(value):
            raise ValueError('Incompatible translation format: ' + key)
        lines.append('    {' + json.dumps(source, ensure_ascii=False) + ', ' + json.dumps(value) + '},')
    lines += ['};', 'struct SoundPair { std::string_view source; std::string_view english; };']
    pairs, copies = [], []
    for sound in sorted((assets / 'locales/en-US').glob('*.ogg')):
        name = sound.stem
        if not re.fullmatch(r'[a-z0-9_]+', name):
            raise ValueError('Unsupported sound name: ' + name)
        target = output / ('petoi_en_' + sound.name)
        if not target.exists() or target.read_bytes() != sound.read_bytes():
            shutil.copyfile(sound, target)
        copies.append(target.resolve().as_posix())
        symbol = 'petoi_en_' + name
        lines += [f'extern const char {symbol}_start[] asm("_binary_{symbol}_ogg_start");',
                  f'extern const char {symbol}_end[] asm("_binary_{symbol}_ogg_end");']
        pairs.append(f'    {{Lang::Sounds::OGG_{name.upper()}, '
                     f'{{{symbol}_start, static_cast<size_t>({symbol}_end - {symbol}_start)}}}},')
    lines += ['inline const SoundPair kSoundPairs[] = {', *pairs, '};', '}']
    header = output / 'petoi_locale_assets.h'
    content = '\n'.join(lines) + '\n'
    if not header.exists() or header.read_text(encoding='utf-8') != content:
        header.write_text(content, encoding='utf-8')
    (output / 'embed_files.cmake').write_text(
        'set(PETOI_LOCALE_SOUNDS\n' + ''.join(f'    "{path}"\n' for path in copies) + ')\n', encoding='utf-8')
    return len(english), len(copies)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--assets-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    print('Petoi locale resources:', generate(args.assets_dir, args.output_dir))
