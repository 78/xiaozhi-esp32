project('opus', 'c',
  version: run_command('meson/get-version.py', '--package-version', check: true).stdout().strip(),
  meson_version: '>=0.54.0',
  default_options: ['warning_level=2',
                    'c_std=gnu99',
                    'buildtype=debugoptimized'])

libversion = run_command('meson/get-version.py', '--libtool-version', check: true).stdout().strip()
macosversion = run_command('meson/get-version.py', '--darwin-version', check: true).stdout().strip()

cc = meson.get_compiler('c')
host_system = host_machine.system()
host_cpu_family = host_machine.cpu_family()
top_srcdir = meson.current_source_dir()
top_builddir = meson.current_build_dir()

opus_includes = include_directories('.', 'include', 'celt', 'silk', 'dnn')
opus_public_includes = include_directories('include')

add_project_arguments('-DOPUS_BUILD', language: 'c')
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

if host_system == 'windows'
  if cc.get_argument_syntax() == 'msvc'
    add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'c')
  endif
endif

if cc.get_argument_syntax() == 'gnu'
  add_project_arguments('-D_FORTIFY_SOURCE=2', language: 'c')
endif

# Check for extra compiler args
additional_c_args = []
if cc.get_argument_syntax() != 'msvc'
  additional_c_args += [
    '-fvisibility=hidden',
    '-Wcast-align',
    '-Wnested-externs',
    '-Wshadow',
    '-Wstrict-prototypes',
  ]

  # On Windows, -fstack-protector-strong adds a libssp-0.dll dependency and
  # prevents static linking
  if host_system != 'windows'
    additional_c_args += ['-fstack-protector-strong']
  endif
endif

foreach arg : additional_c_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language: 'c')
  endif
endforeach

# Windows MSVC warnings
if cc.get_id() == 'msvc'
  # Ignore several spurious warnings.
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  add_project_arguments('/wd4035', '/wd4715', '/wd4116', '/wd4046', '/wd4068',
    '/wd4820', '/wd4244', '/wd4255', '/wd4668',
    language : 'c')
endif

opus_version = meson.project_version()

opus_conf = configuration_data()
opus_conf.set('PACKAGE_BUGREPORT', '"opus@xiph.org"')
opus_conf.set('PACKAGE_NAME', '"opus"')
opus_conf.set('PACKAGE_STRING', '"opus @0@"'.format(opus_version))
opus_conf.set('PACKAGE_TARNAME', '"opus"')
opus_conf.set('PACKAGE_URL', '""')
opus_conf.set('PACKAGE_VERSION', '"@0@"'.format(opus_version))

# FIXME: optional Ne10 dependency
have_arm_ne10 = false

libm = cc.find_library('m', required : false)

opus_conf.set('HAVE_LRINTF', cc.has_function('lrintf', prefix: '#include <math.h>', dependencies: libm))
opus_conf.set('HAVE_LRINT', cc.has_function('lrint', prefix: '#include <math.h>', dependencies: libm))
opus_conf.set('HAVE___MALLOC_HOOK', cc.has_function('__malloc_hook', prefix: '#include <malloc.h>'))
opus_conf.set('HAVE_STDINT_H', cc.check_header('stdint.h'))

# Check for restrict keyword
restrict_tmpl = '''
typedef int * int_ptr;
int foo (int_ptr @0@ ip, int * @0@ baz[]) {
  return ip[0];
}
int main (int argc, char ** argv) {
  int s[1];
  int * @0@ t = s;
  t[0] = 0;
  return foo(t, (void *)0);
}'''
# Define restrict to the equivalent of the C99 restrict keyword, or to
# nothing if this is not supported.  Do not define if restrict is
# supported directly.
if not cc.compiles(restrict_tmpl.format('restrict'), name : 'restrict keyword')
  if cc.compiles(restrict_tmpl.format('__restrict'), name : '__restrict')
    opus_conf.set('restrict', '__restrict')
  elif cc.compiles(restrict_tmpl.format('__restrict__'), name : '__restrict__')
    opus_conf.set('restrict', '__restrict')
  elif cc.compiles(restrict_tmpl.format('_Restrict'), name : '_Restrict')
    opus_conf.set('restrict', '_Restrict')
  else
    opus_conf.set('restrict', '/**/')
  endif
endif

# Check for C99 variable-size arrays, or alloca() as fallback
msg_use_alloca = false
if cc.compiles('''static int x;
                  char some_func (void) {
                    char a[++x];
                    a[sizeof a - 1] = 0;
                    int N;
                    return a[0];
                  }''', name : 'C99 variable-size arrays')
  opus_conf.set('VAR_ARRAYS', 1)
  msg_use_alloca = 'NO (using C99 variable-size arrays instead)'
elif cc.compiles('''#include <alloca.h>
                    void some_func (void) {
                      int foo=10;
                      int * array = alloca(foo);
                    }''', name : 'alloca (alloca.h)')
  opus_conf.set('USE_ALLOCA', true)
  opus_conf.set('HAVE_ALLOCA_H', true)
  msg_use_alloca = true
elif cc.compiles('''#include <malloc.h>
                    #include <stdlib.h>
                    void some_func (void) {
                      int foo=10;
                      int * array = alloca(foo);
                    }''', name : 'alloca (std)')
  opus_conf.set('USE_ALLOCA', true)
  msg_use_alloca = true
endif

opts = [
  [ 'fixed-point', 'FIXED_POINT' ],
  [ 'fixed-point-debug', 'FIXED_DEBUG' ],
  [ 'custom-modes', 'CUSTOM_MODES' ],
  [ 'float-approx', 'FLOAT_APPROX' ],
  [ 'assertions', 'ENABLE_ASSERTIONS' ],
  [ 'hardening', 'ENABLE_HARDENING' ],
  [ 'fuzzing', 'FUZZING' ],
  [ 'check-asm', 'OPUS_CHECK_ASM' ],
]

foreach opt : opts
  # we assume these are all boolean options
  opt_foo = get_option(opt[0])
  if opt_foo
    opus_conf.set(opt[1], 1)
  endif
  set_variable('opt_' + opt[0].underscorify(), opt_foo)
endforeach

feat = [
  [ 'deep-plc', 'ENABLE_DEEP_PLC' ],
  [ 'dred', 'ENABLE_DRED' ],
  [ 'osce', 'ENABLE_OSCE' ],
]

foreach opt : feat
  # we assume these are all boolean options
  opt_foo = get_option(opt[0])
  if opt_foo.enabled()
    opus_conf.set(opt[1], 1)
  endif
  set_variable('opt_' + opt[0].underscorify(), opt_foo)
endforeach

opt_asm = get_option('asm')
opt_rtcd = get_option('rtcd')
opt_intrinsics = get_option('intrinsics')
extra_programs = get_option('extra-programs')
opt_tests = get_option('tests')

disable_float_api = not get_option('float-api')
if disable_float_api
  opus_conf.set('DISABLE_FLOAT_API', 1)
endif

if not get_option('dnn-debug-float').enabled()
  opus_conf.set('DISABLE_DEBUG_FLOAT', 1)
endif

# This is for the description in the pkg-config .pc file
if opt_fixed_point
  pc_build = 'fixed-point'
else
  pc_build = 'floating-point'
endif
if opt_custom_modes
  pc_build = pc_build + ', custom modes'
endif

rtcd_support = []
# With GCC, Clang, ICC, etc, we differentiate between 'may support this SIMD'
# and 'presume we have this SIMD' by checking whether the SIMD / intrinsics can
# be compiled by the compiler as-is (presume) or with SIMD cflags (may have).
# With MSVC, the compiler will always build SIMD/intrinsics targeting all
# specific instruction sets supported by that version of the compiler. No
# special arguments are ever needed. If runtime CPU detection is not disabled,
# we must always assume that we only 'may have' it.
opus_can_presume_simd = true
if cc.get_argument_syntax() == 'msvc'
  if opt_rtcd.disabled()
    warning('Building with an MSVC-like compiler and runtime CPU detection is disabled. Outputs may not run on all @0@ CPUs.'.format(host_cpu_family))
  else
    opus_can_presume_simd = false
  endif
endif

opus_arm_external_asm = false

asm_tmpl = '''
int main (int argc, char ** argv) {
  __asm__("@0@");
  return 0;
}'''

asm_optimization = []
inline_optimization = []
if not opt_asm.disabled()
  # Currently we only have inline asm for fixed-point
  if host_cpu_family == 'arm' and opt_fixed_point
    opus_conf.set('OPUS_ARM_ASM', true)

    # Check if compiler supports gcc-style inline assembly
    if cc.compiles('''#ifdef __GNUC_MINOR__
                      #if (__GNUC__ * 1000 + __GNUC_MINOR__) < 3004
                      #error GCC before 3.4 has critical bugs compiling inline assembly
                      #endif
                      #endif
                      int main(int argc, char ** argv) { __asm__ (""::); }''',
                   name : 'compiler supports gcc-style inline assembly')

      opus_conf.set('OPUS_ARM_INLINE_ASM', 1)

      # AS_ASM_ARM_EDSP
      if cc.compiles(asm_tmpl.format('qadd r3,r3,r3'),
                     name : 'assembler supports EDSP instructions on ARM')
        opus_conf.set('OPUS_ARM_INLINE_EDSP', 1)
        inline_optimization += ['ESDP']
      endif

      # AS_ASM_ARM_MEDIA
      if cc.compiles(asm_tmpl.format('shadd8 r3,r3,r3'),
                     name : 'assembler supports ARMv6 media instructions on ARM')
        opus_conf.set('OPUS_ARM_INLINE_MEDIA', 1)
        inline_optimization += ['Media']
      endif

      # AS_ASM_ARM_NEON
      if cc.compiles(asm_tmpl.format('vorr d0,d0,d0'),
                     name : 'assembler supports NEON instructions on ARM')
        opus_conf.set('OPUS_ARM_INLINE_NEON', 1)
        inline_optimization += ['NEON']
      endif

      # AS_ASM_ARM_DOTPROD
      if cc.compiles(asm_tmpl.format('udot v0.4s,v1.16b,v2.16b'),
                     name : 'assembler supports DOTPROD instructions on ARM')
        opus_conf.set('OPUS_ARM_INLINE_DOTPROD', 1)
        inline_optimization += ['DOTPROD']
      endif
    endif

    # We need Perl to translate RVCT-syntax asm to gas syntax
    perl = find_program('perl', required: get_option('asm'))
    if perl.found()
      opus_arm_external_asm = true
      # opus_arm_presume_* mean we can and will use those instructions
      # directly without doing runtime CPU detection.
      # opus_arm_may_have_* mean we can emit those instructions, but we can
      # only use them after runtime detection.
      # The same rules apply for x86 assembly and intrinsics.

      opus_arm_may_have_edsp = opus_conf.has('OPUS_ARM_INLINE_EDSP')
      opus_arm_presume_edsp = opus_arm_may_have_edsp and opus_can_presume_simd

      opus_arm_may_have_media = opus_conf.has('OPUS_ARM_INLINE_MEDIA')
      opus_arm_presume_media = opus_arm_may_have_media and opus_can_presume_simd

      opus_arm_may_have_neon = opus_conf.has('OPUS_ARM_INLINE_NEON')
      opus_arm_presume_neon = opus_arm_may_have_neon and opus_can_presume_simd

      opus_arm_may_have_dotprod = opus_conf.has('OPUS_ARM_INLINE_DOTPROD')
      opus_arm_presume_dotprod = opus_arm_may_have_dotprod and opus_can_presume_simd

      if not opt_rtcd.disabled()
        if not opus_arm_may_have_edsp
          message('Trying to force-enable armv5e EDSP instructions...')
          # AS_ASM_ARM_EDSP_FORCE
          opus_arm_may_have_edsp = cc.compiles(asm_tmpl.format('.arch armv5te;.object_arch armv4t;qadd r3,r3,r3'),
                                               name : 'Assembler supports EDSP instructions on ARM (forced)')
        endif
        if not opus_arm_may_have_media
          message('Trying to force-enable ARMv6 media instructions...')
          opus_arm_may_have_media = cc.compiles(asm_tmpl.format('.arch armv6;.object_arch armv4t;shadd8 r3,r3,r3'),
                                                name : 'Assembler supports ARMv6 media instructions on ARM (forced)')
        endif
        if not opus_arm_may_have_neon
          message('Trying to force-enable NEON instructions...')
          opus_arm_may_have_neon = cc.compiles(asm_tmpl.format('.arch armv7-a;.fpu neon;.object_arch armv4t;vorr d0,d0,d0'),
                                               name : 'Assembler supports NEON instructions on ARM (forced)')
        endif
      endif

      if opus_arm_may_have_edsp
        opus_conf.set('OPUS_ARM_MAY_HAVE_EDSP', 1)
        if opus_arm_presume_edsp
          opus_conf.set('OPUS_ARM_PRESUME_EDSP', 1)
          asm_optimization += ['EDSP']
        else
          rtcd_support += ['EDSP']
        endif
      endif
      if opus_arm_may_have_media
        opus_conf.set('OPUS_ARM_MAY_HAVE_MEDIA', 1)
        if opus_arm_presume_media
          opus_conf.set('OPUS_ARM_PRESUME_MEDIA', 1)
          asm_optimization += ['Media']
        else
          rtcd_support += ['Media']
        endif
      endif
      if opus_arm_may_have_neon
        opus_conf.set('OPUS_ARM_MAY_HAVE_NEON', 1)
        if opus_arm_presume_neon
          opus_conf.set('OPUS_ARM_PRESUME_NEON', 1)
          asm_optimization += ['NEON']
        else
          rtcd_support += ['NEON']
        endif
      endif
      if opus_arm_may_have_dotprod
        opus_conf.set('OPUS_ARM_MAY_HAVE_DOTPROD', 1)
        if opus_arm_presume_dotprod
          opus_conf.set('OPUS_ARM_PRESUME_DOTPROD', 1)
          asm_optimization += ['DOTPROD']
        else
          rtcd_support += ['DOTPROD']
        endif
      endif

      if cc.get_define('__APPLE__') != ''
        arm2gnu_args = ['--apple']
      else
        arm2gnu_args = []
      endif
    endif # found perl
  else # arm + enable fixed point
    if opt_asm.enabled()
      error('asm option is enabled, but no assembly support for ' + host_cpu_family)
    endif
  endif
endif # enable asm

# Check whether we require assembly and we support assembly on this arch,
# but none were detected. Can happen because of incorrect compiler flags, such
# as missing -mfloat-abi=softfp on ARM32 softfp architectures.
if opt_asm.enabled() and (asm_optimization.length() + inline_optimization.length()) == 0
  error('asm option was enabled, but no assembly support was detected')
endif

# XXX: NEON has hardfp vs softfp compiler configuration issues
# When targeting ARM32 softfp, we sometimes need to explicitly pass
# -mfloat-abi=softfp to enable NEON. F.ex., on Android. It should
# be set in the cross file.
arm_neon_intr_link_args = ['-mfpu=neon']
arm_dotprod_intr_link_args = ['-march=armv8.2-a+dotprod']

have_sse = false
have_sse2 = false
have_sse4_1 = false
have_avx2 = false
have_neon_intr = false
have_dotprod_intr = false

intrinsics_support = []
if not opt_intrinsics.disabled()
  if host_cpu_family in ['arm', 'aarch64']
    # Check for ARMv7/AArch64 neon intrinsics
    intrin_check = '''
    #include <arm_neon.h>
      int main (void) {
        static float32x4_t A0, A1, SUMM;
        SUMM = vmlaq_f32(SUMM, A0, A1);
        return (int)vgetq_lane_f32(SUMM, 0);
      }'''
    intrin_name = 'ARMv7/AArch64 NEON'
    if cc.links(intrin_check,
                name: 'compiler supports @0@ intrinsics'.format(intrin_name))
      opus_arm_presume_neon_intr = opus_can_presume_simd
      opus_arm_may_have_neon_intr = true
    else
      opus_arm_presume_neon_intr = false
      if cc.links(intrin_check,
                  args: arm_neon_intr_link_args,
                  name: 'compiler supports @0@ intrinsics with @1@'.format(intrin_name, ' '.join(arm_neon_intr_link_args)))
        opus_arm_may_have_neon_intr = true
      else
        opus_arm_may_have_neon_intr = false
      endif
    endif

    if opus_arm_may_have_neon_intr
      have_neon_intr = true
      intrinsics_support += [intrin_name]
      opus_conf.set('OPUS_ARM_MAY_HAVE_NEON_INTR', 1)
      if opus_arm_presume_neon_intr
        opus_conf.set('OPUS_ARM_PRESUME_NEON_INTR', 1)
      else
        rtcd_support += [intrin_name]
        opus_neon_intr_args = arm_neon_intr_link_args
      endif
    else
      message('Compiler does not support @0@ intrinsics'.format(intrin_name))
    endif

    # Check for aarch64 neon intrinsics
    intrin_check = '''
    #include <arm_neon.h>
      int main (void) {
        static int32_t IN;
        static int16_t OUT;
        OUT = vqmovns_s32(IN);
      }'''
    intrin_name = 'AArch64 NEON'
    if cc.links(intrin_check,
                name: 'compiler supports @0@ intrinsics'.format(intrin_name))
      opus_arm_presume_aarch64_neon_intr = opus_can_presume_simd
      opus_arm_may_have_aarch64_neon_intr = true
    else
      opus_arm_presume_aarch64_neon_intr = false
      if cc.links(intrin_check,
                  args: arm_neon_intr_link_args,
                  name: 'compiler supports @0@ intrinsics with @1@'.format(intrin_name, ' '.join(arm_neon_intr_link_args)))
        opus_arm_may_have_aarch64_neon_intr = true
      else
        opus_arm_may_have_aarch64_neon_intr = false
      endif
    endif

    if opus_arm_may_have_aarch64_neon_intr
      intrinsics_support += [intrin_name]
      opus_conf.set('OPUS_X86_MAY_HAVE_AARCH64_NEON_INTR', 1)
      if opus_arm_presume_aarch64_neon_intr
        opus_conf.set('OPUS_X86_PRESUME_AARCH64_NEON_INTR', 1)
      endif
    else
      message('Compiler does not support @0@ intrinsics'.format(intrin_name))
    endif

    # Check for ARMv8.2 dotprod intrinsics
    intrin_check = '''
    #include <arm_neon.h>
      int main (void) {
        static int8x16_t a, b;
        static int32x4_t SUMM;
        SUMM = vdotq_s32(SUMM, a, b);
        return (int)vgetq_lane_s32(SUMM, 0);
      }'''
    intrin_name = 'AArch64 DOTPROD'
    if cc.links(intrin_check,
                name: 'compiler supports @0@ intrinsics'.format(intrin_name))
      opus_arm_presume_dotprod_intr = opus_can_presume_simd
      opus_arm_may_have_dotprod_intr = true
    else
      opus_arm_presume_dotprod_intr = false
      if cc.links(intrin_check,
                  args: arm_dotprod_intr_link_args,
                  name: 'compiler supports @0@ intrinsics with @1@'.format(intrin_name, ' '.join(arm_dotprod_intr_link_args)))
        opus_arm_may_have_dotprod_intr = true
      else
        opus_arm_may_have_dotprod_intr = false
      endif
    endif

    if opus_arm_may_have_dotprod_intr
      have_dotprod_intr = true
      intrinsics_support += [intrin_name]
      opus_conf.set('OPUS_ARM_MAY_HAVE_DOTPROD', 1)
      if opus_arm_presume_dotprod_intr
        opus_conf.set('OPUS_ARM_PRESUME_DOTPROD', 1)
      else
        rtcd_support += [intrin_name]
        opus_dotprod_intr_args = arm_dotprod_intr_link_args
      endif
    else
      message('Compiler does not support @0@ intrinsics'.format(intrin_name))
    endif

  elif host_cpu_family in ['x86', 'x86_64']
    # XXX: allow external override/specification of the flags
    x86_intrinsics = [
      [ 'SSE', 'xmmintrin.h', '__m128', '_mm_setzero_ps()', ['-msse'], [] ],
      [ 'SSE2', 'emmintrin.h', '__m128i', '_mm_setzero_si128()', ['-msse2'], [] ],
      [ 'SSE4.1', 'smmintrin.h', '__m128i', '_mm_setzero_si128(); mtest = _mm_cmpeq_epi64(mtest, mtest)', ['-msse4.1'], [] ],
      [ 'AVX2', 'immintrin.h', '__m256i', '_mm256_abs_epi32(_mm256_setzero_si256())', ['-mavx', '-mfma', '-mavx2'], ['/arch:AVX2'] ],
    ]

    foreach intrin : x86_intrinsics
      intrin_check = '''#include <@0@>
                        int main (int argc, char ** argv) {
                          static @1@ mtest;
                          mtest = @2@;
                          return *((unsigned char *) &mtest) != 0;
                        }'''.format(intrin[1],intrin[2],intrin[3])
      intrin_name = intrin[0]
      intrin_args = cc.get_argument_syntax() == 'msvc' ? intrin[5] : intrin[4]
      if cc.get_argument_syntax() == 'msvc' and intrin_args.length() == 0 and cc.links(intrin_check, name : 'compiler supports @0@ intrinsics'.format(intrin_name))
        may_have_intrin = true
        presume_intrin = opus_can_presume_simd
      elif cc.get_argument_syntax() != 'msvc' and cc.links(intrin_check, name : 'compiler supports @0@ intrinsics'.format(intrin_name))
        may_have_intrin = true
        presume_intrin = opus_can_presume_simd
      elif intrin_args.length() > 0
        presume_intrin = false
        if cc.links(intrin_check,
                    args : intrin_args,
                    name : 'compiler supports @0@ intrinsics with @1@'.format(intrin_name, ' '.join(intrin_args)))
          may_have_intrin = true
        else
          may_have_intrin = false
        endif
      endif
      if may_have_intrin
        intrinsics_support += [intrin_name]
        intrin_lower_name = intrin_name.to_lower().underscorify()
        set_variable('have_' + intrin_lower_name, true)
        opus_conf.set('OPUS_X86_MAY_HAVE_' + intrin_name.underscorify(), 1)
        if presume_intrin
          opus_conf.set('OPUS_X86_PRESUME_' + intrin_name.underscorify(), 1)
        else
          rtcd_support += [intrin_name]
          set_variable('opus_@0@_args'.format(intrin_lower_name), intrin_args)
        endif
      else
        message('Compiler does not support @0@ intrinsics'.format(intrin_name))
      endif
    endforeach

    if not opt_rtcd.disabled()
      get_cpuid_by_asm = false
      cpuid_asm_code = '''
        #include <stdio.h>
        int main (int argc, char ** argv) {
                 unsigned int CPUInfo0;
                 unsigned int CPUInfo1;
                 unsigned int CPUInfo2;
                 unsigned int CPUInfo3;
                 unsigned int InfoType;
                #if defined(__i386__) && defined(__PIC__)
                 __asm__ __volatile__ (
                 "xchg %%ebx, %1\n"
                 "cpuid\n"
                 "xchg %%ebx, %1\n":
                 "=a" (CPUInfo0),
                 "=r" (CPUInfo1),
                 "=c" (CPUInfo2),
                 "=d" (CPUInfo3) :
                 "a" (InfoType), "c" (0)
                );
               #else
                 __asm__ __volatile__ (
                 "cpuid":
                 "=a" (CPUInfo0),
                 "=b" (CPUInfo1),
                 "=c" (CPUInfo2),
                 "=d" (CPUInfo3) :
                 "a" (InfoType), "c" (0)
                );
               #endif
          return 0;
        }'''
      cpuid_c_code = '''
        #include <cpuid.h>
        int main (int argc, char ** argv) {
          unsigned int CPUInfo0;
          unsigned int CPUInfo1;
          unsigned int CPUInfo2;
          unsigned int CPUInfo3;
          unsigned int InfoType;
          __get_cpuid(InfoType, &CPUInfo0, &CPUInfo1, &CPUInfo2, &CPUInfo3);
          return 0;
        }'''
      cpuid_msvc_code = '''
        #include <intrin.h>
        int main (void) {
          int CPUInfo, InfoType;
          __cpuid(&CPUInfo, InfoType);
        }'''
      if cc.links(cpuid_asm_code, name : 'Get X86 CPU info via inline assembly')
        opus_conf.set('CPU_INFO_BY_ASM', 1)
      elif cc.links(cpuid_c_code, name : 'Get X86 CPU info via C method')
        opus_conf.set('CPU_INFO_BY_C', 1)
      elif cc.get_define('_MSC_VER') != '' and cc.links(cpuid_msvc_code)
        message('Getting X86 CPU info via __cpuid')
      else
        if opt_intrinsics.enabled() and opt_rtcd.enabled()
          error('intrinsics and rtcd options are enabled, but no Get CPU Info method detected')
        endif
        warning('Get CPU Info method not detected, no rtcd for intrinsics')
      endif
    endif # opt_rtcd
  else
    if opt_intrinsics.enabled()
      error('intrinsics option enabled, but no intrinsics support for ' + host_cpu_family)
    endif
    warning('No intrinsics support for ' + host_cpu_family)
  endif
endif

# Check whether we require intrinsics and we support intrinsics on this arch,
# but none were detected. Can happen because of incorrect compiler flags, such
# as missing -mfloat-abi=softfp on ARM32 softfp architectures.
if opt_intrinsics.enabled() and intrinsics_support.length() == 0
  error('intrinsics option was enabled, but none were detected')
endif

if opt_rtcd.disabled()
  rtcd_support = 'disabled'
else
  if rtcd_support.length() > 0
    opus_conf.set('OPUS_HAVE_RTCD', 1)
  else
    if intrinsics_support.length() == 0
      rtcd_support = 'none'
      if opt_rtcd.enabled()
        error('rtcd option is enabled, but no support for intrinsics or assembly is available')
      endif
    else
      rtcd_support = 'not needed'
    endif
  endif
endif

# extract source file lists from .mk files
mk_files = [
  'opus_headers.mk', 'opus_sources.mk',
  'silk_headers.mk', 'silk_sources.mk',
  'celt_sources.mk', 'celt_headers.mk',
  'lpcnet_headers.mk', 'lpcnet_sources.mk',
]
lines = run_command('meson/read-sources-list.py', mk_files, check: true).stdout().strip().split('\n')
sources = {}
foreach l : lines
  a = l.split(' = ')
  var_name = a[0]
  file_list = a[1].split()
  sources += {var_name: files(file_list)}
endforeach

subdir('include')
subdir('celt')
subdir('silk')
subdir('dnn')
subdir('src')

configure_file(output: 'config.h', configuration: opus_conf)

if not opt_tests.disabled()
  subdir('celt/tests')
  subdir('silk/tests')
  subdir('tests')
endif

pkg = import('pkgconfig')
pkg.generate(opus_lib,
  description: 'Opus IETF audio codec (floating-point build)',
  subdirs: 'opus',
)

doxygen = find_program('doxygen', required: get_option('docs'))
if doxygen.found()
  subdir('doc')
endif

summary(
  {
    'C99 var arrays': opus_conf.has('VAR_ARRAYS'),
    'C99 lrintf': opus_conf.has('HAVE_LRINTF'),
    'Use alloca': msg_use_alloca,
  },
  section: 'Compiler support',
  bool_yn: true,
  list_sep: ', ',
)

# Parse optimization status
foreach status : [['inline_optimization', opt_asm],
                  ['asm_optimization', opt_asm],
                  ['intrinsics_support', opt_intrinsics]]
  res = status[0]
  opt = status[1]
  resval = get_variable(res)
  if opt.disabled()
    set_variable(res, 'disabled')
  elif resval.length() == 0
    if host_cpu_family not in ['arm', 'aarch64', 'x86', 'x86_64']
      set_variable(res, 'No optimizations for your platform, please send patches')
    else
      set_variable(res, 'none')
    endif
  endif
endforeach

summary(
  {
    'Floating point support': not opt_fixed_point,
    'Fast float approximations': opt_float_approx,
    'Fixed point debugging': opt_fixed_point_debug,
    'Inline assembly optimizations': inline_optimization,
    'External assembly optimizations': asm_optimization,
    'Intrinsics optimizations': intrinsics_support,
    'Run-time CPU detection': rtcd_support,
  },
  section: 'Optimizations',
  bool_yn: true,
  list_sep: ', ',
)
summary(
  {
    'Custom modes': opt_custom_modes,
    'Assertions': opt_assertions,
    'Hardening': opt_hardening,
    'Fuzzing': opt_fuzzing,
    'Check ASM': opt_check_asm,
    'API documentation': doxygen.found(),
    'Extra programs': not extra_programs.disabled(),
    'Tests': not opt_tests.disabled(),
  },
  section: 'General configuration',
  bool_yn: true,
  list_sep: ', ',
)
