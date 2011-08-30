#!/bin/bash
set -e

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}

. $TESTS_DIR/test-framework.sh
init_test_env

ORIGIN_DIR=$DIR
ORIGIN_DIR1=$DIR1
ORIGIN_DIR2=$DIR2

export SINGLE_DIR=${SINGLE_DIR:-$ORIGIN_DIR/single_tests}
echo "rm start"
#ls $SINGLE_DIR -l # romove me
rm $SINGLE_DIR -fr
echo "rm end"
mkdir $HRFS_DIR1/$DIR_SUB/single_tests

export DIR=$SINGLE_DIR
export DIR1=$HRFS_MNT1/$DIR_SUB/single_tests
export DIR2=$HRFS_MNT2/$DIR_SUB/single_tests

bash posix.sh
bash multi_mnt.sh

export DIR=$ORIGIN_DIR
export DIR1=$ORIGIN_DIR1
export DIR2=$ORIGIN_DIR2

echo "cleanup_all start"
cleanup_all
echo "=== $0: completed ==="
