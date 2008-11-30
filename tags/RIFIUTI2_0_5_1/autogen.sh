#!/bin/sh
# Run this to generate all the stuff before configure (which only
# maintainers should generate).

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

PROJECT=rifiuti2
TEST_TYPE=-d
FILE=src

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://ftp.cygnus.com/pub/home/tromey/automake-1.2d.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

case $CC in
*lcc | *lcc\ *) am_opt=--include-deps;;
esac

echo "Running gettextize..."
glib-gettextize --copy --force

aclocal $ACLOCAL_FLAGS
autoheader
automake --add-missing --copy $am_opt
autoconf

cd $ORIGDIR

# Helper printing functions and some terminal codes, taken from
# gnome-common/macros2/gnome-autogen.sh ...
boldface="`tput bold 2>/dev/null`"
normal="`tput sgr0 2>/dev/null`"
printbold() {
    echo $ECHO_N "$boldface"
    echo "$@"
    echo $ECHO_N "$normal"
}
printerr() {
    echo "$@" >&2
}

conf_flags="--enable-maintainer-mode"
if test x$NOCONFIGURE = x; then
    if [ "$#" = 0 ]; then
      printerr "**Warning**: I am going to run \`configure' with no arguments."
      printerr "If you wish to pass any to it, please specify them on the"
      printerr \`$0\'" command line."
      printerr
    fi  

    printbold Running $srcdir/configure $conf_flags "$@"
    $srcdir/configure $conf_flags "$@" \
        && echo Now type \`make\' to compile $PROJECT || exit 1
else
    echo You may want to run $srcdir/configure $conf_flags "$@"
    echo to build $PROJECT
fi
