#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="tests for rule_tree"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..1"

#test 1
IN="
" OUT="
" expect 0

