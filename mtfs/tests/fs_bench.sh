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
	$TESTS_DIR/fs_bench/fs_bench.py
}
run_test 0 "run fs_bench"

cleanup_all
echo "=== $0: completed ==="
