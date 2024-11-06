dnl as-gcc-inline-assembly.m4 0.1.0

dnl autostars m4 macro for detection of gcc inline assembly

dnl David Schleef <ds@schleef.org>

dnl AS_COMPILER_FLAG(ACTION-IF-ACCEPTED, [ACTION-IF-NOT-ACCEPTED])
dnl Tries to compile with the given CFLAGS.
dnl Runs ACTION-IF-ACCEPTED if the compiler can compile with the flags,
dnl and ACTION-IF-NOT-ACCEPTED otherwise.

AC_DEFUN([AS_GCC_INLINE_ASSEMBLY],
[
  AC_MSG_CHECKING([if compiler supports gcc-style inline assembly])

  AC_TRY_COMPILE([], [
#ifdef __GNUC_MINOR__
#if (__GNUC__ * 1000 + __GNUC_MINOR__) < 3004
#error GCC before 3.4 has critical bugs compiling inline assembly
#endif
#endif
__asm__ (""::) ], [flag_ok=yes], [flag_ok=no])

  if test "X$flag_ok" = Xyes ; then
    $1
    true
  else
    $2
    true
  fi
  AC_MSG_RESULT([$flag_ok])
])

AC_DEFUN([AS_ASM_ARM_NEON],
[
  AC_MSG_CHECKING([if assembler supports NEON instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__("vorr d0,d0,d0")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])

AC_DEFUN([AS_ASM_ARM_NEON_FORCE],
[
  AC_MSG_CHECKING([if assembler supports NEON instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__(".arch armv7-a\n.fpu neon\n.object_arch armv4t\nvorr d0,d0,d0")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])

AC_DEFUN([AS_ASM_ARM_MEDIA],
[
  AC_MSG_CHECKING([if assembler supports ARMv6 media instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__("shadd8 r3,r3,r3")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])

AC_DEFUN([AS_ASM_ARM_MEDIA_FORCE],
[
  AC_MSG_CHECKING([if assembler supports ARMv6 media instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__(".arch armv6\n.object_arch armv4t\nshadd8 r3,r3,r3")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])

AC_DEFUN([AS_ASM_ARM_EDSP],
[
  AC_MSG_CHECKING([if assembler supports EDSP instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__("qadd r3,r3,r3")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])

AC_DEFUN([AS_ASM_ARM_EDSP_FORCE],
[
  AC_MSG_CHECKING([if assembler supports EDSP instructions on ARM])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[__asm__(".arch armv5te\n.object_arch armv4t\nqadd r3,r3,r3")])],
                    [AC_MSG_RESULT([yes])
                     $1],
                    [AC_MSG_RESULT([no])
                     $2])
])
