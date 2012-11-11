#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="random tests for mlowerfs bucket"

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
