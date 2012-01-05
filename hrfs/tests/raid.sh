#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="
BRANCH_0="$HRFS_DIR1/test"
BRANCH_1="$HRFS_DIR2/test"

remove_branch() 
{
	BRANCH_NUM=$1
	BRANCH="BRANCH"_"$BRANCH_NUM"
	BRANCH_DIR=${!BRANCH}

	rm $DIR/$tfile -f 2>&1 > /dev/null
	touch $DIR/$tfile || error "create failed"
	rm $BRANCH_DIR/$tfile -f || error "rm branch $BRANCH_NUM failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	$CHECKSTAT -t file $DIR/$tfile || error "not exist 1"
	ls $DIR/$tfile > /dev/null || error "not exist 2"
	ls $DIR | grep -q $tfile || error "not exist 3"  || true
}

test_0a() 
{
	remove_branch 0
}
run_test 0a "removing branch 0 is ok"

test_0b() 
{
	remove_branch 1
}
run_test 0b "removing branch 1 is ok"

cleanup_all
echo "=== $0: completed ==="
