#!/bin/bash
set -e

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}

. $TESTS_DIR/test-framework.sh
init_test_env

cleanup_abandon()
{
	local INDEX;
	local BRANCH_DIR;

	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		BRANCH_DIR=${BRANCH_DIR_ARRAY[$INDEX]}
		rm $BRANCH_DIR/$DIR_SUB/* -fr
	done;

	cleanup_and_setup

	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		$UTIL_MTFS setbranch -b $INDEX -d 0 $MTFS_MNT1/$DIR_SUB/
	done;
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

for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
	for SKIP_INDEX in ${SKIP_ABANDON_BRANCHES[@]}; do
		if [ "$SKIP_INDEX" = "$INDEX" ]; then
			continue 2
		fi
	done

	export ABANDON_BINDEX="$INDEX"
	bash posix.sh
	bash multi_mnt.sh
	cleanup_abandon
done;

export ABANDON_BINDEX="-1"

export DIR=$ORIGIN_DIR
export DIR1=$ORIGIN_DIR1
export DIR2=$ORIGIN_DIR2

cleanup_abandon
cleanup_all
echo "=== $0: completed ==="
