opus_sources = sources['OPUS_SOURCES']

opus_sources_float = sources['OPUS_SOURCES_FLOAT']

if not disable_float_api
  opus_sources += opus_sources_float
endif

opus_lib_c_args = []
if host_machine.system() == 'windows'
  opus_lib_c_args += ['-DDLL_EXPORT']
endif

opus_lib = library('opus',
  opus_sources,
  version: libversion,
  darwin_versions: macosversion,
  c_args: opus_lib_c_args,
  include_directories: opus_includes,
  link_whole: [celt_lib, silk_lib, dnn_lib],
  dependencies: libm,
  install: true)

opus_dep = declare_dependency(link_with: opus_lib,
  include_directories: opus_public_includes)

# Extra uninstalled Opus programs
if not extra_programs.disabled()
  foreach prog : ['opus_compare', 'opus_demo', 'repacketizer_demo']
    executable(prog, '@0@.c'.format(prog),
               include_directories: opus_includes,
               link_with: opus_lib,
               dependencies: libm,
               install: false)
  endforeach

  if opt_custom_modes
    executable('opus_custom_demo', '../celt/opus_custom_demo.c',
               include_directories: opus_includes,
               link_with: opus_lib,
               dependencies: libm,
               install: false)
  endif

endif
