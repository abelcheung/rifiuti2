# Copyright (C) 2015-2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

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
	} > $@

noinst_PROGRAMS = test_glib_iconv

test_glib_iconv_LDADD = $(GLIB_LIBS)
test_glib_iconv_CFLAGS = $(GLIB_CFLAGS)

TESTSUITE = $(srcdir)/testsuite

INC_AT_FILES = \
	ax_at_check_pattern.m4 \
	test_cli_option.at   \
	test_common.at       \
	test_encoding.at     \
	test_faulty_dir.at   \
	test_parse_dir.at    \
	test_parse_info2.at  \
	test_read_write.at   \
	test_xml.at

AUTOM4TE = $(SHELL) $(top_srcdir)/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest

# Autotest won't use AC_ macros, therefore pass the result
# of AC_CHECK_PROGS as external variables
TEST_FLAGS_EXTRA = 'AWK=$(AWK)'

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS) $(TEST_FLAGS_EXTRA)

installcheck-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
		$(TESTSUITEFLAGS) $(TEST_FLAGS_EXTRA)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	-rm -f *.tmp

$(TESTSUITE): $(srcdir)/testsuite.at $(srcdir)/package.m4 $(INC_AT_FILES)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

EXTRA_DIST = \
	rifiuti.dtd          \
	samples              \
	$(srcdir)/package.m4 \
	testsuite.at         \
	$(TESTSUITE)         \
	atlocal.in           \
	$(INC_AT_FILES)      \
	$(NULL)

DISTCLEANFILES = atconfig atlocal
CLEANFILES = testsuite.log
MAINTAINERCLEANFILES = \
	$(TESTSUITE)           \
	$(srcdir)/package.m4   \
	$(srcdir)/Makefile.in  \
	$(NULL)
