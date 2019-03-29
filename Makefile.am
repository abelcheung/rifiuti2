# vim: set sw=4 ts=4 noexpandtab :

AUTOMAKE_OPTIONS = foreign dist-xz dist-zip

SUBDIRS = src po test

EXTRA_DIST = \
	docs      \
	README.md \
	NEWS.md   \
	$(NULL)

dist-hook:
	-find $(top_distdir) -type f \( -name '*~' -o -name '.*~' \) -print0 | xargs -0 -r rm -f

# despite what manual says, I want all build auxiliary files gone
MAINTAINERCLEANFILES = \
	$(top_srcdir)/configure          \
	$(top_srcdir)/aclocal.m4         \
	$(top_srcdir)/compile            \
	$(top_srcdir)/config.guess       \
	$(top_srcdir)/config.h.in        \
	$(top_srcdir)/config.sub         \
	$(top_srcdir)/depcomp            \
	$(top_srcdir)/install-sh         \
	$(top_srcdir)/Makefile.in        \
	$(top_srcdir)/missing            \
	$(NULL)

distclean-local:
	-rm -rf win-pkg
