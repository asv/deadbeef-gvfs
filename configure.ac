##
## Copyright (c) 2009 Alexey Smirnov <alexey.smirnov@gmx.com>
##
## See the file LICENSE for information on usage and redistribution
## of this file, and for a DISCLAMER OF ALL WARRANTIES.
##

AC_PREREQ(2.61)
AC_INIT([deadbeef-gvfs], [0.1], [aysv@users.sourceforge.net])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

AC_HEADER_STDC

AC_CHECK_HEADERS([deadbeef/deadbeef.h],,
    [
     AC_MSG_ERROR([missing required header (see above)])
    ])

PKG_CHECK_MODULES([GIO], [gio-2.0])
AC_SUBST([libdir], [$libdir/deadbeef])

AC_OUTPUT([Makefile src/Makefile])
