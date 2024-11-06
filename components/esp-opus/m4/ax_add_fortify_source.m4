# ===========================================================================
#   Modified from https://www.gnu.org/software/autoconf-archive/ax_add_fortify_source.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_ADD_FORTIFY_SOURCE
#
# DESCRIPTION
#
#   Check whether -D_FORTIFY_SOURCE=2 can be added to CFLAGS without macro
#   redefinition warnings. Some distributions (such as Gentoo Linux) enable
#   _FORTIFY_SOURCE globally in their compilers, leading to unnecessary
#   warnings in the form of
#
#     <command-line>:0:0: error: "_FORTIFY_SOURCE" redefined [-Werror]
#     <built-in>: note: this is the location of the previous definition
#
#   which is a problem if -Werror is enabled. This macro checks whether
#   _FORTIFY_SOURCE is already defined, and if not, adds -D_FORTIFY_SOURCE=2
#   to CFLAGS.
#
# LICENSE
#
#   Copyright (c) 2017 David Seifert <soap@gentoo.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 1

AC_DEFUN([AX_ADD_FORTIFY_SOURCE],[
    AC_MSG_CHECKING([whether to add -D_FORTIFY_SOURCE=2 to CFLAGS])
    AC_LINK_IFELSE([
        AC_LANG_SOURCE(
            [[
                int main() {
                #ifndef _FORTIFY_SOURCE
                    return 0;
                #else
                    this_is_an_error;
                #endif
                }
            ]]
        )], [
            AC_MSG_RESULT([yes])
            CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
        ], [
            AC_MSG_RESULT([no])
    ])
])
