#!/usr/bin/env python3
#
# opus/read-sources-list.py
#
# Parses .mk files and extracts list of source files.
# Prints one line per source file list, with filenames space-separated.

import sys

if len(sys.argv) < 2:
  sys.exit('Usage: {} sources_foo.mk [sources_bar.mk...]'.format(sys.argv[0]))

for input_fn in sys.argv[1:]:
  with open(input_fn, 'r', encoding='utf8') as f:
    text = f.read()
    text = text.replace('\\\n', '')

    # Remove empty lines
    lines = [line for line in text.split('\n') if line.strip()]

    # Print SOURCES_XYZ = file1.c file2.c
    for line in lines:
      values = line.strip().split('=', maxsplit=2)
      if len(values) != 2:
        raise RuntimeError('Unable to parse line "{}" from file "{}"'.format(line, input_fn))
      var, files = values
      sources_list = [f for f in files.split(' ') if f]
      print(var.strip(), '=', ' '.join(sources_list))
