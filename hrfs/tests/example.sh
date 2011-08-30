#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

DOING_MUTLTI="yes"

BUG="0"

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$BUG $EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="
RACER_DIR=
. 
#tests 0: examples
test_0()
{
	return 0
}
run_test 0 "do nothing"

test_0a()
{
	return 0
}
run_test 0a "just an example"

#tests 0: other examples
test_1()
{
	return 0
}
run_test 1 "I want this test to be excluded"

test_1a()
{
	return 0
}
run_test 1 "another example"

cleanup_all
echo "=== $0: completed ==="
