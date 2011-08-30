#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

#
#TESTS_0: read and write
#Because of cache, writting on hidden_fs will not affect upper_fs, unless upper_fs restarted
#NEVER let this happen, because unexpected things always happen!
#
test_0a() 
{
	echo "...............rm $DIR/$tfile -f"
	rm $DIR/$tfile -f
	echo "...............touch $HRFS_DIR1/$tfile"
	touch $HRFS_DIR1/$tfile
	echo "...............ls $DIR/"
	ls $DIR/ 
}
run_test 0a "lookup a file with an invalid branch should not panic"

cleanup_all
echo "=== $0: completed ==="
