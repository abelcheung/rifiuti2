AUTOMAKE_OPTIONS = foreign dist-xz dist-zip

SUBDIRS = src po test

EXTRA_DIST = \
	     docs      \
	     README.md \
	     NEWS.md   \
	     $(NULL)

dist-hook:
	-find $(top_distdir) -type f \( -name '*~' -o -name '.*~' \) -print0 | xargs -0 -r rm -f

GITIGNOREFILES = depcomp

-include $(top_srcdir)/git.mk
