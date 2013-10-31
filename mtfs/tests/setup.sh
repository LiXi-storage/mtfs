#! /bin/bash
# This script is for setting up a test enviroment

CONFIGS=$1
if [ "$CONFIGS" = "" ]; then
	echo "Please give me a config name, e.g. sync_replica_ext3"
	exit 1
fi

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh

init_test_env
