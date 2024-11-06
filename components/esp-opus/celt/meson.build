celt_sources = sources['CELT_SOURCES']

celt_sse_sources = sources['CELT_SOURCES_SSE']

celt_sse2_sources = sources['CELT_SOURCES_SSE2']

celt_sse4_1_sources = sources['CELT_SOURCES_SSE4_1']

celt_avx2_sources = sources['CELT_SOURCES_AVX2']

celt_neon_intr_sources = sources['CELT_SOURCES_ARM_NEON_INTR']

celt_static_libs = []

if host_cpu_family in ['x86', 'x86_64'] and opus_conf.has('OPUS_HAVE_RTCD')
  celt_sources +=  sources['CELT_SOURCES_X86_RTCD']
endif

foreach intr_name : ['sse', 'sse2', 'sse4_1', 'avx2', 'neon_intr']
  have_intr = get_variable('have_' + intr_name)
  if not have_intr
    continue
  endif

  intr_sources = get_variable('celt_@0@_sources'.format(intr_name))
  intr_args = get_variable('opus_@0@_args'.format(intr_name), [])
  celt_static_libs += static_library('celt_' + intr_name, intr_sources,
      c_args: intr_args,
      include_directories: opus_includes,
      install: false)
endforeach

have_arm_intrinsics_or_asm = have_arm_ne10
if (intrinsics_support.length() + asm_optimization.length() + inline_optimization.length()) > 0
  have_arm_intrinsics_or_asm = true
endif

if host_cpu_family in ['arm', 'aarch64'] and have_arm_intrinsics_or_asm
  if opus_conf.has('OPUS_HAVE_RTCD')
    celt_sources +=  sources['CELT_SOURCES_ARM_RTCD']
  endif
  if have_arm_ne10
    celt_sources += sources['CELT_SOURCES_ARM_NE10']
  endif
  if opus_arm_external_asm
    subdir('arm')
    celt_static_libs += static_library('celt-armasm',
      celt_arm_armopts_s, celt_sources_arm_asm,
      install: false)
  endif
endif

celt_c_args = []
if host_system == 'windows'
  celt_c_args += ['-DDLL_EXPORT']
endif

celt_lib = static_library('opus-celt',
  celt_sources,
  c_args: celt_c_args,
  include_directories: opus_includes,
  link_whole: celt_static_libs,
  dependencies: libm,
  install: false)
