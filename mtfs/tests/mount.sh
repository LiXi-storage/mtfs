#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

EXCEPT_SLOW="51a"

if [ ${#LOWERFS_MNT_ARRAY[@]} -ne 1 ]; then
	SKIP_MULTI_MNT="1"
fi

ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"0b 0c $MOUNT_EXCEPT $SKIP_MULTI_MNT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env
echo "==== $0: started ===="

#tests 0: mount and umount
test_0()
{
	cleanup_all
	mount_lowerfs
	insert_mtfs_module
	insert_support_module
	$MOUNT_MTFS -o debug=$MTFS_DEBUG $MTFS_MOUNT && error "unexpected mount success"
	cleanup_and_setup
	return 0
}
run_test 0 "no dir option should fail"

test_0a()
{
	cleanup_all
	mount_lowerfs
	insert_mtfs_module
	insert_support_module
	#BUG: origin mtfs will not report error, and lustre will not be able to umount
	$MOUNT_MTFS -o device=$MTFS_DIR1:$MTFS_DIR2 -o device=$MTFS_DIR1/not_exist1:$MTFS_DIR2/not_exist1 -o debug=$MTFS_DEBUG $MTFS_DEV $MTFS_MOUNT && error "unexpected mount success"
	cleanup_and_setup
	return 0
}
run_test 0a "too many dir option should fail"

test_0b()
{
	for((i=0;i<100;i++)) {
		log "cleanup_and_setup test $i"
		cleanup_and_setup
	}
	return 0
}
run_test 0b "cleanup and then setup should be ok"

test_0c()
{
	DOING_MUTLTI="yes"
	for((i=0;i<100;i++)) {
		log "cleanup_and_setup test $i"
		cleanup_and_setup
	}
	cleanup_all
	DOING_MUTLTI="no"
	return 0
}
run_test 0c "cleanup and then setup multi_mnt should be ok"

test_0d() #BUG228
{
	cleanup_all
	insert_mtfs_module
	insert_support_module
	if [ "$MOUNT_LOWERFS" = "yes" ]; then
		mount_mtfs_noexit && error "should fail" || true
	fi
}
run_test 0d "mount mtfs without backend filesystem should fail"

test_0e()
{
	cleanup_all
	mount_lowerfs
	insert_mtfs_module
	mount_mtfs_noexit && error "should fail" || true
}
run_test 0e "mount mtfs without lowerfs support module should fail"

test_1() #BUG226
{
	local MTFS_STATICS=$(stat -f $MTFS_MNT1 -c "%s %S %b %c")
	local LOWERFS_STATICS=$(stat -f ${LOWERFS_MNT_ARRAY[0]} -c "%s %S %b %c")
	[ "$MTFS_STATICS" = "$LOWERFS_STATICS" ] || error "mtfs $MTFS_STATICS != lowerfs $LOWERFS_STATICS"	
}
run_test 1 "mtfs statics is same to lowerfs statics"

#todo: mount -o dir=dir2:dir1 should fail
cleanup_all
echo "==== $0: completed ===="
