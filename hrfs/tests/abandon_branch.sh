#!/bin/bash
set -e

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}

. $TESTS_DIR/test-framework.sh
init_test_env

cleanup_abandon()
{
	rm $HRFS_DIR1/test/* -fr
	rm $HRFS_DIR2/test/* -fr
	cleanup_and_setup
	$UTIL_HRFS setbranch -b 0 -d 0 $HRFS_MNT1/test/
	$UTIL_HRFS setbranch -b 1 -d 0 $HRFS_MNT1/test/
}

cleanup_abandon

ORIGIN_DIR=$DIR
ORIGIN_DIR1=$DIR1
ORIGIN_DIR2=$DIR2

export ABANDON_DIR=${ABANDON_DIR:-$ORIGIN_DIR/abandon_tests}
rm $ABANDON_DIR -fr
mkdir $ABANDON_DIR || error "mkdir $ABANDON_DIR failed"

export DIR=$ABANDON_DIR
export DIR1=$HRFS_MNT1/$DIR_SUB/abandon_tests
export DIR2=$HRFS_MNT2/$DIR_SUB/abandon_tests

export ABANDON_BRANCH="0"
bash posix.sh
bash multi_mnt.sh

cleanup_abandon

export ABANDON_BRANCH="1"
bash posix.sh
bash multi_mnt.sh

export ABANDON_BRANCH="-1"

export DIR=$ORIGIN_DIR
export DIR1=$ORIGIN_DIR1
export DIR2=$ORIGIN_DIR2

cleanup_abandon
cleanup_all
echo "=== $0: completed ==="
