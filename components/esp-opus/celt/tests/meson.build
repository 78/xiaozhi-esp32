tests = [
  'test_unit_types',
  'test_unit_mathops',
  'test_unit_entropy',
  'test_unit_laplace',
  'test_unit_dft',
  'test_unit_mdct',
  'test_unit_rotation',
  'test_unit_cwrs32',
]

foreach test_name : tests
  exe = executable(test_name, '@0@.c'.format(test_name),
                   include_directories : opus_includes,
                   link_with : [celt_lib, celt_static_libs],
                   dependencies : libm,
                   install : false)
  test(test_name, exe)
endforeach
