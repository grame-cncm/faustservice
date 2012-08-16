#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([faustserver], [0.0], [faustbug@grame.fr])
AC_CONFIG_SRCDIR([faustserver.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB([boost_random], [generate_uniform_int])
AC_CHECK_LIB([microhttpd], [MHD_queue_response])
AC_CHECK_PROG(HASPYTHON,[python],yes,no)

if test "x$HAXPYTHON" == "xno"; then
	AC_MSG_ERROR([python is needed to run faustserver])
fi

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK

#AC_CONFIG_FILES([Makefile])
AC_OUTPUT
