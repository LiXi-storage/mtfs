#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

BUG=""

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$BUG $POSIX_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

PJD_DIR=${PJD_DIR:-$TESTS_DIR/pjd_fstest}
PJD_FSTEST=${PJD_FSTEST:-$PJD_DIR/fstest}

check_id()
{
	local localUID=$1
	local localGID=$2
	RUNAS_CMD=${RUNAS_CMD:-"$TESTS_DIR/src/runas"}
	check_runas_id $localUID $localGID "$RUNAS_CMD -u $localUID"
}

check_id 65533 65533
check_id 65534 65534
check_id 65535 65535


expect()
{
	e="${1}"
	shift
	r=`${PJD_FSTEST} $* 2>/dev/null | tail -1`
	echo "${r}" | egrep '^'${e}'$' >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "ok: expect ${e} $*"
	else
		echo "not ok - got ${r}: expect ${e} $*"
		error "got unexpected result"
	fi
}

#chmod_00_56
test_0()
{
	tfile="fstest_96014ba05dce070715913607413a38a3"
	rm -f $DIR/$tfile
	
	expect 0 create $DIR/$tfile 0755
	expect 0 chown $DIR/$tfile 65535 65535
	expect 0 -u 65535 -g 65535 chmod $DIR/$tfile 02755
	expect 02755 stat $DIR/$tfile mode
	expect 0 -u 65535 -g 65535 chmod $DIR/$tfile 0755
	expect 0755 stat $DIR/$tfile mode
	expect 0 -u 65535 -g 65534 chmod $DIR/$tfile 02755
	expect 0755 stat $DIR/$tfile mode
}
#run_test 0 "do nothing"

test_100()
{
	cd $DIR
	prove -r -v $PJD_DIR
}
run_test 100 "should pass all pjd_fstest"

cleanup_all
echo "=== $0: completed ==="