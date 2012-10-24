#! /bin/bash
#call examples:
# sh acceptance-small.sh
# ACC_SM_ONLY=EXAMPLE sh acceptance-small.sh
# EXAMPLE_EXCEPT=0 sh acceptance-small.sh
# START_AT=0a STOP_AT=1 ACC_SM_ONLY=EXAMPLE sh acceptance-small.sh
# ONLY=0a ACC_SM_ONLY=EXAMPLE sh acceptance-small.sh

set -e
[ "$CONFIGS" ] || CONFIGS="replica_ext4"

#export TESTSUITE_LIST="COMPILE"
export TESTSUITE_LIST="MOUNT POSIX MULTI_MNT SINGLE_BRANCH ABANDON_BRANCH RAID RACER COMPILE PJD_FSTEST"
#export TESTSUITE_LIST="MULTI_MNT"
#export TESTSUITE_LIST="POSIX MULTI_MNT SINGLE_BRANCH PJD_FSTEST"
#export TESTSUITE_LIST="COMPILE"
#export TESTSUITE_LIST="ABANDON_BRANCH"
#export TESTSUITE_LIST="RAID"
#export TESTSUITE_LIST="RACER"
#export TESTSUITE_LIST="RAID ABANDON_BRANCH"
#export TESTSUITE_LIST="MOUNT POSIX"
#export TESTSUITE_LIST="MULTI_MNT"
#export TESTSUITE_LIST="MULTI_MNT PJD_FSTEST"
#export TESTSUITE_LIST="POSIX MULTI_MNT SINGLE_BRANCH PJD_FSTEST"
#export TESTSUITE_LIST="POSIX"
#export TESTSUITE_LIST="SINGLE_BRANCH"
#export TESTSUITE_LIST="PJD_FSTEST"
#export TESTSUITE_LIST="MOUNT POSIX MULTI_MNT PJD_FSTEST"
#export TESTSUITE_LIST="MOUNT POSIX MULTI_MNT RACER PJD_FSTEST"

if [ "$ACC_SM_ONLY" ]; then
    for TESTSUITE in $TESTSUITE_LIST; do
		export ${TESTSUITE}="no"
    done
    for TESTSUITE in $ACC_SM_ONLY; do
		TESTSUITE=`echo ${TESTSUITE%.sh} | tr "-" "_"`
		TESTSUITE=`echo $TESTSUITE | tr "[:lower:]" "[:upper:]"`
		export ${TESTSUITE}="yes"
    done
fi

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
STARTTIME=`date +%s`
RANTEST=""

title()
{
	log "-----============= acceptance-small: "$*" ============----- `date`"
	RANTEST=${RANTEST}$*", "
}

#cleanup_all "skip_leak_check"
run_tests()
{
	local NAME=$1
	. $TESTS_DIR/cfg/$NAME.sh
	
	for TESTSUITE in $TESTSUITE_LIST; do
		if [ "${!TESTSUITE}" != "no" ]; then
			title $TESTSUITE
			local testsuite=`echo $TESTSUITE | tr "[:upper:]" "[:lower:]"`
			bash $testsuite.sh
			export ${TESTSUITE}="done"
		fi
	done
	
	return 0
}
init_test_env
for NAME in $CONFIGS; do
	run_tests $NAME
done

echo "Finished at `date` in $((`date +%s` - $STARTTIME))s"
echo "Tests ran: $RANTEST"
print_summary

