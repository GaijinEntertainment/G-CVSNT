dnl Process this file with autoconf to produce a configure script.

dnl Copyright (C) 2002, 2004, 2005 Simon Josefsson.
dnl
dnl This file is part of Libntlm.
dnl
dnl Libntlm is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU Lesser General Public License as
dnl published by the Free Software Foundation; either version 2.1 of
dnl the License, or (at your option) any later version.
dnl
dnl Libntlm is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with Libntlm; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
dnl 02110-1301, USA

AC_INIT([libntlm],[0.3.8],[simon@josefsson.org])
AC_CONFIG_SRCDIR(ntlm.h.in)

# Interfaces removed:    CURRENT++, AGE=0, REVISION=0
# Interfaces added:      CURRENT++, AGE++, REVISION=0
# No interfaces changed:                   REVISION++
AC_SUBST(LT_CURRENT, 0)
AC_SUBST(LT_AGE, 0)
AC_SUBST(LT_REVISION, 7)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
# AB_INIT
gl_EARLY
AM_DISABLE_SHARED
# For broken libtools (eg. the one in debian sarge) where AC_LIBTOOL_PICMODE
# is nonfunctional and the defaults are backwards..
if test "${with_pic+set}" != set; then
   with_pic="yes"
fi
AM_PROG_LIBTOOL

# For gnulib.
gl_INIT
AC_DEFINE(check_version, ntlm_check_version, [Rename to public API name.])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([unistd.h byteswap.h string.h strings.h sys/endian.h machine/endian.h endian.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

AC_CONFIG_FILES([Makefile lib/Makefile libntlm.pc libntlm.spec ntlm.h])
AC_OUTPUT
