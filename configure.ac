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


dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
dnl For test suite
AC_CHECK_PROG([XMLLINT], [xmllint], [xmllint])
AC_CHECK_PROG([ICONV], [iconv], [iconv])
AC_CHECK_PROG([PERL], [perl], [perl])

dnl Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16.0], [:],
                  AC_MSG_ERROR([*** GLib not found. Errors follow:
	$DEP_PKG_ERRORS]))
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


dnl Checks for header files.
AC_CHECK_HEADERS([errno.h inttypes.h stdio.h time.h])

dnl Platform specific check.
case "$host" in
	*-*-mingw*) os_mingw=y ;;
	*-*-netbsd*)  os_netbsd=y ;;
esac
AC_SUBST(os_mingw)
AC_SUBST(os_netbsd)
AM_CONDITIONAL([OS_WINDOWS], [test "x$os_mingw" != x])

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
