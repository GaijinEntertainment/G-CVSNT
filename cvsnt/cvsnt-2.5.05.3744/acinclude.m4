/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

AC_DEFUN([ACX_WITH_GSSAPI],[
#
# Use --with-gssapi[=DIR] to enable GSSAPI support.
#
# defaults to enabled with DIR in default list below
#
# Search for /SUNHEA/ and read the comments about this default below.
#
AC_ARG_WITH([gssapi],
	    [  --with-gssapi=value     GSSAPI directory],
	    [acx_gssapi_withgssapi=$withval], [acx_gssapi_withgssapi=yes])dnl

dnl
dnl FIXME - cache withval and obliterate later cache values when options change
dnl
#
# Try to locate a GSSAPI installation if no location was specified, assuming
# GSSAPI was enabled (the default).
# 
AC_CACHE_VAL([acx_gssapi_cv_gssapi], [
if test x$acx_gssapi_withgssapi = xyes; then
  # --with but no location specified
  # assume a gssapi.h or gssapi/gssapi.h locates our install.
  #
  # This isn't always strictly true.  For instance Solaris 7's SUNHEA (header)
  # package installs gssapi.h whether or not the necessary libraries are
  # installed.  I'm still not sure whether to consider this a bug.  The long
  # way around is to not consider GSSPAI installed unless gss_import_name is
  # found, but that brings up a lot of other hassles, like continuing to let
  # gcc & ld generate the error messages when the user uses --with-gssapi=dir
  # as a debugging aid.  The short way around is to disable GSSAPI by default,
  # but I think Sun users have been faced with this for awhile and I haven't
  # heard many complaints.

  AC_PATH_PROG(KRB5_CONFIG, krb5-config)
  if test -x "$KRB5_CONFIG"; then
    GSSAPI_INCLUDES=`$KRB5_CONFIG --cflags gssapi`
    GSSAPI_LIBS=`$KRB5_CONFIG --libs gssapi`
  else 
    acx_gssapi_save_CPPFLAGS=$CPPFLAGS
    for acx_gssapi_cv_gssapi in yes /usr/kerberos /usr/cygnus/kerbnet no; do
      if test x$acx_gssapi_cv_gssapi = xno; then
        break
      fi
      if test x$acx_gssapi_cv_gssapi = xyes; then
        AC_CHECKING([for GSSAPI])
      else
        CPPFLAGS="$acx_gssapi_save_CPPFLAGS -I$acx_gssapi_cv_gssapi/include"
        AC_CHECKING([for GSSAPI in $acx_gssapi_cv_gssapi])
      fi
      unset ac_cv_header_gssapi_h
      unset ac_cv_header_gssapi_gssapi_h
      AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h])
      if test "$ac_cv_header_gssapi_h" = "yes" ||
          test "$ac_cv_header_gssapi_gssapi_h" = "yes"; then
        break
      fi
    done
    AC_MSG_CHECKING([for krb5.h])
    AC_TRY_LINK([#include <krb5.h>],[int i;],,[acx_gssapi_cv_gssapi="no"])
    AC_MSG_RESULT($acx_gssapi_cv_gssapi)
    CPPFLAGS=$acx_gssapi_save_CPPFLAGS
#
# Set up GSSAPI includes for later use.  We don't bother to check for
# $acx_gssapi_cv_gssapi=no here since that will be caught later.
#
    if test x$acx_gssapi_cv_gssapi = xyes; then # no special includes necessary
      GSSAPI_INCLUDES=""
      GSSAPI_LIBS=""
    else # GSSAPI at $acx_gssapi_cv_gssapi (could be 'no')
      GSSAPI_INCLUDES="-I$acx_gssapi_cv_gssapi/include"
      GSSAPI_LIBS="-L$acx_gssapi_cv_gssapi/lib"
    fi
  fi
else
  acx_gssapi_cv_gssapi=$acx_gssapi_withgssapi
  GSSAPI_INCLUDES="-I$acx_gssapi_cv_gssapi/include"
  GSSAPI_LIBS="-L$acx_gssapi_cv_gssapi/lib"
fi

])dnl

AM_CONDITIONAL(WITH_GSSAPI, test x$acx_gssapi_cv_gssapi != xno)

#
# Get the rest of the information CVS needs to compile with GSSAPI support
#
if test x$acx_gssapi_cv_gssapi != xno; then
  # define HAVE_GSSAPI and set up the includes
  AC_DEFINE([HAVE_GSSAPI],, [Define if you have GSSAPI with Kerberos version 5 available.])
  includeopt=$includeopt$GSSAPI_INCLUDES

  # locate any other headers
  CPPFLAGS="$CPPFLAGS $GSSAPI_INCLUDES"
  dnl We don't use HAVE_KRB5_H anywhere, but including it here might make it
  dnl easier to spot errors by reading configure output
  AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h gssapi/gssapi_generic.h])
  # And look through them for GSS_C_NT_HOSTBASED_SERVICE or its alternatives
  AC_CACHE_CHECK([for GSS_C_NT_HOSTBASED_SERVICE], [acx_gssapi_cv_gss_c_nt_hostbased_service],
   [acx_gssapi_cv_gss_c_nt_hostbased_service=no
    if test "$ac_cv_header_gssapi_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
        test "$ac_cv_header_gssapi_gssapi_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi/gssapi.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
        test "$ac_cv_header_gssapi_gssapi_generic_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi_generic.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi/gssapi_generic.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi])
  if test $acx_gssapi_cv_gss_c_nt_hostbased_service != yes &&
      test $acx_gssapi_cv_gss_c_nt_hostbased_service != no; then
    # don't define for yes since that means it already means something and
    # don't define for no since we'd rather the compiler catch the error
    AC_DEFINE_UNQUOTED([GSS_C_NT_HOSTBASED_SERVICE], [$acx_gssapi_cv_gss_c_nt_hostbased_service],
[Define to an alternative value if GSS_C_NT_HOSTBASED_SERVICE isn't defined
in the gssapi.h header file.  MIT Kerberos 1.2.1 requires this.  Only relevant
when using GSSAPI.])
  fi

  # Expect the libs to be installed parallel to the headers
  #
  # We could try once with and once without, but I'm not sure it's worth the
  # trouble.
  LIBS="$LIBS $GSSAPI_LIBS"

  dnl What happens if we want to enable, say, krb5 and some other GSSAPI
  dnl authentication method at the same time?
  #
  # Some of the order below is particular due to library dependencies
  #

  #
  # des			Heimdal K 0.3d, but Heimdal seems to be set up such
  #			that it could have been installed from elsewhere.
  #
  AC_SEARCH_LIBS([des_set_odd_parity], [des])

  #
  # com_err		Heimdal K 0.3d
  #
  # com_err		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([com_err], [com_err])

  #
  # asn1		Heimdal K 0.3d		-lcom_err
  #
  AC_SEARCH_LIBS([initialize_asn1_error_table_r], [asn1])

  #
  # resolv		required, but not installed by Heimdal K 0.3d
  #
  # resolv		MIT K5 1.2.2-beta1
  # 			Linux 2.2.17
  #
  AC_SEARCH_LIBS([__dn_expand], [resolv])

  #
  # roken		Heimdal K 0.3d		-lresolv
  #
  AC_SEARCH_LIBS([roken_gethostbyaddr], [roken])

  #
  # k5crypto		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([valid_enctype], [k5crypto])

  #
  # gen			? ? ?			Needed on Irix 5.3 with some
  #			Irix 5.3		version of Kerberos.  I'm not
  #						sure which since Irix didn't
  #						get any testing this time
  #						around.  Original comment:
  #
  # This is necessary on Irix 5.3, in order to link against libkrb5 --
  # there, an_to_ln.o refers to things defined only in -lgen.
  #
  AC_SEARCH_LIBS([compile], [gen])

  #
  # krb5		? ? ?			-lgen -l???
  #			Irix 5.3
  #
  # krb5		MIT K5 v1.1.1
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err
  # 			Linux 2.2.17
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err -lresolv
  #
  # krb5		Heimdal K 0.3d		-lasn1 -lroken -ldes
  #
  AC_SEARCH_LIBS([krb5_free_context], [krb5])

  #
  # gssapi_krb5		Only lib needed with MIT K5 v1.2.1, so find it first in
  #			order to prefer MIT Kerberos.  If both MIT & Heimdal
  #			Kerberos are installed and in the path, this will leave
  #			some of the libraries above in LIBS unnecessarily, but
  #			noone would ever do that, right?
  #
  # gssapi_krb5		MIT K5 v1.2.2-beta1	-lkrb5
  #
  # gssapi		Heimdal K 0.3d		-lkrb5
  #
  AC_SEARCH_LIBS([gss_import_name], [gssapi_krb5 gssapi])
fi
])dnl

AC_DEFUN([AC_TYPE_SOCKLEN_T], [
   dnl Since the old-style (autoconf 2.13) macro AC_CHECK_TYPE(type, replacement)
   dnl only checks in <sys/types.h> we have to be more awkward:
   dnl We have to check for socklen_t in <sys/types.h> -- if it's not defined
   dnl there, we also look in <sys/socket.h> (a more common place)
   AC_CHECK_TYPE(socklen_t,,
     AC_DEFINE(socklen_t, int, [Define to `int' if neither <sys/types.h> nor <sys/socket.h> define.]),
     [
#include <sys/types.h>
#include <sys/socket.h>
   ])
])

dnl @synopsis ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
dnl
dnl This macro figures out how to build C programs using POSIX
dnl threads.  It sets the PTHREAD_LIBS output variable to the threads
dnl library and linker flags, and the PTHREAD_CFLAGS output variable
dnl to any special C compiler flags that are needed.  (The user can also
dnl force certain compiler flags/libs to be tested by setting these
dnl environment variables.)
dnl
dnl Also sets PTHREAD_CC to any special C compiler that is needed for
dnl multi-threaded programs (defaults to the value of CC otherwise).
dnl (This is necessary on AIX to use the special cc_r compiler alias.)
dnl
dnl NOTE: You are assumed to not only compile your program with these
dnl flags, but also link it with them as well.  e.g. you should link
dnl with $PTHREAD_CC $CFLAGS $PTHREAD_CFLAGS $LDFLAGS ... $PTHREAD_LIBS $LIBS
dnl
dnl If you are only building threads programs, you may wish to
dnl use these variables in your default LIBS, CFLAGS, and CC:
dnl
dnl        LIBS="$PTHREAD_LIBS $LIBS"
dnl        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
dnl        CC="$PTHREAD_CC"
dnl
dnl In addition, if the PTHREAD_CREATE_JOINABLE thread-attribute
dnl constant has a nonstandard name, defines PTHREAD_CREATE_JOINABLE
dnl to that name (e.g. PTHREAD_CREATE_UNDETACHED on AIX).
dnl
dnl ACTION-IF-FOUND is a list of shell commands to run if a threads
dnl library is found, and ACTION-IF-NOT-FOUND is a list of commands
dnl to run it if it is not found.  If ACTION-IF-FOUND is not specified,
dnl the default action will define HAVE_PTHREAD.
dnl
dnl Please let the authors know if this macro fails on any platform,
dnl or if you have any other suggestions or comments.  This macro was
dnl based on work by SGJ on autoconf scripts for FFTW (www.fftw.org)
dnl (with help from M. Frigo), as well as ac_pthread and hb_pthread
dnl macros posted by Alejandro Forero Cuervo to the autoconf macro
dnl repository.  We are also grateful for the helpful feedback of
dnl numerous users.
dnl
dnl @version $Id: acinclude.m4,v 1.7.2.15 2006/06/28 14:57:33 tmh Exp $
dnl @author Steven G. Johnson <stevenj@alum.mit.edu>

AC_DEFUN([ACX_PTHREAD], [
AC_REQUIRE([AC_CANONICAL_HOST])
AC_LANG_SAVE
AC_LANG_C
acx_pthread_ok=no

# We used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on True64 or Sequent).
# It gets checked for in the link test anyway.

# First of all, check if the user has set any of the PTHREAD_LIBS,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$PTHREAD_LIBS$PTHREAD_CFLAGS" != x; then
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        AC_MSG_CHECKING([for pthread_join in LIBS=$PTHREAD_LIBS with CFLAGS=$PTHREAD_CFLAGS])
        AC_TRY_LINK_FUNC(pthread_join, acx_pthread_ok=yes)
        AC_MSG_RESULT($acx_pthread_ok)
        if test x"$acx_pthread_ok" = xno; then
                PTHREAD_LIBS=""
                PTHREAD_CFLAGS=""
        fi
        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"
fi

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all, and "pthread-config"
# which is a program returning the flags for the Pth emulation library.

acx_pthread_flags="pthreads none -Kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt pthread-config"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
# lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads)
# -pthreads: Solaris/gcc
# -mthreads: Mingw32/gcc, Lynx/gcc
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -D_REENTRANT)
# pthread: Linux, etcetera
# --thread-safe: KAI C++
# pthread-config: use pthread-config program (for GNU Pth library)

case "${host_cpu}-${host_os}" in
        *solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (We need to link with -pthread or
        # -lpthread.)  (The stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  So,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthread -pthreads pthread -mt $acx_pthread_flags"
        ;;
esac

if test x"$acx_pthread_ok" = xno; then
for flag in $acx_pthread_flags; do

        case $flag in
                none)
                AC_MSG_CHECKING([whether pthreads work without any flags])
                ;;

                -*)
                AC_MSG_CHECKING([whether pthreads work with $flag])
                PTHREAD_CFLAGS="$flag"
                ;;

		pthread-config)
		AC_CHECK_PROG(acx_pthread_config, pthread-config, yes, no)
		if test x"$acx_pthread_config" = xno; then continue; fi
		PTHREAD_CFLAGS="`pthread-config --cflags`"
		PTHREAD_LIBS="`pthread-config --ldflags` `pthread-config --libs`"
		;;

                *)
                AC_MSG_CHECKING([for the pthreads library -l$flag])
                PTHREAD_LIBS="-l$flag"
                ;;
        esac

        save_LIBS="$LIBS"
        save_CFLAGS="$CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.
        AC_TRY_LINK([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        AC_MSG_RESULT($acx_pthread_ok)
        if test "x$acx_pthread_ok" = xyes; then
                break;
        fi

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi

# Various other checks:
if test "x$acx_pthread_ok" = xyes; then
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Detect AIX lossage: JOINABLE attribute is called UNDETACHED.
	AC_MSG_CHECKING([for joinable pthread attribute])
	attr_name=unknown
	for attr in PTHREAD_CREATE_JOINABLE PTHREAD_CREATE_UNDETACHED; do
	    AC_TRY_LINK([#include <pthread.h>], [int attr=$attr;],
                        [attr_name=$attr; break])
	done
        AC_MSG_RESULT($attr_name)
        if test "$attr_name" != PTHREAD_CREATE_JOINABLE; then
            AC_DEFINE_UNQUOTED(PTHREAD_CREATE_JOINABLE, $attr_name,
                               [Define to necessary symbol if this constant
                                uses a non-standard name on your system.])
        fi

        AC_MSG_CHECKING([if more special flags are required for pthreads])
        flag=no
        case "${host_cpu}-${host_os}" in
            *-aix* | *-freebsd* | *-darwin*) flag="-D_THREAD_SAFE";;
            *solaris* | *-osf* | *-hpux*) flag="-D_REENTRANT";;
        esac
        AC_MSG_RESULT(${flag})
        if test "x$flag" != xno; then
            PTHREAD_CFLAGS="$flag $PTHREAD_CFLAGS"
        fi

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        # More AIX lossage: must compile with cc_r
        AC_CHECK_PROG(PTHREAD_CC, cc_r, cc_r, ${CC})
else
        PTHREAD_CC="$CC"
fi

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_CC)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi
AC_LANG_RESTORE
])dnl ACX_PTHREAD

# GLIB_CHECK_COMPILE_WARNINGS(PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])# ---------------------------------------------------------------------
# Try to compile PROGRAM, check for warnings
m4_define([GLIB_CHECK_COMPILE_WARNINGS],
[m4_ifvaln([$1], [AC_LANG_CONFTEST([$1])])dnl
rm -f conftest.$ac_objext
glib_ac_compile_save="$ac_compile"
ac_compile='$CC -c $CFLAGS $CPPFLAGS conftest.$ac_ext'
AS_IF([_AC_EVAL_STDERR($ac_compile) &&
         AC_TRY_COMMAND([(if test -s conftest.err; then false ; else true; fi)])],
      [$2],
      [echo "$as_me: failed program was:" >&AS_MESSAGE_LOG_FD
cat conftest.$ac_ext >&AS_MESSAGE_LOG_FD
m4_ifvaln([$3],[$3])dnl])dnl
ac_compile="$glib_ac_compile_save"
rm -f conftest.$ac_objext conftest.err m4_ifval([$1], [conftest.$ac_ext])[]dnl
])# GLIB_CHECK_COMPILE_WARNINGS

dnl @synopsis AX_CHECK_COMPILER_FLAGS(FLAGS, [ACTION-SUCCESS], [ACTION-FAILURE])
dnl
dnl @summary check whether FLAGS are accepted by the compiler
dnl
dnl Check whether the given compiler FLAGS work with the current
dnl language's compiler, or whether they give an error. (Warnings,
dnl however, are ignored.)
dnl
dnl ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
dnl success/failure.
dnl
dnl @category Misc
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Matteo Frigo.
dnl @version 2005-05-30
dnl @license GPLWithACException

AC_DEFUN([AX_CHECK_COMPILER_FLAGS],
[AC_PREREQ(2.59) dnl for _AC_LANG_PREFIX
AC_MSG_CHECKING([whether _AC_LANG compiler accepts $1])
dnl Some hackery here since AC_CACHE_VAL can't handle a non-literal varname:
AS_LITERAL_IF([$1],
  [AC_CACHE_VAL(AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1), [
      ax_save_FLAGS=$[]_AC_LANG_PREFIX[]FLAGS
      _AC_LANG_PREFIX[]FLAGS="$1"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
        AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1)=yes,
        AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1)=no)
      _AC_LANG_PREFIX[]FLAGS=$ax_save_FLAGS])],
  [ax_save_FLAGS=$[]_AC_LANG_PREFIX[]FLAGS
   _AC_LANG_PREFIX[]FLAGS="$1"
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
     eval AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1)=yes,
     eval AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1)=no)
   _AC_LANG_PREFIX[]FLAGS=$ax_save_FLAGS])
eval ax_check_compiler_flags=$AS_TR_SH(ax_cv_[]_AC_LANG_ABBREV[]_flags_$1)
AC_MSG_RESULT($ax_check_compiler_flags)
if test "x$ax_check_compiler_flags" = xyes; then
	m4_default([$2], :)
else
	m4_default([$3], :)
fi
])dnl AX_CHECK_COMPILER_FLAGS

dnl @synopsis AX_COMPILER_VENDOR
dnl
dnl @summary find the vendor (gnu, intel, etc.) of the C/C++ compiler
dnl
dnl Determine the vendor of the C/C++ compiler, e.g., gnu, intel, ibm,
dnl sun, hp, borland, comeau, dec, cray, kai, lcc, metrowerks, sgi,
dnl microsoft, watcom, etc. The vendor is returned in the cache
dnl variable $ax_cv_c_compiler_vendor for C and
dnl $ax_cv_cxx_compiler_vendor for C++.
dnl
dnl @category C
dnl @category Cxx
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> with Matteo Frigo
dnl @version 2005-05-30
dnl @license GPLWithACException

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for gcc first since some other compilers define __GNUC__
  for ventest in intel:__ICC,__ECC,__INTEL_COMPILER ibm:__xlc__,__xlC__,__IBMC__,__IBMCPP__ gnu:__GNUC__ sun:__SUNPRO_C,__SUNPRO_CC hp:__HP_cc,__HP_aCC dec:__DECC,__DECCXX,__DECC_VER,__DECCXX_VER borland:__BORLANDC__,__TURBOC__ comeau:__COMO__ cray:_CRAYC kai:__KCC lcc:__LCC__ metrowerks:__MWERKS__ sgi:__sgi,sgi microsoft:_MSC_VER watcom:__WATCOMC__ portland:__PGI; do
    vencpp="defined("`echo $ventest | cut -d: -f2 | sed 's/,/) || defined(/g'`")"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
#if !($vencpp)
      thisisanerror;
#endif
])], [ax_cv_]_AC_LANG_ABBREV[_compiler_vendor=`echo $ventest | cut -d: -f1`; break])
  done
 ])
])



psis AX_GCC_ARCHFLAG([PORTABLE?], [ACTION-SUCCESS], [ACTION-FAILURE])
dnl
dnl @summary find target architecture name for gcc -march/-mtune flags
dnl
dnl This macro tries to guess the "native" arch corresponding to the
dnl target architecture for use with gcc's -march=arch or -mtune=arch
dnl flags. If found, the cache variable $ax_cv_gcc_archflag is set to
dnl this flag and ACTION-SUCCESS is executed; otherwise
dnl $ax_cv_gcc_archflag is is set to "unknown" and ACTION-FAILURE is
dnl executed. The default ACTION-SUCCESS is to add $ax_cv_gcc_archflag
dnl to the end of $CFLAGS.
dnl
dnl PORTABLE? should be either [yes] (default) or [no]. In the former
dnl case, the flag is set to -mtune (or equivalent) so that the
dnl architecture is only used for tuning, but the instruction set used
dnl is still portable. In the latter case, the flag is set to -march
dnl (or equivalent) so that architecture-specific instructions are
dnl enabled.
dnl
dnl The user can specify --with-gcc-arch=<arch> in order to override
dnl the macro's choice of architecture, or --without-gcc-arch to
dnl disable this.
dnl
dnl When cross-compiling, or if $CC is not gcc, then ACTION-FAILURE is
dnl called unless the user specified --with-gcc-arch manually.
dnl
dnl Requires macros: AX_CHECK_COMPILER_FLAGS, AX_GCC_X86_CPUID
dnl
dnl (The main emphasis here is on recent CPUs, on the principle that
dnl doing high-performance computing on old hardware is uncommon.)
dnl
dnl @category Misc
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Matteo Frigo.
dnl @version 2006-01-04
dnl @license GPLWithACException

AC_DEFUN([AX_GCC_ARCHFLAG],
[AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_CANONICAL_HOST])

AC_ARG_WITH(gcc-arch, [AC_HELP_STRING([--with-gcc-arch=<arch>], [use architecture <arch> for gcc -march/-mtune, instead of guessing])],
	ax_gcc_arch=$withval, ax_gcc_arch=yes)

AC_MSG_CHECKING([for gcc architecture flag])
AC_MSG_RESULT([])
AC_CACHE_VAL(ax_cv_gcc_archflag,
[
ax_cv_gcc_archflag="unknown"

if test "$GCC" = yes; then

if test "x$ax_gcc_arch" = xyes; then
ax_gcc_arch=""
if test "$cross_compiling" = no; then
case $host_cpu in
  i[[3456]]86*|x86_64*) # use cpuid codes, in part from x86info-1.7 by D. Jones
     AX_GCC_X86_CPUID(0)
     AX_GCC_X86_CPUID(1)
     case $ax_cv_gcc_x86_cpuid_0 in
       *:756e6547:*:*) # Intel
          case $ax_cv_gcc_x86_cpuid_1 in
	    *5[[48]]?:*:*:*) ax_gcc_arch="pentium-mmx pentium" ;;
	    *5??:*:*:*) ax_gcc_arch=pentium ;;
	    *6[[3456]]?:*:*:*) ax_gcc_arch="pentium2 pentiumpro" ;;
	    *6a?:*[[01]]:*:*) ax_gcc_arch="pentium2 pentiumpro" ;;
	    *6a?:*[[234]]:*:*) ax_gcc_arch="pentium3 pentiumpro" ;;
	    *6[[9d]]?:*:*:*) ax_gcc_arch="pentium-m pentium3 pentiumpro" ;;
	    *6[[78b]]?:*:*:*) ax_gcc_arch="pentium3 pentiumpro" ;;
	    *6??:*:*:*) ax_gcc_arch=pentiumpro ;;
            *f3[[347]]:*:*:*|*f4[1347]:*:*:*)
		case $host_cpu in
                  x86_64*) ax_gcc_arch="nocona pentium4 pentiumpro" ;;
                  *) ax_gcc_arch="prescott pentium4 pentiumpro" ;;
                esac ;;
            *f??:*:*:*) ax_gcc_arch="pentium4 pentiumpro";;
          esac ;;
       *:68747541:*:*) # AMD
          case $ax_cv_gcc_x86_cpuid_1 in
	    *5[[67]]?:*:*:*) ax_gcc_arch=k6 ;;
	    *5[[8d]]?:*:*:*) ax_gcc_arch="k6-2 k6" ;;
	    *5[[9]]?:*:*:*) ax_gcc_arch="k6-3 k6" ;;
	    *60?:*:*:*) ax_gcc_arch=k7 ;;
	    *6[[12]]?:*:*:*) ax_gcc_arch="athlon k7" ;;
	    *6[[34]]?:*:*:*) ax_gcc_arch="athlon-tbird k7" ;;
	    *67?:*:*:*) ax_gcc_arch="athlon-4 athlon k7" ;;
	    *6[[68a]]?:*:*:*)
	       AX_GCC_X86_CPUID(0x80000006) # L2 cache size
	       case $ax_cv_gcc_x86_cpuid_0x80000006 in
                 *:*:*[[1-9a-f]]??????:*) # (L2 = ecx >> 16) >= 256
			ax_gcc_arch="athlon-xp athlon-4 athlon k7" ;;
                 *) ax_gcc_arch="athlon-4 athlon k7" ;;
	       esac ;;
	    *f[[4cef8b]]?:*:*:*) ax_gcc_arch="athlon64 k8" ;;
	    *f5?:*:*:*) ax_gcc_arch="opteron k8" ;;
	    *f7?:*:*:*) ax_gcc_arch="athlon-fx opteron k8" ;;
	    *f??:*:*:*) ax_gcc_arch="k8" ;;
          esac ;;
	*:746e6543:*:*) # IDT
	   case $ax_cv_gcc_x86_cpuid_1 in
	     *54?:*:*:*) ax_gcc_arch=winchip-c6 ;;
	     *58?:*:*:*) ax_gcc_arch=winchip2 ;;
	     *6[[78]]?:*:*:*) ax_gcc_arch=c3 ;;
	     *69?:*:*:*) ax_gcc_arch="c3-2 c3" ;;
	   esac ;;
     esac
     if test x"$ax_gcc_arch" = x; then # fallback
	case $host_cpu in
	  i586*) ax_gcc_arch=pentium ;;
	  i686*) ax_gcc_arch=pentiumpro ;;
        esac
     fi
     ;;

  sparc*)
     AC_PATH_PROG([PRTDIAG], [prtdiag], [prtdiag], [$PATH:/usr/platform/`uname -i`/sbin/:/usr/platform/`uname -m`/sbin/])
     cputype=`(((grep cpu /proc/cpuinfo | cut -d: -f2) ; ($PRTDIAG -v |grep -i sparc) ; grep -i cpu /var/run/dmesg.boot ) | head -n 1) 2> /dev/null`
     cputype=`echo "$cputype" | tr -d ' -' |tr $as_cr_LETTERS $as_cr_letters`
     case $cputype in
         *ultrasparciv*) ax_gcc_arch="ultrasparc4 ultrasparc3 ultrasparc v9" ;;
         *ultrasparciii*) ax_gcc_arch="ultrasparc3 ultrasparc v9" ;;
         *ultrasparc*) ax_gcc_arch="ultrasparc v9" ;;
         *supersparc*|*tms390z5[[05]]*) ax_gcc_arch="supersparc v8" ;;
         *hypersparc*|*rt62[[056]]*) ax_gcc_arch="hypersparc v8" ;;
         *cypress*) ax_gcc_arch=cypress ;;
     esac ;;

  alphaev5) ax_gcc_arch=ev5 ;;
  alphaev56) ax_gcc_arch=ev56 ;;
  alphapca56) ax_gcc_arch="pca56 ev56" ;;
  alphapca57) ax_gcc_arch="pca57 pca56 ev56" ;;
  alphaev6) ax_gcc_arch=ev6 ;;
  alphaev67) ax_gcc_arch=ev67 ;;
  alphaev68) ax_gcc_arch="ev68 ev67" ;;
  alphaev69) ax_gcc_arch="ev69 ev68 ev67" ;;
  alphaev7) ax_gcc_arch="ev7 ev69 ev68 ev67" ;;
  alphaev79) ax_gcc_arch="ev79 ev7 ev69 ev68 ev67" ;;

  powerpc*)
     cputype=`((grep cpu /proc/cpuinfo | head -n 1 | cut -d: -f2 | cut -d, -f1 | sed 's/ //g') ; /usr/bin/machine ; /bin/machine; grep CPU /var/run/dmesg.boot | head -n 1 | cut -d" " -f2) 2> /dev/null`
     cputype=`echo $cputype | sed -e 's/ppc//g;s/ *//g'`
     case $cputype in
       *750*) ax_gcc_arch="750 G3" ;;
       *740[[0-9]]*) ax_gcc_arch="$cputype 7400 G4" ;;
       *74[[4-5]][[0-9]]*) ax_gcc_arch="$cputype 7450 G4" ;;
       *74[[0-9]][[0-9]]*) ax_gcc_arch="$cputype G4" ;;
       *970*) ax_gcc_arch="970 G5 power4";;
       *POWER4*|*power4*|*gq*) ax_gcc_arch="power4 970";;
       *POWER5*|*power5*|*gr*|*gs*) ax_gcc_arch="power5 power4 970";;
       603ev|8240) ax_gcc_arch="$cputype 603e 603";;
       *) ax_gcc_arch=$cputype ;;
     esac
     ax_gcc_arch="$ax_gcc_arch powerpc"
     ;;
esac
fi # not cross-compiling
fi # guess arch

if test "x$ax_gcc_arch" != x -a "x$ax_gcc_arch" != xno; then
for arch in $ax_gcc_arch; do
  if test "x[]m4_default([$1],yes)" = xyes; then # if we require portable code
    flags="-mtune=$arch"
    # -mcpu=$arch and m$arch generate nonportable code on every arch except
    # x86.  And some other arches (e.g. Alpha) don't accept -mtune.  Grrr.
    case $host_cpu in i*86|x86_64*) flags="$flags -mcpu=$arch -m$arch";; esac
  else
    flags="-march=$arch -mcpu=$arch -m$arch"
  fi
  for flag in $flags; do
    AX_CHECK_COMPILER_FLAGS($flag, [ax_cv_gcc_archflag=$flag; break])
  done
  test "x$ax_cv_gcc_archflag" = xunknown || break
done
fi

fi # $GCC=yes
])
AC_MSG_CHECKING([for gcc architecture flag])
AC_MSG_RESULT($ax_cv_gcc_archflag)
if test "x$ax_cv_gcc_archflag" = xunknown; then
  m4_default([$3],:)
else
  m4_default([$2], [CFLAGS="$CFLAGS $ax_cv_gcc_archflag"])
fi
])

dnl @synopsis AX_GCC_X86_CPUID(OP)
dnl
dnl @summary run x86 cpuid instruction OP using gcc inline assembler
dnl
dnl On Pentium and later x86 processors, with gcc or a compiler that
dnl has a compatible syntax for inline assembly instructions, run a
dnl small program that executes the cpuid instruction with input OP.
dnl This can be used to detect the CPU type.
dnl
dnl On output, the values of the eax, ebx, ecx, and edx registers are
dnl stored as hexadecimal strings as "eax:ebx:ecx:edx" in the cache
dnl variable ax_cv_gcc_x86_cpuid_OP.
dnl
dnl If the cpuid instruction fails (because you are running a
dnl cross-compiler, or because you are not using gcc, or because you
dnl are on a processor that doesn't have this instruction),
dnl ax_cv_gcc_x86_cpuid_OP is set to the string "unknown".
dnl
dnl This macro mainly exists to be used in AX_GCC_ARCHFLAG.
dnl
dnl @category Misc
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Matteo Frigo.
dnl @version 2005-05-30
dnl @license GPLWithACException

AC_DEFUN([AX_GCC_X86_CPUID],
[AC_REQUIRE([AC_PROG_CC])
AC_LANG_PUSH([C])
AC_CACHE_CHECK(for x86 cpuid $1 output, ax_cv_gcc_x86_cpuid_$1,
 [AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <stdio.h>], [
     int op = $1, eax, ebx, ecx, edx;
     FILE *f;
      __asm__("cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (op));
     f = fopen("conftest_cpuid", "w"); if (!f) return 1;
     fprintf(f, "%x:%x:%x:%x\n", eax, ebx, ecx, edx);
     fclose(f);
     return 0;
])],
     [ax_cv_gcc_x86_cpuid_$1=`cat conftest_cpuid`; rm -f conftest_cpuid],
     [ax_cv_gcc_x86_cpuid_$1=unknown; rm -f conftest_cpuid],
     [ax_cv_gcc_x86_cpuid_$1=unknown])])
AC_LANG_POP([C])
])

dnl @synopsis AX_CC_MAXOPT
dnl
dnl @summary turn on optimization flags for the C compiler
dnl
dnl Modified: Returns OPTFLAGS.  Added --enable-optimisation.
dnl
dnl Try to turn on "good" C optimization flags for various compilers
dnl and architectures, for some definition of "good". (In our case,
dnl good for FFTW and hopefully for other scientific codes. Modify as
dnl needed.)
dnl
dnl The user can override the flags by setting the CFLAGS environment
dnl variable. The user can also specify --enable-portable-binary in
dnl order to disable any optimization flags that might result in a
dnl binary that only runs on the host architecture.
dnl
dnl Note also that the flags assume that ANSI C aliasing rules are
dnl followed by the code (e.g. for gcc's -fstrict-aliasing), and that
dnl floating-point computations can be re-ordered as needed.
dnl
dnl Requires macros: AX_CHECK_COMPILER_FLAGS, AX_COMPILER_VENDOR,
dnl AX_GCC_ARCHFLAG, AX_GCC_X86_CPUID.
dnl
dnl @category C
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Matteo Frigo.
dnl @version 2005-05-30
dnl @license GPLWithACException

AC_DEFUN([AX_CC_MAXOPT],
[
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AX_COMPILER_VENDOR])
AC_REQUIRE([AC_CANONICAL_HOST])

AC_ARG_ENABLE(optimisation, [AC_HELP_STRING([--enable-optimisation], [enable automatic optimisation guesses])], acx_maxopt_enable=$withval, acx_maxopt_enable=no)
AC_ARG_ENABLE(portable-binary, [AC_HELP_STRING([--enable-portable-binary], [disable compiler optimizations that would produce unportable binaries])],
	acx_maxopt_portable=$withval, acx_maxopt_portable=no)

# Try to determine "good" native compiler flags if none specified via CFLAGS
if test "$acx_maxopt_enable" != "no"; then
  OLD_CFLAGS="$CFLAGS"
  CFLAGS=""
  case $ax_cv_c_compiler_vendor in
    dec) CFLAGS="-newc -w0 -O5 -ansi_alias -ansi_args -fp_reorder -tune host"
	 if test "x$acx_maxopt_portable" = xno; then
           CFLAGS="$CFLAGS -arch host"
         fi;;

    sun) CFLAGS="-native -fast -xO5 -dalign"
	 if test "x$acx_maxopt_portable" = xyes; then
	   CFLAGS="$CFLAGS -xarch=generic"
         fi;;

    hp)  CFLAGS="+Oall +Optrs_ansi +DSnative"
	 if test "x$acx_maxopt_portable" = xyes; then
	   CFLAGS="$CFLAGS +DAportable"
	 fi;;

    ibm) if test "x$acx_maxopt_portable" = xno; then
           xlc_opt="-qarch=auto -qtune=auto"
	 else
           xlc_opt="-qtune=auto"
	 fi
         AX_CHECK_COMPILER_FLAGS($xlc_opt,
         	CFLAGS="-O3 -qansialias -w $xlc_opt",
               [CFLAGS="-O3 -qansialias -w"
                echo "******************************************************"
                echo "*  You seem to have the IBM  C compiler.  It is      *"
                echo "*  recommended for best performance that you use:    *"
                echo "*                                                    *"
                echo "*    CFLAGS=-O3 -qarch=xxx -qtune=xxx -qansialias -w *"
                echo "*                      ^^^        ^^^                *"
                echo "*  where xxx is pwr2, pwr3, 604, or whatever kind of *"
                echo "*  CPU you have.  (Set the CFLAGS environment var.   *"
                echo "*  and re-run configure.)  For more info, man cc.    *"
                echo "******************************************************"])
         ;;

    intel) CFLAGS="-O3 -ansi_alias"
	if test "x$acx_maxopt_portable" = xno; then
	  icc_archflag=unknown
	  icc_flags=""
	  case $host_cpu in
	    i686*|x86_64*)
              # icc accepts gcc assembly syntax, so these should work:
	      AX_GCC_X86_CPUID(0)
              AX_GCC_X86_CPUID(1)
	      case $ax_cv_gcc_x86_cpuid_0 in # see AX_GCC_ARCHFLAG
                *:756e6547:*:*) # Intel
                  case $ax_cv_gcc_x86_cpuid_1 in
                    *6a?:*[[234]]:*:*|*6[[789b]]?:*:*:*) icc_flags="-xK";;
                    *f3[[347]]:*:*:*|*f4[1347]:*:*:*) icc_flags="-xP -xN -xW -xK";;
                    *f??:*:*:*) icc_flags="-xN -xW -xK";;
                  esac ;;
              esac ;;
          esac
          if test "x$icc_flags" != x; then
            for flag in $icc_flags; do
              AX_CHECK_COMPILER_FLAGS($flag, [icc_archflag=$flag; break])
            done
          fi
          AC_MSG_CHECKING([for icc architecture flag])
	  AC_MSG_RESULT($icc_archflag)
          if test "x$icc_archflag" != xunknown; then
            CFLAGS="$CFLAGS $icc_archflag"
          fi
        fi
	;;

    gnu)
     # default optimization flags for gcc on all systems
     CFLAGS="-O3 -fomit-frame-pointer"

     # -malign-double for x86 systems
     AX_CHECK_COMPILER_FLAGS(-malign-double, CFLAGS="$CFLAGS -malign-double")

     #  -fstrict-aliasing for gcc-2.95+
     AX_CHECK_COMPILER_FLAGS(-fstrict-aliasing,
	CFLAGS="$CFLAGS -fstrict-aliasing")

     # note that we enable "unsafe" fp optimization with other compilers, too
     AX_CHECK_COMPILER_FLAGS(-ffast-math, CFLAGS="$CFLAGS -ffast-math")

     AX_GCC_ARCHFLAG($acx_maxopt_portable)
     ;;
  esac

  if test -z "$CFLAGS"; then
	echo ""
	echo "********************************************************"
        echo "* WARNING: Don't know the best CFLAGS for this system  *"
        echo "* Use ./configure CFLAGS=... to specify your own flags *"
	echo "* (otherwise, a default of CFLAGS=-O3 will be used)    *"
	echo "********************************************************"
	echo ""
        CFLAGS="-O3"
  fi

  AX_CHECK_COMPILER_FLAGS($CFLAGS, [], [
	echo ""
        echo "********************************************************"
        echo "* WARNING: The guessed CFLAGS don't seem to work with  *"
        echo "* your compiler.                                       *"
        echo "* Use ./configure CFLAGS=... to specify your own flags *"
        echo "********************************************************"
        echo ""
        CFLAGS=""
  ])

  OPTFLAGS="$CFLAGS"
  CFLAGS="$OLD_CFLAGS"
else
  OPTFLAGS=""
fi
])

