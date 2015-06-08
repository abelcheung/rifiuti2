dnl Process this file with autoconf to produce a configure script.

AC_INIT([rifiuti2], [0.7.0],
        [https://github.com/abelcheung/rifiuti2/issues],,
        [https://abelcheung.github.io/rifiuti2/])
AC_CONFIG_SRCDIR([src/rifiuti-vista.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_TESTDIR([test])
AM_INIT_AUTOMAKE([1.11])

AC_CANONICAL_HOST

dnl I18N
GETTEXT_PACKAGE=rifiuti
ALL_LINGUAS="`cat $srcdir/po/LINGUAS`"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"],
                   [Define the gettext package to be used])

AM_GLIB_GNU_GETTEXT

dnl Compiler checks.
AC_PROG_CC
AC_PROG_CC_STDC

if test "x$ac_cv_prog_cc_c99" = "xno"; then
	AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi

dnl Always use -Wall for gcc
changequote(,)dnl
if test "x$GCC" = "xyes"; then
	case " $CFLAGS " in
		*[\ \ ]-Wall[\ \	]*) ;;
		*) CFLAGS="$CFLAGS -Wall" ;;
	esac
fi
changequote([,])dnl

dnl Platform specific check.
case "$host" in
	*-*-mingw*)   os_mingw=y ;;
	*-*-netbsd*)  os_netbsd=y ;;
	*-*-solaris*) os_solaris=y ;;
esac
AC_SUBST(os_mingw)
AC_SUBST(os_netbsd)
AM_CONDITIONAL([OS_WINDOWS], [test "x$os_mingw" != x])


dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
dnl For test suite
AC_PATH_PROG([XMLLINT],[xmllint])
AC_PATH_PROG([PERL],[perl])
AC_CACHE_CHECK([for iconv that supports more Windows codepages], [ac_cv_path_ICONV],
               [AC_PATH_PROGS_FEATURE_CHECK([ICONV], [iconv],
                [$ac_path_ICONV -f UTF-8 -t CP950 < /dev/null > /dev/null 2>&1 \
                 && ac_cv_path_ICONV=$ac_path_ICONV ac_path_ICONV_found=:])])
AC_SUBST([ICONV],[$ac_cv_path_ICONV])

dnl Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16.0],,
                  [AC_MSG_ERROR([*** GLib not found. Error is: $DEP_PKG_ERRORS])])

dnl See if glib is linked to Solaris implementation of iconv
dnl Too many code pages support were missing
if test -n "$os_solaris"; then
save_CFLAGS="$CFLAGS"
save_LDFLAGS="$LDFLAGS"
# todo: Determine path for 32/64bit
GLIB_RPATH=`pkg-config --libs-only-L glib-2.0 | sed -e 's#-L#-R#g'`
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LDFLAGS="$LDFLAGS $GLIB_LIBS $GLIB_RPATH"
AC_LANG_ASSERT([C])
AC_MSG_CHECKING([whether glib2 is linked with appropriate iconv])
AC_RUN_IFELSE([AC_LANG_PROGRAM([dnl
#include <glib.h>
#include <glib/gi18n.h>
], [return (g_iconv_open("UTF-8","CP950") == (GIConv) -1);])],
              [AC_MSG_RESULT([yes])],
              [AC_MSG_RESULT([no])
               AC_MSG_ERROR([
iconv() implementation used by Solaris stock glib2 is not supported.
Please install external glib2 package that uses GNU libiconv
on such system, and set \@S|@PKG_CONFIG_PATH accordingly.
Glib2 packages from OpenCSW or Sunfreeware are known to work.])])

CFLAGS="$save_CFLAGS"
LDFLAGS="$save_LDFLAGS"
GLIB_LIBS="$GLIB_LIBS $GLIB_RPATH"
fi

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

dnl vim: set sw=4 ts=4 noexpandtab :
