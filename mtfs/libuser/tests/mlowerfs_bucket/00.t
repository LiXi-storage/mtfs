#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
#

desc="tests for mlowerfs_bucket"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..48"

#
# TEST FORMAT:
# INPUT:
# n            -- operation number
# start end    -- $n lines for operation
#
# OUTPUT:
# [start, end] -- a line of bucket
#


#test 1
IN="3
0 0
1 2
3 4000
0
" OUT="
[0, 0], [1, 2], [3, 4000]
" expect 0

#
# test 2
#
IN="2
0 0
0 1
" OUT="
0 1
"  OUT="
[0, 1]
" expect 0

#
# test 3
#
IN="2
1 1
0 2
" OUT="
[0, 2]
" expect 0

#
# test 4
#
IN="2
1 1
0 1
" OUT="
[0, 1]
" expect 0

#
# test 5
#
IN="2
1 1
0 0
" OUT="
[0, 0], [1, 1]
" expect 0

#
# test 6
#
IN="3
1 1
2 2
1 2
" OUT="
[1, 2]
" expect 0

#
# test 7
#
IN="3
1 1
2 2
1 3
" OUT="
[1, 3]
" expect 0

#
# test 8
#
IN="3
1 1
2 2
0 2
" OUT="
[0, 2]
" expect 0

#
# test 9
#
IN="3
1 1
2 2
0 3
" OUT="
[0, 3]
" expect 0

#
# test 10
#
IN="3
0 3
1 1
2 2
" OUT="
[0, 3]
" expect 0

#
# test 11
#
IN="3
0 3
1 1
2 2
" OUT="
[0, 3]
" expect 0

#
# test 12
#
IN="3
1 3
1 1
2 2
" OUT="
[1, 3]
" expect 0

#
# test 13
#
IN="3
0 2
1 1
2 2
" OUT="
[0, 2]
" expect 0

#
# test 14
#
IN="3
1 2
1 1
2 2
" OUT="
[1, 2]
" expect 0

#
# test 15
#
IN="3
1 2
5 6
0 0
" OUT="
[0, 0], [1, 2], [5, 6]
" expect 0

#
# test 16
#
IN="3
1 2
5 6
0 1
" OUT="
[0, 2], [5, 6]
" expect 0

#
# test 17
#
IN="3
1 2
5 6
0 2
" OUT="
[0, 2], [5, 6]
" expect 0

#
# test 18
#
IN="3
1 2
5 6
0 3
" OUT="
[0, 3], [5, 6]
" expect 0

#
# test 19
#
IN="3
1 2
5 6
0 4
" OUT="
[0, 4], [5, 6]
" expect 0

#
# test 20
#
IN="3
1 2
5 6
0 5
" OUT="
[0, 6]
" expect 0

#
# test 21
#
IN="3
1 2
5 6
0 6
" OUT="
[0, 6]
" expect 0

#
# test 22
#
IN="3
1 2
5 6
0 7
" OUT="
[0, 7]
" expect 0

#
# test 23
#
IN="3
1 2
5 6
1 1
" OUT="
[1, 2], [5, 6]
" expect 0

#
# test 24
#
IN="3
1 2
5 6
1 3
" OUT="
[1, 3], [5, 6]
" expect 0

#
# test 25
#
IN="3
1 2
5 6
1 4
" OUT="
[1, 4], [5, 6]
" expect 0

#
# test 26
#
IN="3
1 2
5 6
1 5
" OUT="
[1, 6]
" expect 0

#
# test 27
#
IN="3
1 2
5 6
1 6
" OUT="
[1, 6]
" expect 0

#
# test 28
#
IN="3
1 2
5 6
1 7
" OUT="
[1, 7]
" expect 0

#
# test 29
#
IN="3
1 2
5 6
2 2
" OUT="
[1, 2], [5, 6]
" expect 0

#
# test 30
#
IN="3
1 2
5 6
2 3
" OUT="
[1, 3], [5, 6]
" expect 0

#
# test 31
#
IN="3
1 2
5 6
2 4
" OUT="
[1, 4], [5, 6]
" expect 0

#
# test 32
#
IN="3
1 2
5 6
2 5
" OUT="
[1, 6]
" expect 0

#
# test 33
#
IN="3
1 2
5 6
2 7
" OUT="
[1, 7]
" expect 0

#
# test 34
#
IN="3
1 2
5 6
3 3
" OUT="
[1, 2], [3, 3], [5, 6]
" expect 0

#
# test 35
#
IN="3
1 2
5 6
3 4
" OUT="
[1, 2], [3, 4], [5, 6]
" expect 0

#
# test 36
#
IN="3
1 2
5 6
3 5
" OUT="
[1, 2], [3, 6]
" expect 0

#
# test 37
#
IN="3
1 2
5 6
3 6
" OUT="
[1, 2], [3, 6]
" expect 0

#
# test 38
#
IN="3
1 2
5 6
3 7
" OUT="
[1, 2], [3, 7]
" expect 0

#
# test 39
#
IN="3
1 2
5 6
4 4
" OUT="
[1, 2], [4, 4], [5, 6]
" expect 0

#
# test 40
#
IN="3
1 2
5 6
4 5
" OUT="
[1, 2], [4, 6]
" expect 0

#
# test 41
#
IN="3
1 2
5 6
4 6
" OUT="
[1, 2], [4, 6]
" expect 0

#
# test 42
#
IN="3
1 2
5 6
4 7
" OUT="
[1, 2], [4, 7]
" expect 0

#
# test 43
#
IN="3
1 2
5 6
5 5
" OUT="
[1, 2], [5, 6]
" expect 0

#
# test 44
#
IN="3
1 2
5 6
5 6
" OUT="
[1, 2], [5, 6]
" expect 0

#
# test 45
#
IN="3
1 2
5 6
5 7
" OUT="
[1, 2], [5, 7]
" expect 0

#
# test 46
#
IN="3
1 2
5 6
6 6
" OUT="
[1, 2], [5, 6]
" expect 0

#
# test 47
#
IN="3
1 2
5 6
6 7
" OUT="
[1, 2], [5, 7]
" expect 0

#
# test 48
#
IN="3
1 2
5 6
7 7
" OUT="
[1, 2], [5, 6], [7, 7]
" expect 0
