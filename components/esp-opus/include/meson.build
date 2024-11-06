opus_headers = [
  'opus.h',
  'opus_multistream.h',
  'opus_projection.h',
  'opus_types.h',
  'opus_defines.h',
]

if opt_custom_modes
  opus_headers += ['opus_custom.h']
endif

install_headers(opus_headers, subdir: 'opus')
