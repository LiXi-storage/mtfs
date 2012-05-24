#* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
#* vim:expandtab:shiftwidth=8:tabstop=8:
#
# LC_CONFIG_SRCDIR
#
# Wrapper for AC_CONFIG_SUBDIR
#
AC_DEFUN([LC_CONFIG_SRCDIR],
[AC_CONFIG_SRCDIR([mtfs/mtfs/file.c])
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
# mtfs specific paths
#
AC_DEFUN([LC_PATH_DEFAULTS],
[HRFS="$PWD/mtfs"
AC_SUBST(HRFS)

# mount.mtfs
rootsbindir='/sbin'
AC_SUBST(rootsbindir)

])

#
# HRFS_TAG is temporary, until backend fs check is in place
#
AC_DEFUN([LC_CONFIG_HRFS_TAG],
[AC_MSG_CHECKING([whether to enable HRFS_TAG])
AC_ARG_ENABLE([mtfs_tag],
        AC_HELP_STRING([--disable-libcfs-cdebug],
                        [disable HRFS_TAG]),
        [],[enable_mtfs_tag='yes'])
AC_MSG_RESULT([$enable_mtfs_tag])
if test x$enable_mtfs_tag = xyes; then
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
# LC_CONFIG_BACKEDN_LUSTRE
#
# whether to build lustre backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_LUSTRE],
[AC_MSG_CHECKING([whether to build lustre backend support])
AC_ARG_ENABLE([lustre-support],
        AC_HELP_STRING([--disable-lustre-support],
                        [disable lustre backend support]),
        [],[enable_lustre_support='yes'])
AC_MSG_RESULT([$enable_lustre_support])])

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
# whether to build libmtfs
#
AC_DEFUN([LC_CONFIG_LIBHRFS],
[AC_MSG_CHECKING([whether to build Hrfs library])
AC_ARG_ENABLE([libmtfs],
        AC_HELP_STRING([--disable-libmtfs],
                        [disable building of mtfs library]),
        [],[enable_libmtfs='yes'])
AC_MSG_RESULT([$enable_libmtfs])

AC_MSG_CHECKING([whether to build libmtfs tests])
AC_ARG_ENABLE([libmtfs-tests],
        AC_HELP_STRING([--enable-libmtfs-tests],
                        [enable libmtfs tests, if --disable-tests is used]),
        [],[enable_libmtfs_tests=$enable_tests])
if test x$enable_libmtfs != xyes ; then
   enable_libmtfs_tests='no'
fi
AC_MSG_RESULT([$enable_libmtfs_tests])
])

#
#
# LC_CONFIG_LUSTRE_PATH
#
# Find paths for lustre
#
AC_DEFUN([LC_CONFIG_LUSTRE_PATH],
[AC_MSG_CHECKING([for Lustre sources])
AC_ARG_WITH([lustre],
        AC_HELP_STRING([--with-lustre=path],
                       [set path to lustre source (default=/usr/src/lustre)]),
        [LB_ARG_CANON_PATH([lustre], [LUSTRE])],
        [LUSTRE=/usr/src/lustre])
AC_MSG_RESULT([$LUSTRE])
AC_SUBST(LUSTRE)

# -------- check for lustre --------
LB_CHECK_FILE([$LUSTRE],[],
        [AC_MSG_ERROR([Lustre source $LUSTRE could not be found.])])

# -------- check for version --------
LB_CHECK_FILE([$LUSTRE/lustre/include/mtfs_version.h], [],[
        AC_MSG_WARN([Unpatched lustre source code detected.])
        AC_MSG_WARN([Lustre backend support cannot be built with an unpatched lustre source code;])
        AC_MSG_WARN([disabling lustre support])
        enable_lustre_support='no'
])

# -------- check for head directory --------
LB_CHECK_FILE([$LUSTRE/lustre/include],[],
	[AC_MSG_ERROR([Lustre head directory $LUSTRE/lustre/include could not be found.])])
	
# -------- define lustre dir --------
	LUSTRE_INCLUDE_DIR="$LUSTRE/lustre/include"
	#CPPFLAGS="$CPPFLAGS -I$LUSTRE_INCLUDE_DIR"
	EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$LUSTRE_INCLUDE_DIR"

]) # end of LC_CONFIG_LUSTRE_PATH


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

if test x$enable_lustre_support = xyes ; then
	LC_CONFIG_LUSTRE_PATH
fi

LC_CONFIG_HRFS_TAG
LC_CONFIG_READLINE
LC_CONFIG_LUA

CPPFLAGS="-I$PWD/mtfs/include $CPPFLAGS"
EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$PWD/mtfs/include"
])

#
# LC_CONDITIONALS
#
# AM_CONDITIONALS for mtfs
#
AC_DEFUN([LC_CONDITIONALS],
[
AM_CONDITIONAL(LIBHRFS, test x$enable_libmtfs = xyes)
AM_CONDITIONAL(LIBHRFS_TESTS, test x$enable_libmtfs_tests = xyes)
AM_CONDITIONAL(LUSTRE_SUPPORT, test x$enable_lustre_support = xyes)
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
mtfs/Makefile
mtfs/autoMakefile
mtfs/autoconf/Makefile
mtfs/doc/Makefile
mtfs/include/Makefile
mtfs/mtfs/Makefile
mtfs/mtfs/autoMakefile
mtfs/libuser/Makefile
mtfs/libuser/tests/Makefile
mtfs/manage/Makefile
mtfs/scripts/Makefile
mtfs/tests/Makefile
mtfs/tests/pjd_fstest/Makefile
mtfs/tests/src/Makefile
mtfs/utils/Makefile
mtfs/ext2_support/Makefile
mtfs/ext2_support/autoMakefile
mtfs/ext3_support/Makefile
mtfs/ext3_support/autoMakefile
mtfs/ext4_support/Makefile
mtfs/ext4_support/autoMakefile
mtfs/nfs_support/Makefile
mtfs/nfs_support/autoMakefile
mtfs/ntfs3g_support/Makefile
mtfs/ntfs3g_support/autoMakefile
mtfs/lustre_support/Makefile
mtfs/lustre_support/autoMakefile
mtfs/tmpfs_support/Makefile
mtfs/tmpfs_support/autoMakefile
])
])
