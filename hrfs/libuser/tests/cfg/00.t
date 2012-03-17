#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

desc="tests for rule_tree"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..13"

#
# TEST FORMAT:
# INPUT:
# type = $type
# param = $param
# $param = $value
#
# OUTPUT:
# $param = $value
#

#test 1
IN="
type = string
param = STRING
STRING = OKOKOKOK
" OUT="
STRING = OKOKOKOK
" expect 0

#test 2
IN="
type = number
param = NUMBER
NUMBER = -88888888
" OUT="
NUMBER = -88888888
" expect 0

#test 3
IN="
type = int8
param = INT8
INT8 = -127
" OUT="
INT8 = -127
" expect 0

#test 4
IN="
type = int8
param = INT8
INT8 = 127
" OUT="
INT8 = 127
" expect 0


#test 5
IN="
type = uint8
param = UINT8
UINT8 = 255
" OUT="
UINT8 = 255
" expect 0

#test 6
IN="
type = uint8
param = UINT8
UINT8 = 256
" OUT="
UINT8 = 0
" expect 0

#test 7
IN="
type = int16
param = INT16
INT16 = -6666
" OUT="
INT16 = -6666
" expect 0

#test 8
IN="
type = uint16
param = UINT16
UINT16 = 6666
" OUT="
UINT16 = 6666
" expect 0

#test 9
IN="
type = int32
param = INT32
INT32 = -66666666
" OUT="
INT32 = -66666666
" expect 0

#test 10
IN="
type = uint32
param = UINT32
UINT32 = 66666666
" OUT="
UINT32 = 66666666
" expect 0

#test 11
IN="
type = int64
param = INT64
INT64 = -6666666666666666
" OUT="
INT64 = -6666666666666666
" expect 0

#test 12
IN="
type = uint64
param = UINT64
UINT64 = 6666666666666666
" OUT="
UINT64 = 6666666666666666
" expect 0

#test 13
IN="
type = double
param = DOUBLE
DOUBLE = -0.6666666666666666
" OUT="
DOUBLE = -0.667
" expect 0

