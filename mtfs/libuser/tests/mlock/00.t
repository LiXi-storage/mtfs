#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="tests for mlock"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..34"

#
# TEST FORMAT:
# INPUT:
# n            -- operation number
# start end    -- $n lines for operation
#
# OUTPUT:
# [start, end] -- a line of bucket
#

# C: Canceled
# B: Blocked
# G: Granted
# N: Null

#test 1
IN="2
0 W 2 5
1 W 0 0
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test 2
IN="2
0 W 2 5
1 W 0 1
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test 3
IN="2
0 W 2 5
1 W 0 2
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test 4
IN="2
0 W 2 5
1 W 0 2
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test 5
IN="2
0 W 2 5
1 W 0 3
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test6
IN="2
0 W 2 5
1 W 0 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test7
IN="2
0 W 2 5
1 W 0 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test8
IN="2
0 W 2 5
1 W 0 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test9
IN="2
0 W 2 5
1 W 1 1
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test10
IN="2
0 W 2 5
1 W 1 2
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test11
IN="2
0 W 2 5
1 W 1 3
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test12
IN="2
0 W 2 5
1 W 1 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test13
IN="2
0 W 2 5
1 W 1 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test14
IN="2
0 W 2 5
1 W 1 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test15
IN="2
0 W 2 5
1 W 1 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test16
IN="2
0 W 2 5
1 W 2 2
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test17
IN="2
0 W 2 5
1 W 2 3
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test18
IN="2
0 W 2 5
1 W 2 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test19
IN="2
0 W 2 5
1 W 2 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test20
IN="2
0 W 2 5
1 W 2 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test21
IN="2
0 W 2 5
1 W 2 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test22
IN="2
0 W 2 5
1 W 3 3
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test23
IN="2
0 W 2 5
1 W 3 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test243
IN="2
0 W 2 5
1 W 3 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test25
IN="2
0 W 2 5
1 W 3 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test26
IN="2
0 W 2 5
1 W 4 4
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test27
IN="2
0 W 2 5
1 W 4 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test28
IN="2
0 W 2 5
1 W 4 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test29
IN="2
0 W 2 5
1 W 4 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test30
IN="2
0 W 2 5
1 W 5 5
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test31
IN="2
0 W 2 5
1 W 5 6
0 + 0 G 1 N
1 + 0 G 1 B
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test32
IN="2
0 W 2 5
1 W 6 6
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test33
IN="2
0 W 2 5
1 W 6 6
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0

#test34
IN="2
0 W 2 5
1 W 6 7
0 + 0 G 1 N
1 + 0 G 1 G
0 - 0 N 1 G
1 - 0 N 1 N
" OUT="
" expect 0