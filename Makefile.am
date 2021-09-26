ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libaaelftools.la

include_HEADERS = $(top_srcdir)/include/aa-elf-util.h

libaaelftools_la_SOURCES = $(top_srcdir)/src/util.c
libaaelftools_la_LIBADD = -lelf

AM_CPPFLAGS = -I$(top_srcdir)/include

bin_PROGRAMS = findelfs
findelfs_SOURCES = $(top_srcdir)/src/findelfs.c
findelfs_LDADD = libaaelftools.la
