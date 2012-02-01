#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

DOING_MUTLTI="yes"

LOWERFS_BUG_331="4"
LOWERFS_BUG_260="5"
BUG_267="14x 14b"
LOWERFS_BUG_274="25"

EXCEPT_SLOW=""
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$LOWERFS_BUG_260 $BUG_267 $LOWERFS_BUG_274 $LOWERFS_BUG_331 $MULTI_MNT_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

if [ "$LOWERFS_HAVE_DEV" = "yes" ]; then
	echo "Lowerfs have backup dev, not able to run MULTI_MNT tests"
	echo "Skeep all"
	exit 0
fi

test_1a() {
	touch $DIR1/f1
	[ -f $DIR2/f1 ] || error
}
run_test 1a "check create on 2 mtpt's =========================="

test_1b() {
	chmod 777 $DIR2/f1
	$CHECKSTAT -t file -p 0777 $DIR1/f1 || error
	chmod a-x $DIR2/f1
}
run_test 1b "check attribute updates on 2 mtpt's ==============="

test_1c() {
	$CHECKSTAT -t file -p 0666 $DIR1/f1 || error
}
run_test 1c "check after remount attribute updates on 2 mtpt's ="

test_1d() {
	rm $DIR2/f1
	$CHECKSTAT -a $DIR1/f1 || error
}
run_test 1d "unlink on one mountpoint removes file on other ===="

test_2a() {
	touch $DIR1/f2a
	ls -l $DIR2/f2a
	chmod 777 $DIR2/f2a
	$CHECKSTAT -t file -p 0777 $DIR1/f2a || error
}
run_test 2a "check cached attribute updates on 2 mtpt's ========"

test_2b() {
	touch $DIR1/f2b
	ls -l $DIR2/f2b
	chmod 777 $DIR1/f2b
	$CHECKSTAT -t file -p 0777 $DIR2/f2b || error
}
run_test 2b "check cached attribute updates on 2 mtpt's ========"

# NEED TO SAVE ROOT DIR MODE
test_2c() {
	chmod 777 $DIR1
	$CHECKSTAT -t dir -p 0777 $DIR2 || error
}
run_test 2c "check cached attribute updates on 2 mtpt's root ==="

test_2d() {
	chmod 755 $DIR1
	$CHECKSTAT -t dir -p 0755 $DIR2 || error
}
run_test 2d "check cached attribute updates on 2 mtpt's root ==="

test_2e() {
	chmod 755 $DIR1
	ls -l $DIR1
	ls -l $DIR2
	chmod 777 $DIR1
	$RUNAS dd if=/dev/zero of=$DIR2/$tfile count=1 || error
}
run_test 2e "check chmod on root is propagated to others"

test_3() {
	( cd $DIR1 ; ln -s this/is/good $tfile )
	[ "this/is/good" = "`perl -e 'print readlink("'$DIR2/$tfile'");'`" ] ||
		error "link $DIR2/$tfile not as expected"
}
run_test 3 "symlink on one mtpt, readlink on another ==========="

test_4() {
	$MULTIFSTAT $DIR1/f4 $DIR2/f4
}
run_test 4 "fstat validation on multiple mount points =========="

test_5() {
	$MCREATE $DIR1/f5
	$TRUNCATE $DIR2/f5 100
	$CHECKSTAT -t file -s 100 $DIR1/f5 || error
	rm $DIR1/f5
}
run_test 5 "create a file on one mount, truncate it on the other"

test_6() {
	$OPENUNLINK $DIR1/$tfile $DIR2/$tfile || \
		error "openunlink $DIR1/$tfile $DIR2/$tfile"
}
run_test 6 "remove of open file on other node =================="

test_7() {
	local dir=d7
	$OPENDIRUNLINK $DIR1/$dir $DIR2/$dir || \
		error "opendirunlink $DIR1/$dir $DIR2/$dir"
}
run_test 7 "remove of open directory on other node ============="

test_8() {
	$OPENDEVUNLINK $DIR1/$tfile $DIR2/$tfile || \
		error "opendevunlink $DIR1/$tfile $DIR2/$tfile"
}
run_test 8 "remove of open special file on other node =========="

test_9() {
	MTPT=1
	local dir
	> $DIR2/f9
	for C in a b c d e f g h i j k l; do
		dir=`eval echo \\$DIR$MTPT`
		echo -n $C >> $dir/f9
		[ "$MTPT" -eq 1 ] && MTPT=2 || MTPT=1
	done
	[ "`cat $DIR1/f9`" = "abcdefghijkl" ] || \
		error "`od -a $DIR1/f9` != abcdefghijkl"
}
run_test 9 "append of file with sub-page size on multiple mounts"

test_10a() {
	MTPT=1
	local dir
	OFFSET=0
	> $DIR2/f10
	for C in a b c d e f g h i j k l; do
		dir=`eval echo \\$DIR$MTPT`
		echo -n $C | dd of=$dir/f10 bs=1 seek=$OFFSET count=1
		[ "$MTPT" -eq 1 ] && MTPT=2 || MTPT=1
		OFFSET=`expr $OFFSET + 1`
	done
	[ "`cat $DIR1/f10`" = "abcdefghijkl" ] || \
		error "`od -a $DIR1/f10` != abcdefghijkl"
}
run_test 10a "write of file with sub-page size on multiple mounts "

test_10b() {
	# create a seed file
	yes "R" | head -c 4000 >$TMP/f10b-seed
	dd if=$TMP/f10b-seed of=$DIR1/f10b bs=3k count=1 || error "dd $DIR1"

	$TRUNCATE $DIR1/f10b 4096 || error "truncate 4096"

	dd if=$DIR2/f10b of=$TMP/f10b-swgfs bs=4k count=1 || error "dd $DIR2"

	# create a test file locally to compare
	dd if=$TMP/f10b-seed of=$TMP/f10b bs=3k count=1 || error "dd random"
	$TRUNCATE $TMP/f10b 4096 || error "truncate 4096"
	cmp $TMP/f10b $TMP/f10b-swgfs || error "file miscompare"
	rm $TMP/f10b $TMP/f10b-swgfs $TMP/f10b-seed
}
run_test 10b "write of file with sub-page size on multiple mounts "


test_11() {
	mkdir $DIR1/d11
	multiop_bg_pause $DIR1/d11/f O_c || return 1
	MULTIPID=$!
	cp -p /bin/ls $DIR1/d11/f
	$DIR2/d11/f
	RC=$?
	kill -USR1 $MULTIPID
	wait $MULTIPID || error
	[ $RC -eq 0 ] && error "execution succeeded unexpectly" || true
}
run_test 11 "execution of file opened for write should return error ===="

test_12() {
	DIR=$DIR DIR2=$DIR2 sh lockorder.sh
}

# So many operations, log will overflow
FORMER_DETECT_LEAK="$DETECT_LEAK"
DETECT_LEAK="no"
run_test 12 "test lock ordering (link, stat, unlink) ==========="
sleep 3
lctl debug_kernel > /dev/null
sleep 3
lctl debug_kernel > /dev/null
DETECT_LEAK="$FORMER_DETECT_LEAK"

test_13() {
	rm -rf $DIR1/d13
	mkdir $DIR1/d13 || error
	cd $DIR1/d13 || error
	ls
	( touch $DIR1/d13/f13 ) # needs to be a separate shell
	ls
	rm -f $DIR2/d13/f13 || error
	ls 2>&1 | grep f13 && error "f13 shouldn't return an error (1)" || true
	# need to run it twice
	( touch $DIR1/d13/f13 ) # needs to be a separate shell
	ls
	rm -f $DIR2/d13/f13 || error
	ls 2>&1 | grep f13 && error "f13 shouldn't return an error (2)" || true
}
run_test 13 "test directory page revocation ===================="

test_14() {
	mkdir -p $DIR1/$tdir
	cp -p /bin/ls $DIR1/$tdir/$tfile
	multiop_bg_pause $DIR1/$tdir/$tfile Ow_c || return 1
	MULTIPID=$!

	$DIR2/$tdir/$tfile && error || true
	kill -USR1 $MULTIPID
	wait $MULTIPID || return 2
}
run_test 14 "execution of file open for write returns -ETXTBSY ="

test_14a() {
	mkdir -p $DIR1/d14
	cp -p $MULTIOP $DIR1/d14/multiop || error "cp failed"
	MULTIOP_PROG=$DIR1/d14/multiop multiop_bg_pause $TMP/test14.junk O_c || return 1
	MULTIOP_PID=$!
	$MULTIOP $DIR2/d14/multiop Oc && error "expected error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	rm $TMP/test14.junk $DIR1/d14/multiop || error "removing multiop"
}
run_test 14a "open(RDWR) of executing file returns -ETXTBSY ===="

test_14x() {
	mkdir -p /mnt/swgfs/d14
	cp -p $MULTIOP /mnt/swgfs/d14/multiop || error "cp failed"
	MULTIOP_PROG=/mnt/swgfs/d14/multiop multiop_bg_pause $TMP/test14.junk O_c || return 1
	MULTIOP_PID=$!
	$TRUNCATE /mnt/swgfs2/d14/multiop 0 && kill -9 $MULTIOP_PID && \
		error "expected truncate error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	cmp $MULTIOP $DIR1/d14/multiop || error "binary changed"
	rm $TMP/test14.junk $DIR1/d14/multiop || error "removing multiop"
}
run_test 14x "truncate of executing file returns -ETXTBSY ======"

test_14b() {
	mkdir -p $DIR1/d14
	cp -p $MULTIOP $DIR1/d14/multiop || error "cp failed"
	MULTIOP_PROG=$DIR1/d14/multiop multiop_bg_pause $TMP/test14.junk O_c || return 1
	MULTIOP_PID=$!
	$TRUNCATE $DIR2/d14/multiop 0 && kill -9 $MULTIOP_PID && \
	error "expected truncate error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	cmp $MULTIOP $DIR1/d14/multiop || error "binary changed"
	rm $TMP/test14.junk $DIR1/d14/multiop || error "removing multiop"
}
run_test 14b "truncate of executing file returns -ETXTBSY ======"

test_14c() {
	mkdir -p $DIR1/d14
	cp -p $MULTIOP $DIR1/d14/multiop || error "cp failed"
	MULTIOP_PROG=$DIR1/d14/multiop multiop_bg_pause $TMP/test14.junk O_c || return 1
        MULTIOP_PID=$!
	cp /etc/hosts $DIR2/d14/multiop && error "expected error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	cmp $MULTIOP $DIR1/d14/multiop || error "binary changed"
	rm $TMP/test14.junk $DIR1/d14/multiop || error "removing multiop"
}
run_test 14c "open(O_TRUNC) of executing file return -ETXTBSY =="

test_14d() {
	mkdir -p $DIR1/d14
	cp -p $MULTIOP $DIR1/d14/multiop || error "cp failed"
	MULTIOP_PROG=$DIR1/d14/multiop multiop_bg_pause $TMP/test14.junk O_c || return 1
        MULTIOP_PID=$!
	log chmod
	chmod 600 $DIR1/d14/multiop || error "chmod failed"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	cmp $MULTIOP $DIR1/d14/multiop || error "binary changed"
	rm $TMP/test14.junk $DIR1/d14/multiop || error "removing multiop"
}
run_test 14d "chmod of executing file is still possible ========"

test_18() {
	# $MMAP_SANITY -d $HRFS_MNT1 -m $HRFS_MNT2
	$MMAP_SANITY -d $DIR1 -m $DIR2
	sync; sleep 1; sync
}
run_test 18 "mmap sanity check ================================="

cleanup_21() {
	trap 0
	umount $DIR1/d21
}

test_21() { # Bug 5907
	mkdir $DIR1/d21
	mount /etc $DIR1/d21 --bind || error "mount failed" # Poor man's mount.
	trap cleanup_21 EXIT
	rmdir -v $DIR1/d21 && error "Removed mounted directory"
	rmdir -v $DIR2/d21 && echo "Removed mounted directory from another mountpoint, needs to be fixed"
	test -d $DIR1/d21 || error "Mounted directory disappeared"
	cleanup_21
	test -d $DIR2/d21 || test -d $DIR1/d21 && error "Removed dir still visible after umount"
	true
}
run_test 21 " Try to remove mountpoint on another dir ===="

test_23() { # Bug 5972
	echo "others should see updated atime while another read" > $DIR1/f23

	# clear the lock(mode: LCK_PW) gotten from creating operation
	#cancel_lru_locks osc

	time1=`date +%s`
	sleep 2

	multiop_bg_pause $DIR1/f23 or20_c || return 1
	MULTIPID=$!

	time2=`stat -c "%X" $DIR2/f23`

	if (( $time2 <= $time1 )); then
		kill -USR1 $MULTIPID
		error "atime doesn't update among nodes"
	fi

	kill -USR1 $MULTIPID || return 1
	rm -f $DIR1/f23 || error "rm -f $DIR1/f23 failed"
	true
}
run_test 23 " others should see updated atime while another read===="

test_25() {
	#[ `lctl get_param -n mdc.*-mdc-*.connect_flags | grep -c acl` -lt 2 ] && \
	#    skip "must have acl, skipping" && return

	mkdir -p $DIR1/$tdir
	touch $DIR1/$tdir/f1 || error "touch $DIR1/$tdir/f1"
	chmod 0755 $DIR1/$tdir/f1 || error "chmod 0755 $DIR1/$tdir/f1"

	$RUNAS $CHECKSTAT $DIR2/$tdir/f1 || error "checkstat $DIR2/$tdir/f1 #1"
	setfacl -m u:$RUNAS_ID:--- $DIR1/$tdir || error "setfacl $DIR1/$tdir #1"
	$RUNAS $CHECKSTAT $DIR2/$tdir/f1 && error "checkstat $DIR2/$tdir/f1 #2"
	setfacl -m u:$RUNAS_ID:r-x $DIR1/$tdir || error "setfacl $DIR1/$tdir #2"
	$RUNAS $CHECKSTAT $DIR2/$tdir/f1 || error "checkstat $DIR2/$tdir/f1 #3"
	setfacl -m u:$RUNAS_ID:--- $DIR1/$tdir || error "setfacl $DIR1/$tdir #3"
	$RUNAS $CHECKSTAT $DIR2/$tdir/f1 && error "checkstat $DIR2/$tdir/f1 #4"
	setfacl -x u:$RUNAS_ID: $DIR1/$tdir || error "setfacl $DIR1/$tdir #4"
	$RUNAS $CHECKSTAT $DIR2/$tdir/f1 || error "checkstat $DIR2/$tdir/f1 #5"

	rm -rf $DIR1/$tdir
}
run_test 25 "change ACL on one mountpoint be seen on another ==="

test_26a() {
	$UTIME $DIR1/f26a -s $DIR2/f26a || error
}
run_test 26a "allow mtime to get older"

test_26b() {
	touch $DIR1/$tfile
	sleep 1
	echo "aaa" >> $DIR1/$tfile
	sleep 1
	chmod a+x $DIR2/$tfile
	mt1=`stat -c %Y $DIR1/$tfile`
	mt2=`stat -c %Y $DIR2/$tfile`

	if [ x"$mt1" != x"$mt2" ]; then
		error "not equal mtime, client1: "$mt1", client2: "$mt2"."
	fi
}
run_test 26b "sync mtime between ost and mds"

test_30() { #bug #11110
    mkdir -p $DIR1/$tdir
    cp -f /bin/bash $DIR1/$tdir/bash
    /bin/sh -c 'sleep 1; rm -f $DIR2/$tdir/bash; cp /bin/bash $DIR2/$tdir' &
    err=$($DIR1/$tdir/bash -c 'sleep 2; openfile -f O_RDONLY /proc/$$/exe >& /dev/null; echo $?')
    wait
    [ $err -ne 116 ] && error_ignore 12900 "return code ($err) != -ESTALE" && return
    true
}
run_test 30 "recreate file race ========="

test_39() {
        local originaltime
        local updatedtime
        local delay=3

        touch $DIR1/$tfile
        originaltime=$(stat -c %Y $DIR1/$tfile)
        log "original modification time is $originaltime"
        sleep $delay
        $MULTIOP $DIR1/$tfile oO_DIRECT:O_WRONLY:w$((10*1048576))c || error "multiop has failed"
        updatedtime=$(stat -c %Y $DIR2/$tfile)
        log "updated modification time is $updatedtime"
        [ $((updatedtime - originaltime)) -ge $delay ] || error "invalid modification time"
        rm -rf $DIR/$tfile
}
run_test 39 "direct I/O writes should update mtime ========="

cleanup_all
DOING_MUTLTI="no"
echo "=== $0: completed ==="
