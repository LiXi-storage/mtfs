#!/bin/bash
set -e

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}

. $TESTS_DIR/test-framework.sh
init_test_env

cleanup_abandon()
{
	rm $MTFS_DIR1/test/* -fr
	rm $MTFS_DIR2/test/* -fr
	cleanup_and_setup
	$UTIL_MTFS setbranch -b 0 -d 0 $MTFS_MNT1/test/
	$UTIL_MTFS setbranch -b 1 -d 0 $MTFS_MNT1/test/
}

cleanup_abandon

ORIGIN_DIR=$DIR
ORIGIN_DIR1=$DIR1
ORIGIN_DIR2=$DIR2

export ABANDON_DIR=${ABANDON_DIR:-$ORIGIN_DIR/abandon_tests}
rm $ABANDON_DIR -fr
check_dir_nonexist $ABANDON_DIR ||  error "$ABANDON_DIR: exist $?"
mkdir $ABANDON_DIR || error "mkdir $ABANDON_DIR failed"
check_dir_exist $ABANDON_DIR ||  error "$ABANDON_DIR: not exist $?"

export DIR=$ABANDON_DIR
export DIR1=$MTFS_MNT1/$DIR_SUB/abandon_tests
export DIR2=$MTFS_MNT2/$DIR_SUB/abandon_tests

if [ "$SKIP_ABANDON_BRANCH0" != "yes" ]; then
	export ABANDON_BINDEX="0"
	echo 3xxxx
	bash posix.sh
	bash multi_mnt.sh
fi

cleanup_abandon

if [ "$SKIP_ABANDON_BRANCH1" != "yes" ]; then
	export ABANDON_BINDEX="1"
	bash posix.sh
	bash multi_mnt.sh
fi

export ABANDON_BINDEX="-1"

export DIR=$ORIGIN_DIR
export DIR1=$ORIGIN_DIR1
export DIR2=$ORIGIN_DIR2

cleanup_abandon
cleanup_all
echo "=== $0: completed ==="
