# The `:;' works around a Bash 3.2 bug when the output is not writable.
$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	:;{ \
	echo '# Signature of the current package.' && \
	echo 'm4_define([AT_PACKAGE_NAME],  [$(PACKAGE_NAME)])' && \
	echo 'm4_define([AT_PACKAGE_TARNAME],  [$(PACKAGE_TARNAME)])' && \
	echo 'm4_define([AT_PACKAGE_VERSION],  [$(PACKAGE_VERSION)])' && \
	echo 'm4_define([AT_PACKAGE_STRING],  [$(PACKAGE_STRING)])' && \
	echo 'm4_define([AT_PACKAGE_BUGREPORT],  [$(PACKAGE_BUGREPORT)])'; \
	echo 'm4_define([AT_PACKAGE_URL],  [$(PACKAGE_URL)])'; \
	} >'$(srcdir)/package.m4'

TESTSUITE = $(srcdir)/testsuite

EXTRA_DIST = \
	     INFO2_test1 \
	     INFO2_test2 \
	     vista_test1 \
	     $(srcdir)/package.m4 \
	     testsuite.at \
	     $(TESTSUITE) \
	     atlocal.in

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

installcheck-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
		$(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	-rm -f *.tmp

AUTOM4TE = $(SHELL) $(top_srcdir)/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/testsuite.at $(srcdir)/package.m4
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

DISTCLEANFILES = atconfig atlocal
CLEANFILES = testsuite.log
MAINTAINERCLEANFILES = testsuite $(srcdir)/package.m4

-include $(top_srcdir)/git.mk
