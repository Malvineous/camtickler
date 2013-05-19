#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([camtickler], [0.1], [malvineous@shikadi.net], [], [http://www.shikadi.net/])
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_LANG(C++)

AC_PROG_CXX
AC_PROG_LIBTOOL

BOOST_REQUIRE([1.46])
BOOST_PROGRAM_OPTIONS
BOOST_ASIO
BOOST_REGEX
BOOST_TEST

m4_pattern_allow([BOOST_ASIO_HAS_SERIAL_PORT])

AC_MSG_CHECKING([for serial port support in boost::asio])
AC_EGREP_CPP(yes_have_boost_asio_serial, [
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_SERIAL_PORT
yes_have_boost_asio_serial
#endif
], [
	AC_MSG_RESULT([yes])
], [
	AC_MSG_RESULT([no])
	AC_ERROR([boost::asio must be compiled with serial port support enabled])
])

AM_SILENT_RULES([yes])

AC_OUTPUT(Makefile src/Makefile tests/Makefile)
