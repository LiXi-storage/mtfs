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
device=/mnt/hrfs1:/mnt/hrfs2:/mnt/hrfs3:/mnt/hrfs4:/mnt/hrfs5:/mnt/hrfs6,debug=0
" OUT="
bnum = 6
/mnt/hrfs1:/mnt/hrfs2:/mnt/hrfs3:/mnt/hrfs4:/mnt/hrfs5:/mnt/hrfs6
debug_level = 0
" expect 0

#
# test 2
# Colon at beginning or end is allowed
#
IN="
device=:/mnt/hrfs1:/mnt/hrfs2:,debug=1
" OUT="
bnum = 2
/mnt/hrfs1:/mnt/hrfs2
debug_level = 1
" expect 0

#
# test 3
# Debug level value is checked
#
IN="
device=:/mnt/hrfs1:/mnt/hrfs2:,debug=-12345
" OUT="
" expect -EINVAL


#
# test 4
# Unkown option is checked
#
IN="unkown_option,device=:/mnt/hrfs1:/mnt/hrfs2:,debug=1
" OUT="
" expect -EINVAL

#
# test 5
# Only one device option is allowed
#
IN="
device=:/mnt/hrfs1:/mnt/hrfs2:,debug=-12345,device=:/mnt/hrfs1:/mnt/hrfs2:
" OUT="
" expect -EINVAL
