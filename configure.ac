dnl vim: set sw=2 ts=2 expandtab :
dnl
dnl Copyright (C) 2007-2019 Abel Cheung.
dnl All rights reserved.

AC_PREREQ([2.69])
AC_INIT([rifiuti2], [0.7.0],
        [https://github.com/abelcheung/rifiuti2/issues], [],
        [https://abelcheung.github.io/rifiuti2/])
AC_CONFIG_SRCDIR([src/rifiuti-vista.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_TESTDIR([test])
AM_INIT_AUTOMAKE([1.11])

AC_CANONICAL_HOST
dnl Platform check.
AS_CASE([$host],
        [*-*-msys* | *-*-mingw*], [is_mingw=y  ],
        [*-*-cygwin*           ], [is_cygwin=y ],
        [*-*-solaris*          ], [os_solaris=y])
AC_SUBST(is_mingw)
AC_SUBST(is_cygwin)
AM_CONDITIONAL([OS_WINDOWS], [test "x$is_mingw" != x])

dnl I18N
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19])
LOCALEDIR_PORTABLE="${PACKAGE}-l10n"
AC_SUBST([LOCALEDIR_PORTABLE])

dnl Compiler and devel toolchain checks.
AS_BOX([Compiler and toolchain])
AC_PROG_CC
AC_PROG_CC_STDC

if test "x$ac_cv_prog_cc_c99" = "xno"; then
	AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi

dnl Always use -Wall for gcc
AC_MSG_CHECKING(whether compiler understands -Wall)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   CFLAGS="$old_CFLAGS"])

dnl Checks for programs.
AC_PROG_LN_S

dnl
dnl For test suite
dnl
AS_BOX([Test suite programs])
AC_PATH_PROG([XMLLINT],[xmllint])
dnl CP932 is commonly available among glibc, libiconv,
dnl as well as solaris with extra iconv module installed
AC_CACHE_CHECK([for iconv that supports more Windows codepages], [ac_cv_path_ICONV],
               [AC_PATH_PROGS_FEATURE_CHECK([ICONV], [iconv],
                [$ac_path_ICONV -f UTF-8 -t CP932 < /dev/null > /dev/null 2>&1 \
                 && ac_cv_path_ICONV=$ac_path_ICONV ac_path_ICONV_found=:])])
AC_SUBST([ICONV],[$ac_cv_path_ICONV])

dnl
dnl Checks for libraries.
dnl
AS_BOX([Libraries])

dnl g_slist_free_full() requires 2.28.0, and
dnl g_win32_get_command_line() needs 2.40.0
AS_VAR_SET_IF([is_mingw],
	[glib_req="glib-2.0 >= 2.40.0"],
	[glib_req="glib-2.0 >= 2.28.0"])
PKG_CHECK_MODULES([GLIB], [$glib_req],,
	[AC_MSG_ERROR([*** GLib not found. Error is: $GLIB_PKG_ERRORS])])

dnl
dnl See if glib is linked to Solaris implementation of iconv
dnl FIXME this check goes berserk under MinGW but magically works
dnl after manual gcc argument reordering
dnl
dnl There exists solaris derivatives that can install external
dnl glib2 package that links with libiconv, and they would require
dnl appropriate RPATH settings. However this is exception rather than
dnl the norm, and I don't have such system readily available now.
dnl GLIB_RPATH=`pkg-config --libs-only-L glib-2.0 | sed -e 's#-L#-R#g'`
dnl GLIB_LIBS="$GLIB_LIBS $GLIB_RPATH"

AS_VAR_SET_IF([os_solaris],[dnl
save_CFLAGS="$CFLAGS"
save_LDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LDFLAGS="$LDFLAGS $GLIB_LIBS $GLIB_RPATH"
AC_LANG_ASSERT([C])
AC_MSG_CHECKING([iconv implementation used by glib2])
AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <glib.h>],
	[return (g_iconv_open("UTF-8","CP950") == (GIConv) -1);])],
	[AC_MSG_RESULT([native/libiconv])],
	[AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <glib.h>],
		[return (g_iconv_open("UTF-8","646") == (GIConv) -1);])],
		[AC_MSG_RESULT([solaris])],
		[AC_MSG_RESULT([unknown])
		 AC_MSG_ERROR([dnl
iconv() implementation used by glib2 is not recognized, which
would encumber functionality of rifiuti2. Please contact maintainer
if you think this is a bug.])])])
CFLAGS="$save_CFLAGS"
LDFLAGS="$save_LDFLAGS"
])dnl end AS_VAR_SET_IF os_solaris

AC_ARG_ENABLE(static,
              [AS_HELP_STRING([--enable-static],
                              [build static binaries [default=no]])],,
              [enable_static=no])

dnl Need to pull in libiconv manually for mingw static build
dnl And this undocumented GLIB_STATIC_COMPILATION thing... dang
AS_VAR_IF([enable_static], [yes], [dnl
	AS_VAR_SET_IF([is_mingw],
		[pcfiles="glib-2.0 iconv"],
		[pcfiles="glib-2.0"])
	GLIB_CFLAGS="$GLIB_CFLAGS -DGLIB_STATIC_COMPILATION"
	GLIB_LIBS="`$PKG_CONFIG --libs --static $pcfiles` $GLIB_RPATH"
	])
AM_CONDITIONAL([STATIC_BUILD], [test "x$enable_static" = "xyes"])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

dnl Checks for header files.
AC_CHECK_HEADERS([errno.h inttypes.h stdio.h time.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT64_T
if test "x$ac_cv_c_uint64_t" = "xno" || test "x$ac_cv_c_uint64_t" = "x"; then
	AC_MSG_ERROR([This system does not support 64 bit integers.])
fi

dnl Checks for library functions.
AC_CHECK_FUNCS([setlocale strerror fseek fread memcpy])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
test/Makefile
test/atlocal
])

AC_OUTPUT
