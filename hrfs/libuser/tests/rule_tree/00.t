#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

desc="tests for rule_tree"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..10"

#
# TEST FORMAT:
# INPUT:
# n -- rule number
# rule_string -- $n lines for rule_string
# m -- check number
# check_string -- $m lines for check string
#
# OUTPUT:
# rule_tree -- rule tree constructed
# raid -- $m lines for raid pattern checked
#


#test 1
IN="3
a 0
b 0
c 0
0
" OUT="
/_/_/
a|b|c
" expect 0

#
# test 2
# same rule should report error
#
IN="2
a 0
a 0
0
" OUT="
" expect -EINVAL

#
# test 3
# rule too long (> 32) should report error
#
IN="2
a 0
123456789012345678901234567890123 0
0
" OUT="
" expect -EINVAL

#
# test 4
# rule size = 32 should be ok
#
IN="2
a 0
12345678901234567890123456789012 0
0
" expect 0

#
# test 5
# rule checked
#
IN="3
a 0
b 1
c 5
4
a
b
c
d
" OUT="
/_/_/
a|b|c
0
1
5
0
" expect 0

#
# test 6
# result should be the longest matched rule
#
IN="2
aa 1
aaa 5
1
aaa
" OUT="
/_/
a_a
a_a
.|a
5
" expect 0

#
# test 7
# partly matched shold be judged as not matched #1
#
IN="2
aa 1
aba 5
1
ab
" OUT="
/_/
a_a
a|b
.|a
0
" expect 0

#
# test 8
# 
#
IN="2
xba 5
ba 1
4
ba
cba
xcba
abc
" OUT="
/_/
a_a
b_b
.|x
1
1
1
0
" expect 0

#test 9
IN="3
c 5
a 1
b 0
3
a
b
c
" OUT="
/_/_/
a|b|c
1
0
5
" expect 0

#test 10
IN="10
aac 0
adc 0
agc 0
ajc 0
abc 1
aec 1
ahc 1
acc 5
afc 5
aic 5
10
aac
abc
acc
adc
aec
afc
agc
ahc
aic
ajc
" OUT="
/_/_/_/_/_/_/_/_/_/
c_c_c_c_c_c_c_c_c_c
a|b|c|d|e|f|g|h|i|j
a|a|a|a|a|a|a|a|a|a
0
1
5
0
1
5
0
1
5
0
" expect 0
