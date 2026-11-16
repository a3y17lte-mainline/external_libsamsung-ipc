NULL =

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	$(NULL)

bin_PROGRAMS = \
	ipc-test \
	nv_data-md5 \
	$(NULL)

if WANT_VALGRIND_CHECKING
AM_TESTS_ENVIRONMENT = VALGRIND='$(VALGRIND)'; export VALGRIND;
endif
PY_LOG_COMPILER = $(PYTHON)
TEST_EXTENSIONS = .py
TESTS = \
	tests/nv_data-md5.py

ipc_test_SOURCES = ipc-test.c
ipc_test_LDADD = $(top_builddir)/samsung-ipc/libsamsung-ipc.la
ipc_test_LDFLAGS =

nv_data_md5_SOURCES = nv_data-md5.c
nv_data_md5_LDADD = $(top_builddir)/samsung-ipc/libsamsung-ipc.la
nv_data_md5_LDFLAGS =

