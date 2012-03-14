#! /bin/bash
# This script is for setting up a test enviroment

CONFIGS=$1
if [ "$CONFIGS" = "" ]; then
	CONFIGS="swgfs"
fi

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh

init_test_env
if [ "$CONFIGS" = "lustre" ]; then
	cp /home/lixi/lustre-2.1.52/lustre/llite/lustre.ko /lib/modules/2.6.18-prep/updates/kernel/fs/lustre
	#cp /home/lixi/lustre-2.1.52/lustre/obdclass/obdclass.ko /lib/modules/2.6.18-prep/updates/kernel/fs/lustre
	cleanup_all_check
	rmmod lustre
	modprobe lustre
fi

init_test_env
