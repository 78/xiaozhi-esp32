silk_sources = sources['SILK_SOURCES']

silk_sources_sse4_1 = sources['SILK_SOURCES_SSE4_1']

silk_sources_avx2 = sources['SILK_SOURCES_AVX2']

silk_sources_neon_intr = sources['SILK_SOURCES_ARM_NEON_INTR']

silk_sources_fixed_neon_intr = sources['SILK_SOURCES_FIXED_ARM_NEON_INTR']

silk_sources_fixed = sources['SILK_SOURCES_FIXED']

silk_sources_fixed_sse4_1 = sources['SILK_SOURCES_FIXED_SSE4_1']

silk_sources_float_sse4_1 = []
silk_sources_float_neon_intr = []
silk_sources_float_avx2 = sources['SILK_SOURCES_FLOAT_AVX2']

silk_sources_float = sources['SILK_SOURCES_FLOAT']

if opt_fixed_point
  silk_sources += silk_sources_fixed
else
  silk_sources += silk_sources_float
endif

silk_includes = [opus_includes, include_directories('float', 'fixed')]
silk_static_libs = []

if host_cpu_family in ['x86', 'x86_64'] and opus_conf.has('OPUS_HAVE_RTCD')
  silk_sources +=  sources['SILK_SOURCES_X86_RTCD']
endif

if host_cpu_family in ['arm', 'aarch64'] and have_arm_intrinsics_or_asm
  if opus_conf.has('OPUS_HAVE_RTCD')
    silk_sources +=  sources['SILK_SOURCES_ARM_RTCD']
  endif
endif

foreach intr_name : ['sse4_1', 'avx2', 'neon_intr']
  have_intr = get_variable('have_' + intr_name)
  if not have_intr
    continue
  endif

  intr_sources = get_variable('silk_sources_' + intr_name)
  if not opt_fixed_point
    intr_sources += get_variable('silk_sources_float_' + intr_name)
  endif

  intr_args = get_variable('opus_@0@_args'.format(intr_name), [])
  silk_static_libs += static_library('silk_' + intr_name, intr_sources,
      c_args: intr_args,
      include_directories: silk_includes,
      install: false)
endforeach

silk_c_args = []
if host_machine.system() == 'windows'
  silk_c_args += ['-DDLL_EXPORT']
endif

silk_lib = static_library('opus-silk',
  silk_sources,
  c_args: silk_c_args,
  include_directories: silk_includes,
  link_whole: [silk_static_libs],
  dependencies: libm,
  install: false)
