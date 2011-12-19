#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="
BRANCH_PRIMARY="$HRFS_DIR1/test"
BRANCH_SECONDARY="$HRFS_DIR2/test"

test_0a() 
{
	rm $DIR/$tfile -f 2>&1 > /dev/null
	touch $DIR/$tfile || error "create failed"
	rm $BRANCH_PRIMARY/$tfile -f || error "rm primary branch failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	$CHECKSTAT -t file $DIR/$tfile && error "still exist 1"
	ls $DIR/$tfile > /dev/null && error "still exist 2"
	ls $DIR | grep -q $tfile && error "still exist 3"
	mkdir $DIR/$tfile || error "recreate failed"
}
run_test 0a "remove primary branch"

test_0b() 
{
	rm $DIR/$tfile -f 2>&1 > /dev/null
	touch $DIR/$tfile || error "create failed"
	rm $BRANCH_SECONDARY/$tfile -f || error "rm secondary branch failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	$CHECKSTAT -t file $DIR/$tfile || error "not exist 1"
	ls $DIR/$tfile > /dev/null || error "not exist 2"
	ls $DIR | grep -q $tfile || error "not exist 3"  || true
}
run_test 0b "remove secondary branch"

cleanup_all
echo "=== $0: completed ==="
