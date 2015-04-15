dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)
AC_INIT([rifiuti2],[0.6.0],[https://github.com/abelcheung/rifiuti2/issues])
AC_CONFIG_SRCDIR([src/rifiuti-vista.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_TESTDIR([test])
AM_INIT_AUTOMAKE


dnl I18N
GETTEXT_PACKAGE=rifiuti
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"],
  [Define the gettext package to be used])

AM_GLIB_GNU_GETTEXT

dnl Compiler checks.
AC_PROG_CC
AC_PROG_CC_STDC

# https://www.varnish-cache.org/lists/pipermail/varnish-commit/2011-March/006315.html
if test "x$ac_cv_prog_cc_c99" = "xno" || test "x$ac_cv_prog_cc_c99" = "x"; then
  if test "x$ac_cv_c_compiler_gnu" = "yes"; then
    CC="$CC -std=gnu99"
    AC_RUN_IFELSE(
		    [AC_LANG_PROGRAM([],[[ return 0; ]])], [],
		    [AC_MSG_ERROR([Could not find a C99 compatible compiler])])
  else
    AC_MSG_ERROR([Could not find a C99 compatible compiler])
  fi
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


dnl Checks for libraries.
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.6.0, :,
  AC_MSG_ERROR([
        *** GLib not found. You can find it on ftp://ftp.gtk.org
        *** Errors follow:
            $DEP_PKG_ERRORS]))
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([locale.h stdint.h stdlib.h string.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_STRUCT_TM
dnl AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

dnl Checks for library functions.
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([setlocale strerror])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
test/Makefile
test/atlocal
])

AC_OUTPUT
