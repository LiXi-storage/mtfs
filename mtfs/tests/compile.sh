#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

BUG=""

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$BUG $EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

umask 077

compile()
{
	SOURCE_DIR=$1
	BASE_NAME=$(basename $SOURCE_DIR)
	rm $DIR/$BASE_NAME -fr
	cp $SOURCE_DIR -a $DIR/$BASE_NAME || error "cp failed"
	make clean -C $DIR/$BASE_NAME || error "make clean failed"
	make -C $DIR/$BASE_NAME || error "make failed"
	make clean -C $DIR/$BASE_NAME  || error "make clean again failed"
}

LINUX_DIR=$(grep "LINUX =" ../../autoMakefile | awk '{print $3}')

test_0()
{
	PWD=$(pwd)
	SOURCE_DIR=$(cd ../../; pwd; cd $PWD)
	BASE_NAME=$(basename $SOURCE_DIR)

	rm $DIR/$BASE_NAME -fr
	cp $SOURCE_DIR -a $DIR/$BASE_NAME || error "cp failed"
	cd $DIR/$BASE_NAME
	./configure --with-linux=$LINUX_DIR
	cd $PWD
	make clean -C $DIR/$BASE_NAME || error "make clean failed"
	make -C $DIR/$BASE_NAME || error "make failed"
	make distclean -C $DIR/$BASE_NAME  || error "make distclean failed"
}
run_test 0 "compile mtfs source code"

test_1()
{
	compile $LINUX_DIR
}
run_test 1 "compile kernel source code"

cleanup_all
echo "=== $0: completed ==="
