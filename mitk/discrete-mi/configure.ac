#                                               -*- Autoconf -*-

#
# Written by Richard Rogers <rprogers@uw.edu>
#
# Copyright (C) 2010 Jeff Bilmes
# Licensed under the Open Software License version 3.0
# See COPYING or http://opensource.org/licenses/OSL-3.0
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(GMTK, 0.0, https://j.ee.washington.edu/trac/gmtk/newticket)
: ${CXXFLAGS=""}
AC_CONFIG_SRCDIR([mixNormalCollection.cc])
AC_CONFIG_HEADER([ac_config.h])

AM_INIT_AUTOMAKE([1.12 silent-rules -Wall -Werror foreign])
AM_SILENT_RULES([yes])

AC_ARG_ENABLE([assertions],
              [AS_HELP_STRING([--disable-assertions],
                              [disable assertion checking @<:@default: no@:>@])],
              [assertions=${enableval}], [assertions=yes])

AC_ARG_ENABLE([model-checks],
              [AS_HELP_STRING([--disable-model-checks],
                              [disable run-time checks for models @<:@default: no@:>@])],
              [model_checks=${enableval}], [model_checks=yes])

AM_CONDITIONAL([NO_ASSERTIONS],[test x$assertions != xyes])
AM_CONDITIONAL([NO_MODEL_CHECKS],[test x$model_checks != xyes])

# Checks for programs
AC_PROG_CXX
AM_CONDITIONAL([USE_GCC_FLAGS],[test x$GXX = xyes])
m4_ifdef([AM_PROG_AR],[AM_PROG_AR])

# Checks for libraries.
AC_CHECK_LIB([m], [log])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h values.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([long])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([strchr strtol])

AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
