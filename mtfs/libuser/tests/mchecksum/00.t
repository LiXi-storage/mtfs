#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="tests for mchecksum"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..7"

#
# TEST FORMAT:
# INPUT:
# inputs
# AGAIN
# inputs
#
# OUTPUT:
# "equal" or "diff"
#

#test 1
IN="
AGAIN
" OUT="
equal
" expect 0

#test 2
IN="
abcdefg
AGAIN
abcdefg
" OUT="
equal
" expect 0

#test 3
IN="
abcdefg
AGAIN
abcdef
" OUT="
differ
" expect 0

#test 4
IN="
abcdefg
AGAIN
abcdefgh
" OUT="
differ
" expect 0

#test 5
IN="
abcdefg
AGAIN
abcdegf
" OUT="
differ
" expect 0

#test 6
IN="
abc
def
AGAIN
abcdef
" OUT="
equal
" expect 0

#test 7
IN="
def
abc
AGAIN
abcdef
" OUT="
differ
" expect 0