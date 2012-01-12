#!/bin/bash
set -e

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}

. $TESTS_DIR/test-framework.sh
init_test_env

export ABANDON_BRANCH="0"
bash posix.sh
bash multi_mnt.sh

export ABANDON_BRANCH="1"
bash posix.sh
bash multi_mnt.sh

export ABANDON_BRANCH="-1"
echo "cleanup_all start"
cleanup_all
echo "=== $0: completed ==="
