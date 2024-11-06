exe = executable('test_unit_LPC_inv_pred_gain',
  'test_unit_LPC_inv_pred_gain.c', '../LPC_inv_pred_gain.c',
  include_directories: opus_includes,
  link_with: [celt_lib, celt_static_libs, silk_lib, silk_static_libs],
  dependencies: libm,
  install: false)

test(test_name, exe)
