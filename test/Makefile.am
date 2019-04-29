# vim: set sw=4 ts=4 noexpandtab :
#
# Copyright (C) 2015 Abel Cheung.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

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
