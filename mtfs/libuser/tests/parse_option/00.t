#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="tests for example"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

#
# TEST FORMAT:
# INPUT:
# mount_option
#
# OUTPUT:
# mount_option struct dumped
#


# test 1
IN="
device=/mnt/mtfs1:/mnt/mtfs2:/mnt/mtfs3:/mnt/mtfs4:/mnt/mtfs5:/mnt/mtfs6
" OUT="
bnum = 6
/mnt/mtfs1:/mnt/mtfs2:/mnt/mtfs3:/mnt/mtfs4:/mnt/mtfs5:/mnt/mtfs6
subject = (null)
" expect 0

#
# test 2
# Colon at beginning or end is allowed
#
IN="
device=:/mnt/mtfs1:/mnt/mtfs2:
" OUT="
bnum = 2
/mnt/mtfs1:/mnt/mtfs2
subject = (null)
" expect 0

#
# test 3
# Unkown option is checked
#
IN="unkown_option,device=:/mnt/mtfs1:/mnt/mtfs2:
" OUT="
" expect -EINVAL

#
# test 4
# Only one device option is allowed
#
IN="
device=:/mnt/mtfs1:/mnt/mtfs2:,device=:/mnt/mtfs1:/mnt/mtfs2:
" OUT="
" expect -EINVAL

#
# test 5
# Subject is set
#
IN="
device=:/mnt/mtfs1:/mnt/mtfs2:,subject=high-reliability
" OUT="
bnum = 2
/mnt/mtfs1:/mnt/mtfs2
subject = high-reliability
" expect 0

