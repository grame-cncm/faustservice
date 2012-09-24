#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([faustserver], [0.0], [mike@mikesolomon.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
BOOST_REQUIRE([1.50.0])

# hack...
LIBS="$LIBS -lboost_filesystem -lboost_system -lboost_program_options"

AC_SEARCH_LIBS([MHD_queue_response], [microhttpd], [], [
  AC_MSG_ERROR([Unable to find libmicrohttpd])
])

AC_SEARCH_LIBS([pthread_create], [pthread], [], [
  AC_MSG_ERROR([Unable to find libpthread])
])

AC_MSG_CHECKING([for library containing CryptoPP::SHA1])
AC_LANG_PUSH([C++])
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -lcryptopp"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <cryptopp/sha.h>],
                   [CryptoPP::SHA1 sha1;])],
  [LIBS="$LIBS -lcryptopp"] [HAVE_CRYPTOPP=1],
  [AC_MSG_ERROR([libcryptopp is not installed.])])
LDFLAGS=$SAVED_LDFLAGS
AC_LANG_POP([C++])
AC_MSG_RESULT([-lcryptopp])

AC_SEARCH_LIBS([archive_read_support_filter_all], [archive], [], [
  AC_MSG_ERROR([Unable to find libarchive])
])


# Checks for header files.
AC_CHECK_HEADERS([sstream iostream string map vector stdlib.h string.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
