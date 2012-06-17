#!/bin/bash

#set -xv
set -e

#
# This script is to generate lib user library as a whole. It will leave
# two files on current directory: libuser.a and libuser.so.
#
# Most concern here is the libraries linking order
#
# FIXME: How to do this cleanly use makefile?
#

AR=/usr/bin/ar
# see http://osdir.com/ml/gmane.comp.gnu.binutils.bugs/2006-01/msg00016.php
ppc64_CPU=`uname -p`
if [ ${ppc64_CPU} == "ppc64" ]; then
  LD="gcc -m64"
else
  LD="gcc"
fi
RANLIB=/usr/bin/ranlib

CWD=`pwd`

# do cleanup at first
rm -f libuser.so

ALL_OBJS=

build_obj_list() {
  _objs=`$AR -t $1/$2`
  for _lib in $_objs; do
    ALL_OBJS=$ALL_OBJS"$1/$_lib ";
  done;
}

# user components libs
build_obj_list . libuser_local.a
build_obj_list ../mtfs libmtfs.a

# create static lib user
rm -f $CWD/libuser.a
$AR -cru $CWD/libuser.a $ALL_OBJS
$RANLIB $CWD/libuser.a

# create shared lib user
rm -f $CWD/libuser.so
OS=`uname`
$LD -shared -nostdlib -o $CWD/libuser.so $ALL_OBJS $CAP_LIBS $PTHREAD_LIBS $ZLIB

rm -rf $sysio_tmp
