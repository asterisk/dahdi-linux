# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
 
m4_define([TOOLSVERSION],
          m4_bpatsubst(m4_esyscmd([build_tools/make_version . dahdi/tools]),
                       [\([0-9.]*\)\(\w\|\W\)*],
                       [\1]))
AC_INIT(dahdi, TOOLSVERSION, www.asterisk.org)

# check existence of the package
AC_CONFIG_SRCDIR([dahdi_cfg.c])

AC_COPYRIGHT("dahdi-tools")
AC_REVISION($Revision$)

ac_default_prefix=/usr
if test ${sysconfdir} = '${prefix}/etc'; then
   sysconfdir=/etc
fi
if test ${mandir} = '${prefix}/man'; then
   mandir=/usr/share/man
fi

if test ${localstatedir} = '${prefix}/var'; then
     localstatedir=/var
fi

# specify output header file
AC_CONFIG_HEADER(autoconfig.h)

# This needs to be before any macros that use the C compiler
AC_GNU_SOURCE

AC_CHECK_HEADERS([sys/soundcard.h linux/soundcard.h])

AC_CHECK_TOOL([LD], [ld])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AST_CHECK_GNU_MAKE

test_obj=conftest.o
AC_COMPILE_IFELSE(AC_LANG_SOURCE(),[
	BDFNAME=`LANG=C objdump -f $test_obj | grep -e "$test_obj:" | sed "s/.*file format \(.*\)/\1/"`
	BDFARCH=`LANG=C objdump -f $test_obj | grep -e "architecture:" | sed "s/.*ture: \(.*\),.*/\1/"`	   
],[])
AC_SUBST(BDFNAME)
AC_SUBST(BDFARCH)

# Set the default value of HOSTCC from CC if --host was not provided:
HOSTCC=${HOSTCC:=${CC}}
AC_SUBST(HOSTCC)

AC_PATH_PROG([GREP], [grep], :)
AC_PATH_PROG([SHELL], [sh], :)
AC_PATH_PROG([LN], [ln], :)

AC_PATH_PROG([WGET], [wget], :)
if test "${WGET}" != ":" ; then
  DOWNLOAD=${WGET}
else
  AC_PATH_PROG([FETCH], [fetch], [:])
  DOWNLOAD=${FETCH}
fi
AC_SUBST(DOWNLOAD)

AC_LANG(C)

AC_ARG_ENABLE(dev-mode,
	[  --enable-dev-mode    Turn on developer mode],
	[case "${enableval}" in
	      y|ye|yes) DAHDI_DEVMODE=yes ;;
	      n|no)  DAHDI_DEVMODE=no ;;
	      *) AC_MSG_ERROR(bad value ${enableval} for --enable-dev-mode)  ;;
	esac])
AC_SUBST(DAHDI_DEVMODE)

AC_MSG_CHECKING(for -Wdeclaration-after-statement support)
if $(${CC} -Wdeclaration-after-statement -S -o /dev/null -xc /dev/null > /dev/null 2>&1); then
	AC_MSG_RESULT(yes)
	DAHDI_DECLARATION_AFTER_STATEMENT=-Wdeclaration-after-statement
else
	AC_MSG_RESULT(no)
	DAHDI_DECLARATION_AFTER_STATEMENT=
fi
AC_SUBST(DAHDI_DECLARATION_AFTER_STATEMENT)

AST_EXT_LIB_SETUP([DAHDI], [DAHDI], [dahdi])
AST_EXT_LIB_SETUP([NEWT], [newt], [newt])
AST_EXT_LIB_SETUP([USB], [usb], [usb])

AST_C_DEFINE_CHECK([DAHDI], [DAHDI_CODE], [dahdi/user.h])
DAHDI23_DIR="${DAHDI_DIR}"
AST_C_DEFINE_CHECK([DAHDI23], [DAHDI_CONFIG_NTTE], [dahdi/user.h])
AST_EXT_LIB_CHECK([NEWT], [newt], [newtBell], [newt.h])
AST_EXT_LIB_CHECK([USB], [usb], [usb_init], [usb.h])

AC_CHECK_FUNCS([semtimedop])

PBX_HDLC=0
AC_MSG_CHECKING([for GENERIC_HDLC_VERSION version 4 in linux/hdlc.h])
AC_COMPILE_IFELSE(
    [ AC_LANG_PROGRAM( [#include <linux/hdlc.h>],
	    [#if defined(GENERIC_HDLC_VERSION) && GENERIC_HDLC_VERSION >= 4
		int foo = 0;
		#else
		int foo = bar;
		#endif
		0])],
	[AC_MSG_RESULT(yes)
	 PBX_HDLC=1],
	[AC_MSG_RESULT(no)]
)
if test $PBX_HDLC = 0; then
    AC_MSG_CHECKING([for GENERIC_HDLC_VERSION version 4 in linux/hdlc/ioctl.h])
    AC_COMPILE_IFELSE(
	    [ AC_LANG_PROGRAM( [
		    #include <sys/socket.h>
			#include <linux/if.h>],
			[#if defined(GENERIC_HDLC_VERSION) && GENERIC_HDLC_VERSION >= 4
			int foo = 0;
			#else
			int foo = bar;
			#endif
			0])],
		[AC_MSG_RESULT(yes)
		 PBX_HDLC=1],
		[AC_MSG_RESULT(no)]
	)
fi

if test "x${PBX_HDLC}" != "x1"; then
   AC_MSG_NOTICE([GENERIC_HDLC_VERSION (version 4) not found, disabling sethdlc.])
fi

AC_SUBST(PBX_HDLC)

AC_ARG_WITH(selinux,
	[AS_HELP_STRING([--with-selinux],
			[enable (with) / disable (without) SELinux])],
	[USE_SELINUX=$withval],
	[ if test ! -x /usr/sbin/sestatus; then 
	   	USE_SELINUX=no;
	  elif /usr/sbin/sestatus | grep "SELinux status:" | grep -q "enabled"; then
		USE_SELINUX=yes
	  fi
	]
)


AC_SUBST(USE_SELINUX)

# for asciidoc before ver. 7, the backend must be stated explicitly:
ASCIIDOC='asciidoc'
asciidoc_ver=`asciidoc --version 2>&1 | awk '/^asciidoc /{print $2}' | cut -d. -f 1 | head -n 1`
if test "$asciidoc_ver" != '' && test $asciidoc_ver -lt 7; then
	ASCIIDOC="asciidoc -b xhtml"
fi
AC_SUBST(ASCIIDOC)

AC_ARG_WITH(ppp,
	    [AS_HELP_STRING([--with-ppp=PATH],[Use PPP support from PATH])],
	    [],
	    [with_ppp=check]
	    )
# somebody will fix that
default_ppp_path=/usr

case "$with_ppp" in
	yes|check) ppp_path="$default_ppp_path";;
	no) ppp_path='' ;;
	*) ppp_path="$with_ppp" ;;
esac

level_file="$ppp_path/include/pppd/patchlevel.h"
PPP_VERSION=
if test "$ppp_path" != '' && test -r "$level_file"; then
	PPPD_VERSION=`awk -F '"' '/VERSION/ { print $$2; }' $level_file`
fi

case "$with_ppp" in 
	check|no) :;;
	*) 
	# If we asked explicitly for ppp support
	if test "$PPPD_VERSION" = ''; then
		# but have not detected it
		AC_MSG_ERROR(failed to find pppd/patchlevel.h: no ppp support.)
	fi
	;;
esac

if test "x${PBX_DAHDI}" != "x1"; then
   AC_MSG_NOTICE([***])
   AC_MSG_NOTICE([*** Building this package requires DAHDI support. *** ])
   AC_MSG_NOTICE([*** Please install the dahdi-linux package. ***])
   AC_MSG_NOTICE([***])
   exit 1
fi

if test "x${PBX_DAHDI23}" != "x1"; then
   AC_MSG_NOTICE([***])
   AC_MSG_NOTICE([*** Building this package requires DAHDI support (>= 2.3) *** ])
   AC_MSG_NOTICE([*** Please install a recent dahdi-linux package. ***])
   AC_MSG_NOTICE([***])
   exit 1
fi

AC_SUBST(PPPD_VERSION)

AC_CONFIG_FILES([makeopts])
AC_OUTPUT

AC_MSG_NOTICE(*** dahdi-tools build successfully configured ***)
