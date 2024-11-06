dnl opus-intrinsics.m4
dnl macro for testing for support for compiler intrinsics, either by default or with a compiler flag

dnl OPUS_CHECK_INTRINSICS(NAME-OF-INTRINSICS, COMPILER-FLAG-FOR-INTRINSICS, VAR-IF-PRESENT, VAR-IF-DEFAULT, TEST-PROGRAM-HEADER, TEST-PROGRAM-BODY)
AC_DEFUN([OPUS_CHECK_INTRINSICS],
[
   AC_MSG_CHECKING([if compiler supports $1 intrinsics])
   AC_LINK_IFELSE(
     [AC_LANG_PROGRAM($5, $6)],
     [
        $3=1
        $4=1
        AC_MSG_RESULT([yes])
      ],[
        $4=0
        AC_MSG_RESULT([no])
        AC_MSG_CHECKING([if compiler supports $1 intrinsics with $2])
        save_CFLAGS="$CFLAGS"; CFLAGS="$CFLAGS $2"
        AC_LINK_IFELSE([AC_LANG_PROGRAM($5, $6)],
        [
           AC_MSG_RESULT([yes])
           $3=1
        ],[
           AC_MSG_RESULT([no])
           $3=0
        ])
        CFLAGS="$save_CFLAGS"
     ])
])
