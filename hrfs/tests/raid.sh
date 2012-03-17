#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

BUG_343="1a"

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$EXAMPLE_EXCEPT $BUG_343"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="
BRANCH_0="$HRFS_DIR1/test"
BRANCH_1="$HRFS_DIR2/test"

check_nonexist()
{
	local LOCAL_FILE=$1
	local LOCAL_DIR=$(dirname $1)
	local LOCAL_BASE=$(basename $1)
	local EXIST=0

	$CHECKSTAT -t file $LOCAL_FILE > /dev/null 2>&1
	local RETVAL=$?
	if [ $RETVAL -eq 0 ]; then
		EXIST=`expr $EXIST + 1`
	fi

	ls $LOCAL_FILE > /dev/null 2>&1
	RETVAL=$?
	if [ $RETVAL -eq 0 ]; then
		EXIST=`expr $EXIST + 2`
	fi

	ls $LOCAL_DIR | grep -q $LOCAL_BASE > /dev/null 2>&1
	RETVAL=$?
	if [ $RETVAL -eq 0 ]; then
		EXIST=`expr $EXIST + 4`
	fi

	return $EXIST
}

check_exist()
{
	check_nonexist $1
	local RETVAL=$?
	if [ $RETVAL -eq 7 ]; then
		return 0
	fi
	return $RETVAL
}

liberate_file()
{
	local FILE=$1
	$UTIL_HRFS setbranch -b 0 -d 0 $FILE 2>&1 > /dev/null || error "set flag failed"
	$UTIL_HRFS setbranch -b 1 -d 0 $FILE 2>&1 > /dev/null || error "set flag failed"
}

# Dentry should be alright if the bad branch is removed.
remove_bad_branch() 
{
	local BRANCH_NUM=$1
	local BRANCH="BRANCH"_"$BRANCH_NUM"
	local BRANCH_DIR=${!BRANCH}

	rm $DIR/$tfile -f 2>&1 > /dev/null
	echo "rm $DIR/$tfile -f"
	check_nonexist $BRANCH_0/$tfile || error "branch[0]: exist $?"
	check_nonexist $BRANCH_1/$tfile || error "branch[1]: exist $?"
	touch $DIR/$tfile || error "create failed"
	check_exist $BRANCH_0/$tfile || error "branch[0]: not exist $?"
	check_exist $BRANCH_1/$tfile || error "branch[1]: not exist $?"

	liberate_file $DIR
	check_exist $DIR/$tfile || error "not exist $?"

	$UTIL_HRFS setbranch -b $BRANCH_NUM -d 1 $DIR 2>&1 > /dev/null || error "set flag failed"
	rm $BRANCH_DIR/$tfile -f || error "rm branch[$BRANCH_NUM] failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	check_exist $DIR/$tfile || error "not exist $? after removed branch[$BRANCH_NUM]"
}

get_opposite_bindex()
{
	local BINDEX=$1
	if [ "$BINDEX" = "0" ]; then
		echo "1"
	else
		echo "0"
	fi
}

# Dentry Should be removed if the latest branch is removed.
remove_good_branch() 
{
	local BRANCH_NUM=$1
	local BRANCH="BRANCH"_"$BRANCH_NUM"
	local BRANCH_DIR=${!BRANCH}
	
	local BAD_BRANCH_NUM=$(get_opposite_bindex $BRANCH_NUM)

	rm $DIR/$tfile -f 2>&1 > /dev/null
	check_nonexist $BRANCH_0/$tfile || error "branch[0]: $BRANCH_0/$tfile exist $?"
	check_nonexist $BRANCH_1/$tfile || error "branch[1]: $BRANCH_1/$tfile exist $?"
	touch $DIR/$tfile || error "create failed"
	check_exist $BRANCH_0/$tfile || error "branch[0]: not exist $?"
	check_exist $BRANCH_1/$tfile || error "branch[1]: not exist $?"

	liberate_file $DIR
	check_exist $DIR/$tfile || error "not exist $?"

	$UTIL_HRFS setbranch -b $BAD_BRANCH_NUM -d 1 $DIR 2>&1 > /dev/null || error "set flag failed"
	rm $BRANCH_DIR/$tfile -f || error "rm branch[$BRANCH_NUM] failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	check_nonexist $DIR/$tfile || error "exist $? after removed branch[$BRANCH_NUM]"
	mkdir $DIR/$tfile || error "mkdir failed"
	rmdir $DIR/$tfile || error "rmdir failed"
}

test_0a() 
{
	remove_bad_branch 0
	remove_bad_branch 1
	remove_bad_branch 0
	remove_bad_branch 0
	remove_bad_branch 1
	remove_bad_branch 1
}
run_test 0a "removing bad branches is ok"

test_0b() 
{
	remove_good_branch 0
	remove_good_branch 1
	remove_good_branch 0
	remove_good_branch 0
	remove_good_branch 1
	remove_good_branch 1
}
run_test 0b "removing good branches"

test_1a()
{
	$UTIL_HRFS setbranch -b 0 -d 1 $HRFS_MNT1
	$UTIL_HRFS getstate $HRFS_MNT1
	$UTIL_HRFS setbranch -b 0 -d 0 $HRFS_MNT1
	$UTIL_HRFS getstate $HRFS_MNT1
	$UTIL_HRFS setbranch -b 1 -d 1 $HRFS_MNT1
	$UTIL_HRFS getstate $HRFS_MNT1
	$UTIL_HRFS setbranch -b 1 -d 0 $HRFS_MNT1
	$UTIL_HRFS getstate $HRFS_MNT1
}
run_test 1a "set and get state of mount point"

check_isolate()
{
	local ENTRY="$1"
	local BINDEX="$2"
	FAILED_BINDEX=$($UTIL_HRFS getstate $ENTRY | grep ffff | awk '{print $1}')
	if [ "$FAILED_BINDEX" = "$BINDEX" ]; then
		return 0
	fi
	return 1
}

isolate_long_path()
{
	local BINDEX="$1"
	local DEPTH=$2
	local PWD=`pwd`
	local ENTRY;

	mkdir $DIR/d2a
	cd $DIR/d2a
	for((ENTRY = 0;ENTRY < $DEPTH; ENTRY++)) {
		mkdir $ENTRY
		cd $ENTRY
		PARENT=$ENTRY
		echo $ENTRY
	}
	touch ./$ENTRY

	$UTIL_HRFS rmbranch -b $BINDEX $ENTRY  || error "failed to rmbranch";
	check_isolate $ENTRY $BINDEX || error "failed to isolate";
	local OPPOSITE_BINDEX=$(get_opposite_bindex $BINDEX)
	echo "$UTIL_HRFS setbranch -b $OPPOSITE_BINDEX -d 1 $ENTRY"
	$UTIL_HRFS setbranch -b $OPPOSITE_BINDEX -d 1 ./ 2>&1 > /dev/null || error "set flag failed"
	check_nonexist $ENTRY || error "$ENTRY exist $?"

	cd $PWD
}
test_2a()
{
	isolate_long_path 1 10 || error "failed to isolate long path"
	isolate_long_path 1 11 || error "failed to isolate long path"
}
run_test 2a "long path when cleanup branch"

cleanup_all
echo "=== $0: completed ==="
