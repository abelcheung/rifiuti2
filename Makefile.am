AUTOMAKE_OPTIONS = foreign dist-xz dist-zip

SUBDIRS = src po test

EXTRA_DIST = \
             docs      \
             README.md \
             NEWS.md   \
             $(NULL)

dist-hook:
	-find $(top_distdir) -type f \( -name '*~' -o -name '.*~' \) -print0 | xargs -0 -r rm -f

MAINTAINERCLEANFILES = \
                       $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL)        \
                       $(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN)     \
		       configure                                         \
		       po/Makefile.in.in                                 \
                       $(NULL)

-include $(top_srcdir)/dist-win.mk

distclean-local:
	-rm -rf win-pkg

-include $(top_srcdir)/git.mk
