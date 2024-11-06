dnn_sources = sources['DEEP_PLC_SOURCES']

dred_sources = sources['DRED_SOURCES']
if opt_dred.enabled()
  dnn_sources += dred_sources
endif

osce_sources = sources['OSCE_SOURCES']
if opt_osce.enabled()
  dnn_sources += osce_sources
endif

dnn_sources_sse2 = sources['DNN_SOURCES_SSE2']
dnn_sources_sse4_1 = sources['DNN_SOURCES_SSE4_1']
dnn_sources_avx2 = sources['DNN_SOURCES_AVX2']

dnn_sources_neon_intr = sources['DNN_SOURCES_NEON']
dnn_sources_dotprod_intr = sources['DNN_SOURCES_DOTPROD']

dnn_includes = [opus_includes]
dnn_static_libs = []

if host_cpu_family in ['x86', 'x86_64'] and opus_conf.has('OPUS_HAVE_RTCD')
  dnn_sources +=  sources['DNN_SOURCES_X86_RTCD']
endif

if host_cpu_family in ['arm', 'aarch64'] and have_arm_intrinsics_or_asm
  if opus_conf.has('OPUS_HAVE_RTCD')
    dnn_sources +=  sources['DNN_SOURCES_ARM_RTCD']
  endif
endif

foreach intr_name : ['sse2', 'sse4_1', 'avx2', 'neon_intr', 'dotprod_intr']
  have_intr = get_variable('have_' + intr_name)
  if not have_intr
    continue
  endif

  intr_sources = get_variable('dnn_sources_' + intr_name)

  intr_args = get_variable('opus_@0@_args'.format(intr_name), [])
  dnn_static_libs += static_library('dnn_' + intr_name, intr_sources,
      c_args: intr_args,
      include_directories: dnn_includes,
      install: false)
endforeach

dnn_c_args = []
if host_machine.system() == 'windows'
  dnn_c_args += ['-DDLL_EXPORT']
endif


if opt_deep_plc.enabled()
 dnn_lib = static_library('opus-dnn',
  dnn_sources,
  c_args: dnn_c_args,
  include_directories: dnn_includes,
  link_whole: [dnn_static_libs],
  dependencies: libm,
  install: false)
else
  dnn_lib = []
endif
