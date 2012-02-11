#* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
#* vim:expandtab:shiftwidth=8:tabstop=8:
#
# LC_CONFIG_SRCDIR
#
# Wrapper for AC_CONFIG_SUBDIR
#
AC_DEFUN([LC_CONFIG_SRCDIR],
[AC_CONFIG_SRCDIR([hrfs/random/main.c])
libcfs_is_module=yes
])

#
# LC_TARGET_SUPPORTED
#
# is the target os supported?
#
AC_DEFUN([LC_TARGET_SUPPORTED],
[case $target_os in
        linux*)
$1
                ;;
        *)
$2
                ;;
esac
])

#
# LC_PATH_DEFAULTS
#
# hrfs specific paths
#
AC_DEFUN([LC_PATH_DEFAULTS],
[HRFS="$PWD/hrfs"
AC_SUBST(HRFS)

# mount.hrfs
rootsbindir='/sbin'
AC_SUBST(rootsbindir)

])

#
# HRFS_TAG is temporary, until backend fs check is in place
#
AC_DEFUN([LC_CONFIG_HRFS_TAG],
[AC_MSG_CHECKING([whether to enable HRFS_TAG])
AC_ARG_ENABLE([hrfs_tag],
        AC_HELP_STRING([--disable-libcfs-cdebug],
                        [disable HRFS_TAG]),
        [],[enable_hrfs_tag='yes'])
AC_MSG_RESULT([$enable_hrfs_tag])
if test x$enable_hrfs_tag = xyes; then
        AC_DEFINE(HRFS_TAG, 1, [enable HRFS_TAG])
fi
])

#
# LC_PROG_LINUX
#
# Hrfs linux kernel checks
#
AC_DEFUN([LC_PROG_LINUX],
[
])

#
# LC_CONFIG_BACKEDN_SWGFS
#
# whether to build swgfs backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_SWGFS],
[AC_MSG_CHECKING([whether to build swgfs backend support])
AC_ARG_ENABLE([swgfs-support],
        AC_HELP_STRING([--disable-swgfs-support],
                        [disable swgfs backend support]),
        [],[enable_swgfs_support='yes'])
AC_MSG_RESULT([$enable_swgfs_support])])

#
# LC_CONFIG_BACKEDN_EXT2
#
# whether to build ext2 backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_EXT2],
[AC_MSG_CHECKING([whether to build ext2 backend support])
AC_ARG_ENABLE([ext2-support],
        AC_HELP_STRING([--disable-ext2-support],
                        [disable ext2 backend support]),
        [],[enable_ext2_support='yes'])
AC_MSG_RESULT([$enable_ext2_support])])

#
# LC_CONFIG_BACKEDN_EXT3
#
# whether to build ext3 backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_EXT3],
[AC_MSG_CHECKING([whether to build ext3 backend support])
AC_ARG_ENABLE([ext3-support],
        AC_HELP_STRING([--disable-ext3-support],
                        [disable ext3 backend support]),
        [],[enable_ext3_support='yes'])
AC_MSG_RESULT([$enable_ext3_support])])

#
# LC_CONFIG_BACKEDN_EXT4
#
# whether to build ext4 backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_EXT4],
[AC_MSG_CHECKING([whether to build ext4 backend support])
AC_ARG_ENABLE([ext4-support],
        AC_HELP_STRING([--disable-ext4-support],
                        [disable ext4 backend support]),
        [],[enable_ext4_support='yes'])
AC_MSG_RESULT([$enable_ext4_support])])

#
# LC_CONFIG_BACKEDN_NFS
#
# whether to build nfs backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_NFS],
[AC_MSG_CHECKING([whether to build nfs backend support])
AC_ARG_ENABLE([nfs-support],
        AC_HELP_STRING([--disable-nfs-support],
                        [disable nfs backend support]),
        [],[enable_nfs_support='yes'])
AC_MSG_RESULT([$enable_nfs_support])])

#
# LC_CONFIG_BACKEDN_TMPFS
#
# whether to build tmpfs backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_TMPFS],
[AC_MSG_CHECKING([whether to build tmpfs backend support])
AC_ARG_ENABLE([tmpfs-support],
        AC_HELP_STRING([--disable-tmpfs-support],
                        [disable tmpfs backend support]),
        [],[enable_tmpfs_support='yes'])
AC_MSG_RESULT([$enable_tmpfs_support])])

#
# LC_CONFIG_BACKEDN_NTFS3G
#
# whether to build ntfs3g backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_NTFS3G],
[AC_MSG_CHECKING([whether to build ntfs3g backend support])
AC_ARG_ENABLE([ntfs3g-support],
        AC_HELP_STRING([--disable-ntfs3g-support],
                        [disable ntfs3g backend support]),
        [],[enable_ntfs3g_support='yes'])
AC_MSG_RESULT([$enable_ntfs3g_support])])

#
# LC_CONFIG_MANAGE
#
# whether to build manage support
#
AC_DEFUN([LC_CONFIG_MANAGE],
[AC_MSG_CHECKING([whether to build manage support])
AC_ARG_ENABLE([manage],
        AC_HELP_STRING([--disable-manage],
                        [disable manage support]),
        [],[enable_manage='yes'])
AC_MSG_RESULT([$enable_manage])

AC_MSG_CHECKING([whether to build manage tests])
AC_ARG_ENABLE([manage-tests],
        AC_HELP_STRING([--enable-manage-tests],
                        [enable manage tests, if --disable-tests is used]),
        [],[enable_manage_tests=$enable_tests])
if test x$enable_manage != xyes ; then
        enable_manage_tests='no'
fi
AC_MSG_RESULT([$enable_manage_tests])
])

#
# LC_CONFIG_LIBHRFS
#
# whether to build libhrfs
#
AC_DEFUN([LC_CONFIG_LIBHRFS],
[AC_MSG_CHECKING([whether to build Hrfs library])
AC_ARG_ENABLE([libhrfs],
        AC_HELP_STRING([--disable-libhrfs],
                        [disable building of hrfs library]),
        [],[enable_libhrfs='yes'])
AC_MSG_RESULT([$enable_libhrfs])

AC_MSG_CHECKING([whether to build libhrfs tests])
AC_ARG_ENABLE([libhrfs-tests],
        AC_HELP_STRING([--enable-libhrfs-tests],
                        [enable libhrfs tests, if --disable-tests is used]),
        [],[enable_libhrfs_tests=$enable_tests])
if test x$enable_libhrfs != xyes ; then
   enable_libhrfs_tests='no'
fi
AC_MSG_RESULT([$enable_libhrfs_tests])
])

#
#
# LC_CONFIG_SWGFS_PATH
#
# Find paths for swgfs
#
AC_DEFUN([LC_CONFIG_SWGFS_PATH],
[AC_MSG_CHECKING([for Swgfs sources])
AC_ARG_WITH([swgfs],
        AC_HELP_STRING([--with-swgfs=path],
                       [set path to Swgfs source (default=/usr/src/swgfs)]),
        [LB_ARG_CANON_PATH([swgfs], [SWGFS])],
        [SWGFS=/usr/src/swgfs])
AC_MSG_RESULT([$SWGFS])
AC_SUBST(SWGFS)

# -------- check for swgfs --------
LB_CHECK_FILE([$SWGFS],[],
        [AC_MSG_ERROR([Swgfs source $SWGFS could not be found.])])

# -------- check for version --------
LB_CHECK_FILE([$SWGFS/swgfs/include/hrfs_version.h], [],[
        AC_MSG_WARN([Unpatched swgfs source code detected.])
        AC_MSG_WARN([Swgfs backend support cannot be built with an unpatched swgfs source code;])
        AC_MSG_WARN([disabling swgfs support])
        enable_swgfs_support='no'
])

# -------- check for head directory --------
LB_CHECK_FILE([$SWGFS/swgfs/include],[],
	[AC_MSG_ERROR([Swgfs head directory $SWGFS/swgfs/include could not be found.])])
	
# -------- define swgfs dir --------
	SWGFS_INCLUDE_DIR="$SWGFS/swgfs/include"
	#CPPFLAGS="$CPPFLAGS -I$SWGFS_INCLUDE_DIR"
	EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$SWGFS_INCLUDE_DIR"

]) # end of LC_CONFIG_SWGFS_PATH

#
# LC_CONFIG_READLINE
#
# Build with readline
#
AC_DEFUN([LC_CONFIG_READLINE],
[
AC_MSG_CHECKING([whether to enable readline support])
AC_ARG_ENABLE(readline,
        AC_HELP_STRING([--disable-readline],
                        [disable readline support]),
        [],[enable_readline='yes'])
AC_MSG_RESULT([$enable_readline])

# -------- check for readline if enabled ----
if test x$enable_readline = xyes ; then
	LIBS_save="$LIBS"
	LIBS="-lncurses $LIBS"
	AC_CHECK_LIB([readline],[readline],[
	LIBREADLINE="-lreadline -lncurses"
	AC_DEFINE(HAVE_LIBREADLINE, 1, [readline library is available])
	],[
	LIBREADLINE=""
	])
	LIBS="$LIBS_save"
else
	LIBREADLINE=""
fi
AC_SUBST(LIBREADLINE)
]) # end of LC_CONFIG_READLINE

#
# LC_CONFIG_LUA
#
# Build with lua
#
AC_DEFUN([LC_CONFIG_LUA],
[
if test x$enable_manage = xyes ; then
	LIBS_save="$LIBS"
	LIBS="-ldl -lm $LIBS"
	AC_CHECK_LIB([lua],[luaL_newstate],[
	LIBLUA="-llua -ldl -lm"
	AC_DEFINE(HAVE_LIBLUA, 1, [lua library is available])
	],[
	AC_MSG_WARN([Lua library is not found.])
	AC_MSG_WARN([Hfsm cannot be built without Lua library.])
	AC_MSG_WARN([Disabling hfsm.])
	enable_manage="no"
	])
	LIBS="$LIBS_save"
	AC_SUBST(LIBREADLINE)
else
	LIBLUA=""
fi
AC_SUBST(LIBLUA)
]) # end of LC_CONFIG_LUA

#
# LC_CONFIGURE
#
# other configure checks
#
AC_DEFUN([LC_CONFIGURE],
[
if test $target_cpu == "i686" -o $target_cpu == "x86_64"; then
        CFLAGS="$CFLAGS -Werror"
fi

if test x$enable_swgfs_support = xyes ; then
	LC_CONFIG_SWGFS_PATH
fi

LC_CONFIG_HRFS_TAG
LC_CONFIG_READLINE
LC_CONFIG_LUA

CPPFLAGS="-I$PWD/hrfs/include $CPPFLAGS"
EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$PWD/hrfs/include"
])

#
# LC_CONDITIONALS
#
# AM_CONDITIONALS for hrfs
#
AC_DEFUN([LC_CONDITIONALS],
[
AM_CONDITIONAL(LIBHRFS, test x$enable_libhrfs = xyes)
AM_CONDITIONAL(LIBHRFS_TESTS, test x$enable_libhrfs_tests = xyes)
AM_CONDITIONAL(SWGFS_SUPPORT, test x$enable_swgfs_support = xyes)
AM_CONDITIONAL(EXT2_SUPPORT, test x$enable_ext2_support = xyes)
AM_CONDITIONAL(EXT3_SUPPORT, test x$enable_ext3_support = xyes)
AM_CONDITIONAL(EXT4_SUPPORT, test x$enable_ext4_support = xyes)
AM_CONDITIONAL(NFS_SUPPORT, test x$enable_nfs_support = xyes)
AM_CONDITIONAL(TMPFS_SUPPORT, test x$enable_tmpfs_support = xyes)
AM_CONDITIONAL(NTFS3G_SUPPORT, test x$enable_ntfs3g_support = xyes)
AM_CONDITIONAL(MANAGE, test x$enable_manage = xyes)
AM_CONDITIONAL(MANAGE_TESTS, test x$enable_manage_tests = xyes)
])

#
# LC_CONFIG_FILES
#
# files that should be generated with AC_OUTPUT
#
AC_DEFUN([LC_CONFIG_FILES],
[AC_CONFIG_FILES([
hrfs/Makefile
hrfs/autoMakefile
hrfs/autoconf/Makefile
hrfs/doc/Makefile
hrfs/include/Makefile
hrfs/random/Makefile
hrfs/random/autoMakefile
hrfs/hrfs/Makefile
hrfs/hrfs/autoMakefile
hrfs/libuser/Makefile
hrfs/libuser/tests/Makefile
hrfs/manage/Makefile
hrfs/scripts/Makefile
hrfs/tests/Makefile
hrfs/tests/pjd_fstest/Makefile
hrfs/tests/src/Makefile
hrfs/utils/libcoreutils/Makefile
hrfs/utils/Makefile
hrfs/ext2_support/Makefile
hrfs/ext2_support/autoMakefile
hrfs/ext3_support/Makefile
hrfs/ext3_support/autoMakefile
hrfs/ext4_support/Makefile
hrfs/ext4_support/autoMakefile
hrfs/nfs_support/Makefile
hrfs/nfs_support/autoMakefile
hrfs/ntfs3g_support/Makefile
hrfs/ntfs3g_support/autoMakefile
hrfs/swgfs_support/Makefile
hrfs/swgfs_support/autoMakefile
hrfs/tmpfs_support/Makefile
hrfs/tmpfs_support/autoMakefile
])
])
