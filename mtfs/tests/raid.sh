#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

BUG_343="1a"

EXCEPT_SLOW=""

export CONFIGS=${CONFIGS:-local}
. $TESTS_DIR/cfg/$CONFIGS.sh

if [ "$SUBJECT_NAME" != "replica" ]; then
	SKIP_4="4"
fi

ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$EXAMPLE_EXCEPT $BUG_343 $SKIP_4"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

liberate_file()
{
	local FILE=$1
	$UTIL_MTFS setbranch -b 0 -d 0 $FILE 2>&1 > /dev/null || error "set flag failed"
	$UTIL_MTFS setbranch -b 1 -d 0 $FILE 2>&1 > /dev/null || error "set flag failed"
}

# Dentry should be alright if the bad branch is removed.
remove_bad_branch() 
{
	local BRANCH_NUM=$1
	local BRANCH_DIR
	local INDEX

	rm $DIR/$tfile -f 2>&1 > /dev/null
	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		BRANCH_DIR=${BRANCH_DIR_ARRAY[$INDEX]}
		check_nonexist $BRANCH_DIR/$DIR_SUB/$tfile || error "branch[$INDEX]: exist $?"
	done;
	check_nonexist $DIR/$tfile || error "exist $? after remove"
	touch $DIR/$tfile || error "create failed"
	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		BRANCH_DIR=${BRANCH_DIR_ARRAY[$INDEX]}
		check_exist $BRANCH_DIR/$DIR_SUB/$tfile || error "branch[$INDEX]: not exist $?"
	done;

	liberate_file $DIR
	check_exist $DIR/$tfile || error "not exist $?"

	$UTIL_MTFS setbranch -b $BRANCH_NUM -d 1 $DIR 2>&1 > /dev/null || error "set flag failed"
	BRANCH_DIR=${BRANCH_DIR_ARRAY[$BRANCH_NUM]}
	rm $BRANCH_DIR/$DIR_SUB/$tfile -f || error "rm branch[$BRANCH_NUM] failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	check_exist $DIR/$tfile || error "not exist $? after removed branch[$BRANCH_NUM]"

	liberate_file $DIR
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
	local BRANCH_DIR
	local INDEX

	local BAD_BRANCH_NUM=$(get_opposite_bindex $BRANCH_NUM)

	rm $DIR/$tfile -f 2>&1 > /dev/null
	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		BRANCH_DIR=${BRANCH_DIR_ARRAY[$INDEX]}
		check_nonexist $BRANCH_DIR/$DIR_SUB/$tfile || error "branch[$INDEX]: exist $?"
	done;
	check_nonexist $DIR/$tfile || error "exist $? after remove"
	touch $DIR/$tfile || error "create failed"
	for INDEX in ${!BRANCH_DIR_ARRAY[@]}; do
		BRANCH_DIR=${BRANCH_DIR_ARRAY[$INDEX]}
		check_exist $BRANCH_DIR/$DIR_SUB/$tfile || error "branch[$INDEX]: not exist $?"
	done;

	liberate_file $DIR
	check_exist $DIR/$tfile || error "not exist $?"

	$UTIL_MTFS setbranch -b $BAD_BRANCH_NUM -d 1 $DIR 2>&1 > /dev/null || error "set flag failed"
	BRANCH_DIR=${BRANCH_DIR_ARRAY[$BRANCH_NUM]}
	rm $BRANCH_DIR/$DIR_SUB/$tfile -f || error "rm branch[$BRANCH_NUM] failed"

	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "no" ]; then
		cleanup_and_setup
	fi

	check_nonexist $DIR/$tfile || error "exist $? after removed branch[$BRANCH_NUM]"
	mkdir $DIR/$tfile || error "mkdir failed"
	rmdir $DIR/$tfile || error "rmdir failed"

	liberate_file $DIR
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
	$UTIL_MTFS setbranch -b 0 -d 1 $MTFS_MNT1
	$UTIL_MTFS getstate $MTFS_MNT1
	$UTIL_MTFS setbranch -b 0 -d 0 $MTFS_MNT1
	$UTIL_MTFS getstate $MTFS_MNT1
	$UTIL_MTFS setbranch -b 1 -d 1 $MTFS_MNT1
	$UTIL_MTFS getstate $MTFS_MNT1
	$UTIL_MTFS setbranch -b 1 -d 0 $MTFS_MNT1
	$UTIL_MTFS getstate $MTFS_MNT1
}
run_test 1a "set and get state of mount point"

check_isolate()
{
	local ENTRY="$1"
	local BINDEX="$2"
	FAILED_BINDEX=$($UTIL_MTFS getstate $ENTRY | grep ffff | awk '{print $1}')
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

	$UTIL_MTFS rmbranch -b $BINDEX $ENTRY  || error "failed to rmbranch";
	check_isolate $ENTRY $BINDEX || error "failed to isolate";
	local OPPOSITE_BINDEX=$(get_opposite_bindex $BINDEX)
	echo "$UTIL_MTFS setbranch -b $OPPOSITE_BINDEX -d 1 $ENTRY"
	$UTIL_MTFS setbranch -b $OPPOSITE_BINDEX -d 1 ./ 2>&1 > /dev/null || error "set flag failed"
	check_nonexist $ENTRY || error "$ENTRY exist $?"

	liberate_file $DIR
	rm $DIR/d2a -rf
	cd $PWD
}

test_2a()
{
	isolate_long_path 1 10 || error "failed to isolate long path"
	isolate_long_path 1 11 || error "failed to isolate long path"
}
#run_test 2a "long path when cleanup branch"

test_3a()
{
	touch $DIR/f3a
	$UTIL_MTFS rmbranch -b 100 $DIR/f3a > /dev/null  2>&1 && error "rm branch succeeded";
	$UTIL_MTFS rmbranch -b -1 $DIR/f3a > /dev/null  2>&1 && error "rm branch succeeded";
	return 0
}
run_test 3a "illegal argument to rmbranch"

test_3b()
{
	rm $DIR/f3b -f
	touch $DIR/f3b
	$UTIL_MTFS rmbranch -b 1 $DIR/f3b || error "rm branch[1] failed";
	check_exist $DIR/f3b || error "$DIR/f3b not exist $?"
	echo "$UTIL_MTFS rmbranch -b 0 $DIR/f3b"
	$UTIL_MTFS rmbranch -b 0 $DIR/f3b || error "rm branch[0] failed";
	check_nonexist $DIR/f3b || error "$DIR/f3b exist $?"
}
run_test 3b "rmbranch all branches"

test_4() {
	$MULTICORRECT $DIR/$tfile $BRANCH_0/$tfile $BRANCH_1/$tfile -s 60 > /dev/null \
	|| error "multicorret failed"
	diff $BRANCH_0/$tfile $BRANCH_1/$tfile > /dev/null || error "file diff"
	rm -f $DIR/$tfile
}
leak_detect_state_push "no"
run_test 4 "concurrent write test ======================================="
leak_detect_state_pop


cleanup_all
echo "=== $0: completed ==="
