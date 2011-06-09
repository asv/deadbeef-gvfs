
lib_LTLIBRARIES = ddb_gvfs.la
ddb_gvfs_la_SOURCES = deadbeef-gvfs.c
ddb_gvfs_la_LDFLAGS = -module
ddb_gvfs_la_LIBADD = $(LDADD) $(GIO_LIBS)

AM_CFLAGS = $(CFLAGS) $(GIO_CFLAGS) -std=c99
