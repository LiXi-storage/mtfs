#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

desc="tests for branch_bitmap"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..1"

#
# TEST FORMAT:
# INPUT:
# nothing
#
# OUTPUT:
# message if error, nothing if ok
#


#test 1
IN="
" OUT="
" expect 0
