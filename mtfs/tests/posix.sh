#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

LOWERFS_BUG_251="52b"
BUG_242="32 32a 32b 32s 37"
BUG_304="212 54c"
LOWERFS_BUG_305="34b 34c"
BUG_318="39k"
EXCEPT_SLOW="24v 51a 51b 51c 501"

export CONFIGS=${CONFIGS:-local}
. $TESTS_DIR/cfg/$CONFIGS.sh

if [ "$LOWERFS_STRICT_TIMESTAMP" = "no" ]; then
	LOWERFS_BUG_326="36a 36c 36d 39c"
fi

if [ "$LOWERFS_SUPPORT_CHATTR" = "no" ]; then
	LOWERFS_BUG_327="52a"
fi

if [ "$LOWERFS_NO_ATTR" = "yes" ]; then
	LOWERFS_SKIP_ATTR="0b 5 6 7a 8 11 12 19c 22 24m 28 33 35a 36e 39 40 52a"
fi

if [ "$LOWERFS_NO_MMAP" = "yes" ]; then
	LOWERFS_SKIP_NOMMAP="61 99"
fi

if [ "$LOWERFS_MMAP_NO_WRITE" = "yes" ]; then
	LOWERFS_SKIP_MMAP_NO_WRITE="43a 43b"
fi

if [ "$LOWERFS_NO_XATTR" = "yes" ]; then
	LOWERFS_SKIP_NO_XATTR="102a"
fi

ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$BUG_318 $LOWERFS_BUG_251 $BUG_242 $BUG_304
                                $LOWERFS_BUG_305 $LOWERFS_BUG_326 $LOWERFS_BUG_327
                                $LOWERFS_SKIP_ATTR $LOWERFS_SKIP_NOMMAP
                                $LOWERFS_SKIP_MMAP_NO_WRITE $POSIX_EXCEPT
                                $LOWERFS_SKIP_NO_XATTR"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env
echo "==== $0: started ===="

umask 077
test_0()
{
	touch $DIR/$tfile
	$CHECKSTAT -t file $DIR/$tfile || error
	rm $DIR/$tfile
	$CHECKSTAT -a $DIR/$tfile || error
}
run_test 0 "touch .../$tfile ; rm .../$tfile ====================="

test_0b() {
	chmod 0755 $DIR || error
	$CHECKSTAT -p 0755 $DIR || error
}
run_test 0b "chmod 0755 $DIR ============================="

test_1a() {
	mkdir $DIR/d1
	mkdir $DIR/d1/d2
	$CHECKSTAT -t dir $DIR/d1/d2 || error
}
run_test 1a "mkdir .../d1; mkdir .../d1/d2 ====================="

test_1b() {
	rmdir $DIR/d1/d2
	rmdir $DIR/d1
	$CHECKSTAT -a $DIR/d1 || error
}
run_test 1b "rmdir .../d1/d2; rmdir .../d1 ====================="

test_2a() {
	mkdir $DIR/d2
	touch $DIR/d2/f
	$CHECKSTAT -t file $DIR/d2/f || error
}
run_test 2a "mkdir .../d2; touch .../d2/f ======================"

test_2b() {
	rm -r $DIR/d2
	$CHECKSTAT -a $DIR/d2 || error
}
run_test 2b "rm -r .../d2; checkstat .../d2/f ======================"

test_3a() {
	mkdir $DIR/d3
	$CHECKSTAT -t dir $DIR/d3 || error
}
run_test 3a "mkdir .../d3 ======================================"

test_3b() {
	if [ ! -d $DIR/d3 ]; then
		mkdir $DIR/d3
	fi
	touch $DIR/d3/f
	$CHECKSTAT -t file $DIR/d3/f || error
}
run_test 3b "touch .../d3/f ===================================="

test_3c() {
	rm -r $DIR/d3
	$CHECKSTAT -a $DIR/d3 || error
}
run_test 3c "rm -r .../d3 ======================================"

test_4a() {
	mkdir $DIR/d4
	$CHECKSTAT -t dir $DIR/d4 || error
}
run_test 4a "mkdir .../d4 ======================================"

test_4b() {
	if [ ! -d $DIR/d4 ]; then
		mkdir $DIR/d4
	fi
	mkdir $DIR/d4/d2
	$CHECKSTAT -t dir $DIR/d4/d2 || error
}
run_test 4b "mkdir .../d4/d2 ==================================="

test_5() {
	mkdir $DIR/d5
	mkdir $DIR/d5/d2
	chmod 0707 $DIR/d5/d2
	$CHECKSTAT -t dir -p 0707 $DIR/d5/d2 || error
}
run_test 5 "mkdir .../d5 .../d5/d2; chmod .../d5/d2 ============"

test_6a() {
	touch $DIR/f6a
	chmod 0666 $DIR/f6a || error
	$CHECKSTAT -t file -p 0666 -u \#$UID $DIR/f6a || error
}
run_test 6a "touch .../f6a; chmod .../f6a ======================"

test_6b() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	if [ ! -f $DIR/f6a ]; then
		touch $DIR/f6a
		chmod 0666 $DIR/f6a
	fi
	$RUNAS chmod 0444 $DIR/f6a && error "unexpected chmod success"
	$CHECKSTAT -t file -p 0666 -u \#$UID $DIR/f6a || error "checkstat failed"
}
run_test 6b "$RUNAS chmod .../f6a (should return error) =="

test_6c() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	touch $DIR/f6c
	chown $RUNAS_ID $DIR/f6c || error
	$CHECKSTAT -t file -u \#$RUNAS_ID $DIR/f6c || error
}
run_test 6c "touch .../f6c; chown .../f6c ======================"

test_6d() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	if [ ! -f $DIR/f6c ]; then
		touch $DIR/f6c
		chown $RUNAS_ID $DIR/f6c
	fi
	$RUNAS chown $UID $DIR/f6c && error
	$CHECKSTAT -t file -u \#$RUNAS_ID $DIR/f6c || error
}
run_test 6d "$RUNAS chown .../f6c (should return error) =="

test_6e() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	touch $DIR/f6e
	chgrp $RUNAS_ID $DIR/f6e || error
	echo "$CHECKSTAT -t file -u \#$UID -g \#$RUNAS_ID $DIR/f6e "
	$CHECKSTAT -t file -u \#$UID -g \#$RUNAS_ID $DIR/f6e || error
}
run_test 6e "touch .../f6e; chgrp .../f6e ======================"

test_6f() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	if [ ! -f $DIR/f6e ]; then
		touch $DIR/f6e
		chgrp $RUNAS_ID $DIR/f6e
	fi
	$RUNAS chgrp $UID $DIR/f6e && error
	$CHECKSTAT -t file -u \#$UID -g \#$RUNAS_ID $DIR/f6e || error
}
run_test 6f "$RUNAS chgrp .../f6e (should return error) =="

test_6g() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	mkdir $DIR/d6g || error
	chmod 777 $DIR/d6g || error
	$RUNAS mkdir $DIR/d6g/d || error
	chmod g+s $DIR/d6g/d || error
	mkdir $DIR/d6g/d/subdir
	$CHECKSTAT -g \#$RUNAS_ID $DIR/d6g/d/subdir || error
}
run_test 6g "Is new dir in sgid dir inheriting group?"

test_6h() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	touch $DIR/f6h || error "touch failed"
	chown $RUNAS_ID:$RUNAS_ID $DIR/f6h || error "initial chown failed"
	$RUNAS -G$RUNAS_ID chown $RUNAS_ID:0 $DIR/f6h && error "chown worked"
	$CHECKSTAT -t file -u \#$RUNAS_ID -g \#$RUNAS_ID $DIR/f6h || error
}
run_test 6h "$RUNAS chown RUNAS_ID.0 .../f6h (should return error)"

test_7a() {
	mkdir $DIR/d7
	$MCREATE $DIR/d7/f
	chmod 0666 $DIR/d7/f
	$CHECKSTAT -t file -p 0666 $DIR/d7/f || error
}
run_test 7a "mkdir .../d7; mcreate .../d7/f; chmod .../d7/f ===="

test_7b() {
	if [ ! -d $DIR/d7 ]; then
		mkdir $DIR/d7
	fi
	$MCREATE $DIR/d7/f2
	echo -n foo > $DIR/d7/f2
	[ "`cat $DIR/d7/f2`" = "foo" ] || error
	$CHECKSTAT -t file -s 3 $DIR/d7/f2 || error
}
run_test 7b "mkdir .../d7; mcreate d7/f2; echo foo > d7/f2 ====="

test_8() {
	mkdir $DIR/d8
	touch $DIR/d8/f
	chmod 0666 $DIR/d8/f
	$CHECKSTAT -t file -p 0666 $DIR/d8/f || error
}
run_test 8 "mkdir .../d8; touch .../d8/f; chmod .../d8/f ======="

test_9() {
	mkdir $DIR/d9
	mkdir $DIR/d9/d2
	mkdir $DIR/d9/d2/d3
	$CHECKSTAT -t dir $DIR/d9/d2/d3 || error
}
run_test 9 "mkdir .../d9 .../d9/d2 .../d9/d2/d3 ================"

test_10() {
	mkdir $DIR/d10
	mkdir $DIR/d10/d2
	touch $DIR/d10/d2/f
	$CHECKSTAT -t file $DIR/d10/d2/f || error
}
run_test 10 "mkdir .../d10 .../d10/d2; touch .../d10/d2/f ======"

test_11() {
	mkdir $DIR/d11
	mkdir $DIR/d11/d2
	chmod 0666 $DIR/d11/d2
	chmod 0705 $DIR/d11/d2
	$CHECKSTAT -t dir -p 0705 $DIR/d11/d2 || error
}
run_test 11 "mkdir .../d11 d11/d2; chmod .../d11/d2 ============"

test_12() {
	mkdir $DIR/d12
	touch $DIR/d12/f
	chmod 0666 $DIR/d12/f
	chmod 0654 $DIR/d12/f
	$CHECKSTAT -t file -p 0654 $DIR/d12/f || error
}
run_test 12 "touch .../d12/f; chmod .../d12/f .../d12/f ========"

test_13() {
	mkdir $DIR/d13
	dd if=/dev/zero of=$DIR/d13/f count=10
	>  $DIR/d13/f
	$CHECKSTAT -t file -s 0 $DIR/d13/f || error
}
run_test 13 "creat .../d13/f; dd .../d13/f; > .../d13/f ========"

test_14() {
	mkdir $DIR/d14
	touch $DIR/d14/f
	rm $DIR/d14/f
	$CHECKSTAT -a $DIR/d14/f || error
}
run_test 14 "touch .../d14/f; rm .../d14/f; rm .../d14/f ======="

test_15() {
	mkdir $DIR/d15
	touch $DIR/d15/f
	mv $DIR/d15/f $DIR/d15/f2
	$CHECKSTAT -t file $DIR/d15/f2 || error
}
run_test 15 "touch .../d15/f; mv .../d15/f .../d15/f2 =========="

test_16() {
	mkdir $DIR/d16
	touch $DIR/d16/f
	rm -rf $DIR/d16/f
	$CHECKSTAT -a $DIR/d16/f || error
}
run_test 16 "touch .../d16/f; rm -rf .../d16/f ================="

test_17a() {
	mkdir -p $DIR/d17
	touch $DIR/d17/f
	ln -s $DIR/d17/f $DIR/d17/l-exist
	ls -l $DIR/d17
	$CHECKSTAT -l $DIR/d17/f $DIR/d17/l-exist || error
	$CHECKSTAT -f -t f $DIR/d17/l-exist || error
	rm -f $DIR/d17/l-exist
	$CHECKSTAT -a $DIR/d17/l-exist || error
}
run_test 17a "symlinks: create, remove (real) =================="

test_17b() {
	mkdir -p $DIR/d17
	ln -s no-such-file $DIR/d17/l-dangle
	ls -l $DIR/d17
	$CHECKSTAT -l no-such-file $DIR/d17/l-dangle || error
	$CHECKSTAT -fa $DIR/d17/l-dangle || error
	rm -f $DIR/d17/l-dangle
	$CHECKSTAT -a $DIR/d17/l-dangle || error
}
run_test 17b "symlinks: create, remove (dangling) =============="

test_17c() { # bug 3440 - don't save failed open RPC for replay
	mkdir -p $DIR/d17
	ln -s foo $DIR/d17/f17c
	cat $DIR/d17/f17c && error "opened non-existent symlink" || true
}
run_test 17c "symlinks: open dangling (should return error) ===="

test_17d() {
	mkdir -p $DIR/d17
	ln -s foo $DIR/d17/f17d
	touch $DIR/d17/f17d || error "creating to new symlink"
}
run_test 17d "symlinks: create dangling ========================"

test_17e() {
	mkdir -p $DIR/$tdir
	local foo=$DIR/$tdir/$tfile
	ln -s $foo $foo || error "create symlink failed"
	ls -l $foo || error "ls -l failed"
	ls $foo && error "ls not failed" || true
}
run_test 17e "symlinks: create recursive symlink (should return error) ===="

test_17f() {
	mkdir -p $DIR/d17f
	ln -s 1234567890/2234567890/3234567890/4234567890 $DIR/d17f/111
	ln -s 1234567890/2234567890/3234567890/4234567890/5234567890/6234567890 $DIR/d17f/222
	ln -s 1234567890/2234567890/3234567890/4234567890/5234567890/6234567890/7234567890/8234567890 $DIR/d17f/333
	ln -s 1234567890/2234567890/3234567890/4234567890/5234567890/6234567890/7234567890/8234567890/9234567890/a234567890/b234567890 $DIR/d17f/444
	ln -s 1234567890/2234567890/3234567890/4234567890/5234567890/6234567890/7234567890/8234567890/9234567890/a234567890/b234567890/c234567890/d234567890/f234567890 $DIR/d17f/555
	ln -s 1234567890/2234567890/3234567890/4234567890/5234567890/6234567890/7234567890/8234567890/9234567890/a234567890/b234567890/c234567890/d234567890/f234567890/aaaaaaaaaa/bbbbbbbbbb/cccccccccc/dddddddddd/eeeeeeeeee/ffffffffff/ $DIR/d17f/666
	ls -l  $DIR/d17f
}
run_test 17f "symlinks: long and very long symlink name ========================"

test_17g() {
	mkdir -p $DIR/$tdir
	LONGSYMLINK="$(dd if=/dev/zero bs=4095 count=1 | tr '\0' 'x')"
	ln -s $LONGSYMLINK $DIR/$tdir/$tfile
	ls -l $DIR/$tdir
}
run_test 17g "symlinks: really long symlink name ==============================="

#test_17h() { #bug 17378
#	mkdir -p $DIR/$tdir
#	$SETSTRIPE $DIR/$tdir -c -1
##define OBD_FAIL_MDS_LOV_PREP_CREATE 0x141
#	do_facet mds lctl set_param fail_loc=0x80000141
#        touch $DIR/$tdir/$tfile || true
#}
#run_test 17h "create objects: lov_free_memmd() doesn't lbug"

test_18() {
	touch $DIR/f
	ls $DIR || error
}
run_test 18 "touch .../f ; ls ... =============================="

test_19a() {
	touch $DIR/f19
	ls -l $DIR
	rm $DIR/f19
	$CHECKSTAT -a $DIR/f19 || error
}
run_test 19a "touch .../f19 ; ls -l ... ; rm .../f19 ==========="

test_19b() {
	ls -l $DIR/f19 && error || true
}
run_test 19b "ls -l .../f19 (should return error) =============="

test_19c() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	$RUNAS touch $DIR/f19 && error || true
}
run_test 19c "$RUNAS touch .../f19 (should return error) =="

test_19d() {
	cat $DIR/f19 && error || true
}
run_test 19d "cat .../f19 (should return error) =============="

test_20() {
	touch $DIR/f
	rm $DIR/f
	log "1 done"
	touch $DIR/f
	rm $DIR/f
	log "2 done"
	touch $DIR/f
	rm $DIR/f
	log "3 done"
	$CHECKSTAT -a $DIR/f || error
}
run_test 20 "touch .../f ; ls -l ... ==========================="

test_21() {
	mkdir $DIR/d21
	[ -f $DIR/d21/dangle ] && rm -f $DIR/d21/dangle
	ln -s dangle $DIR/d21/link
	echo foo >> $DIR/d21/link
	cat $DIR/d21/dangle
	$CHECKSTAT -t link $DIR/d21/link || error
	$CHECKSTAT -f -t file $DIR/d21/link || error
}
run_test 21 "write to dangling link ============================"

test_22() {
	WDIR=$DIR/$tdir
	mkdir -p $WDIR
	chown $RUNAS_ID $WDIR
	(cd $WDIR || error "cd $WDIR failed";
	$RUNAS tar cf - /etc/hosts /etc/sysconfig/network | \
	$RUNAS tar xf -)
	ls -lR $WDIR/etc || error "ls -lR $WDIR/etc failed"
	$CHECKSTAT -t dir $WDIR/etc || error "checkstat -t dir failed"
	$CHECKSTAT -u \#$RUNAS_ID $WDIR/etc || error "checkstat -u failed"
}
run_test 22 "unpack tar archive as non-root user ==============="

# was test_23
test_23a() {
	mkdir -p $DIR/$tdir
	local file=$DIR/$tdir/$tfile

	$OPENFILE -f O_CREAT:O_EXCL $file || error "$file create failed"
	$OPENFILE -f O_CREAT:O_EXCL $file &&
		error "$file recreate succeeded" || true
}
run_test 23a "O_CREAT|O_EXCL in subdir =========================="

test_23b() {
	mkdir -p $DIR/$tdir
	local file=$DIR/$tdir/$tfile

	rm -f $file
	echo foo > $file || error "write filed"
	echo bar >> $file || error "append filed"
	$CHECKSTAT -s 8 $file || error "wrong size"
	rm $file
}
run_test 23b "O_APPEND check =========================="

test_24a() {
	echo '== rename sanity =============================================='
	echo '-- same directory rename'
	mkdir $DIR/R1
	touch $DIR/R1/f
	mv $DIR/R1/f $DIR/R1/g
	$CHECKSTAT -t file $DIR/R1/g || error
}
run_test 24a "touch .../R1/f; rename .../R1/f .../R1/g ========="

test_24b() {
	mkdir $DIR/R2
	touch $DIR/R2/{f,g}
	mv $DIR/R2/f $DIR/R2/g
	$CHECKSTAT -a $DIR/R2/f || error
	$CHECKSTAT -t file $DIR/R2/g || error
}
run_test 24b "touch .../R2/{f,g}; rename .../R2/f .../R2/g ====="

test_24c() {
	mkdir $DIR/R3
	mkdir $DIR/R3/f
	mv $DIR/R3/f $DIR/R3/g
	$CHECKSTAT -a $DIR/R3/f || error
	$CHECKSTAT -t dir $DIR/R3/g || error
}
run_test 24c "mkdir .../R3/f; rename .../R3/f .../R3/g ========="

test_24d() {
	mkdir $DIR/R4
	mkdir $DIR/R4/{f,g}
	$MRENAME $DIR/R4/f $DIR/R4/g
	$CHECKSTAT -a $DIR/R4/f || error
	$CHECKSTAT -t dir $DIR/R4/g || error
}
run_test 24d "mkdir .../R4/{f,g}; rename .../R4/f .../R4/g ====="

test_24e() {
	echo '-- cross directory renames --'
	mkdir $DIR/R5{a,b}
	touch $DIR/R5a/f
	mv $DIR/R5a/f $DIR/R5b/g
	$CHECKSTAT -a $DIR/R5a/f || error
	$CHECKSTAT -t file $DIR/R5b/g || error
}
run_test 24e "touch .../R5a/f; rename .../R5a/f .../R5b/g ======"

test_24f() {
	mkdir $DIR/R6{a,b}
	touch $DIR/R6a/f $DIR/R6b/g
	mv $DIR/R6a/f $DIR/R6b/g
	$CHECKSTAT -a $DIR/R6a/f || error
	$CHECKSTAT -t file $DIR/R6b/g || error
}
run_test 24f "touch .../R6a/f R6b/g; mv .../R6a/f .../R6b/g ===="

test_24g() {
	mkdir $DIR/R7{a,b}
	mkdir $DIR/R7a/d
	mv $DIR/R7a/d $DIR/R7b/e
	$CHECKSTAT -a $DIR/R7a/d || error
	$CHECKSTAT -t dir $DIR/R7b/e || error
}
run_test 24g "mkdir .../R7{a,b}/d; mv .../R7a/d .../R5b/e ======"

test_24h() {
	mkdir $DIR/R8{a,b}
	mkdir $DIR/R8a/d $DIR/R8b/e
	$MRENAME $DIR/R8a/d $DIR/R8b/e
	$CHECKSTAT -a $DIR/R8a/d || error
	$CHECKSTAT -t dir $DIR/R8b/e || error
}
run_test 24h "mkdir .../R8{a,b}/{d,e}; rename .../R8a/d .../R8b/e"

test_24i() {
	echo "-- rename error cases"
	mkdir $DIR/R9
	mkdir $DIR/R9/a
	touch $DIR/R9/f
	$MRENAME $DIR/R9/f $DIR/R9/a
	$CHECKSTAT -t file $DIR/R9/f || error
	$CHECKSTAT -t dir  $DIR/R9/a || error
	$CHECKSTAT -a $DIR/R9/a/f || error
}
run_test 24i "rename file to dir error: touch f ; mkdir a ; rename f a"

test_24j() {
	mkdir $DIR/R10
	$MRENAME $DIR/R10/f $DIR/R10/g
	$CHECKSTAT -t dir $DIR/R10 || error
	$CHECKSTAT -a $DIR/R10/f || error
	$CHECKSTAT -a $DIR/R10/g || error
}
run_test 24j "source does not exist ============================"

test_24k() {
	mkdir $DIR/R11a $DIR/R11a/d
	touch $DIR/R11a/f
	mv $DIR/R11a/f $DIR/R11a/d
	$CHECKSTAT -a $DIR/R11a/f || error
	$CHECKSTAT -t file $DIR/R11a/d/f || error
}
run_test 24k "touch .../R11a/f; mv .../R11a/f .../R11a/d ======="

test_24l() {
	f="$DIR/f24l"
	$MULTIOP $f OcNs || error
}
run_test 24l "Renaming a file to itself ========================"

test_24m() {
	f="$DIR/f24m"
	$MULTIOP $f OcLN ${f}2 ${f}2 || error "link ${f}2 ${f}2 failed"
	# on ext3 this does not remove either the source or target files
	# though the "expected" operation would be to remove the source
	$CHECKSTAT -t file ${f} || error "${f} missing"
	$CHECKSTAT -t file ${f}2 || error "${f}2 missing"
}
run_test 24m "Renaming a file to a hard link to itself ========="

test_24n() {
    f="$DIR/f24n"
    # this stats the old file after it was renamed, so it should fail
    touch ${f}
    $CHECKSTAT ${f}
    mv ${f} ${f}.rename
    $CHECKSTAT ${f}.rename
    $CHECKSTAT -a ${f}
}
run_test 24n "Statting the old file after renaming (Posix rename 2)"

test_24o() {
	#check_kernel_version 37 || return 0
	mkdir -p $DIR/d24o
	$RENAME_MANY -s random -v -n 10 $DIR/d24o
}
leak_detect_state_push "no"
run_test 24o "rename of files during htree split ==============="
leak_detect_state_pop

test_24p() {
	mkdir $DIR/R12{a,b}
	DIRINO=`ls -lid $DIR/R12a | awk '{ print $1 }'`
	$MRENAME $DIR/R12a $DIR/R12b
	$CHECKSTAT -a $DIR/R12a || error
	$CHECKSTAT -t dir $DIR/R12b || error
	DIRINO2=`ls -lid $DIR/R12b | awk '{ print $1 }'`
	[ "$DIRINO" = "$DIRINO2" ] || error "R12a $DIRINO != R12b $DIRINO2"
}
run_test 24p "mkdir .../R12{a,b}; rename .../R12a .../R12b"

test_24q() {
	mkdir $DIR/R13{a,b}
	DIRINO=`ls -lid $DIR/R13a | awk '{ print $1 }'`
	multiop_bg_pause $DIR/R13b D_c || return 1
	MULTIPID=$!

	$MRENAME $DIR/R13a $DIR/R13b
	$CHECKSTAT -a $DIR/R13a || error
	$CHECKSTAT -t dir $DIR/R13b || error
	DIRINO2=`ls -lid $DIR/R13b | awk '{ print $1 }'`
	[ "$DIRINO" = "$DIRINO2" ] || error "R13a $DIRINO != R13b $DIRINO2"
	kill -USR1 $MULTIPID
	wait $MULTIPID || error "multiop close failed"
}
run_test 24q "mkdir .../R13{a,b}; open R13b rename R13a R13b ==="

test_24r() {
	mkdir $DIR/R14a $DIR/R14a/b
	$MRENAME $DIR/R14a $DIR/R14a/b && error "rename to subdir worked!"
	$CHECKSTAT -t dir $DIR/R14a || error "$DIR/R14a missing"
	$CHECKSTAT -t dir $DIR/R14a/b || error "$DIR/R14a/b missing"
}
run_test 24r "mkdir .../R14a/b; rename .../R14a .../R14a/b ====="

test_24s() {
	mkdir $DIR/R15a $DIR/R15a/b $DIR/R15a/b/c
	$MRENAME $DIR/R15a $DIR/R15a/b/c && error "rename to sub-subdir worked!"
	$CHECKSTAT -t dir $DIR/R15a || error "$DIR/R15a missing"
	$CHECKSTAT -t dir $DIR/R15a/b/c || error "$DIR/R15a/b/c missing"
}
run_test 24s "mkdir .../R15a/b/c; rename .../R15a .../R15a/b/c ="
test_24t() {
	mkdir $DIR/R16a $DIR/R16a/b $DIR/R16a/b/c
	$MRENAME $DIR/R16a/b/c $DIR/R16a && error "rename to sub-subdir worked!"
	$CHECKSTAT -t dir $DIR/R16a || error "$DIR/R16a missing"
	$CHECKSTAT -t dir $DIR/R16a/b/c || error "$DIR/R16a/b/c missing"
}
run_test 24t "mkdir .../R16a/b/c; rename .../R16a/b/c .../R16a ="

test_24u() {
	# was for create stripe file
	return 0
}
run_test 24u "do nothing"

test_24v() {
	local NRFILES=100000
#	local FREE_INODES=`lfs df -i|grep "filesystem summary" | awk '{print $5}'`
	local FREE_INODES=`df -i | grep $MTFS_MNT1 | awk {'print $4'}`
	[ $FREE_INODES -lt $NRFILES ] && \
		skip "not enough free inodes $FREE_INODES required $NRFILES" && \
		return

	mkdir -p $DIR/d24v
	$CREATEMANY -m $DIR/d24v/$tfile $NRFILES
	ls $DIR/d24v >/dev/null || error "error in listing large dir"

	rm $DIR/d24v -rf
}
run_test 24v "list directory with large files (handle hash collision, bug: 17560)"

test_25a() {
	echo '== symlink sanity ============================================='

	mkdir $DIR/d25
	ln -s d25 $DIR/s25
	touch $DIR/s25/foo || error
}
run_test 25a "create file in symlinked directory ==============="

test_25b() {
	[ ! -d $DIR/d25 ] && test_25a
	$CHECKSTAT -t file $DIR/s25/foo || error
}
run_test 25b "lookup file in symlinked directory ==============="

test_26a() {
	mkdir $DIR/d26
	mkdir $DIR/d26/d26-2
	ln -s d26/d26-2 $DIR/s26
	touch $DIR/s26/foo || error
}
run_test 26a "multiple component symlink ======================="

test_26b() {
	mkdir -p $DIR/d26b/d26-2
	ln -s d26b/d26-2/foo $DIR/s26-2
	touch $DIR/s26-2 || error
}
run_test 26b "multiple component symlink at end of lookup ======"

test_26c() {
	mkdir $DIR/d26.2
	touch $DIR/d26.2/foo
	ln -s d26.2 $DIR/s26.2-1
	ln -s s26.2-1 $DIR/s26.2-2
	ln -s s26.2-2 $DIR/s26.2-3
	chmod 0666 $DIR/s26.2-3/foo
}
run_test 26c "chain of symlinks ================================"

# recursive symlinks (bug 439)
test_26d() {
	ln -s d26-3/foo $DIR/d26-3
}
run_test 26d "create multiple component recursive symlink ======"

test_26e() {
	[ ! -h $DIR/d26-3 ] && test_26d
	rm $DIR/d26-3
}
run_test 26e "unlink multiple component recursive symlink ======"

# recursive symlinks (bug 7022)
test_26f() {
	mkdir -p $DIR/$tdir
	mkdir $DIR/$tdir/$tfile        || error "mkdir $DIR/$tdir/$tfile failed"
	cd $DIR/$tdir/$tfile           || error "cd $DIR/$tdir/$tfile failed"
	mkdir -p lndir/bar1      || error "mkdir lndir/bar1 failed"
	mkdir $tfile             || error "mkdir $tfile failed"
	cd $tfile                || error "cd $tfile failed"
	ln -s .. dotdot          || error "ln dotdot failed"
	ln -s dotdot/lndir lndir || error "ln lndir failed"
	cd $DIR/$tdir                 || error "cd $DIR/$tdir failed"
	output=`ls $tfile/$tfile/lndir/bar1`
	[ "$output" = bar1 ] && error "unexpected output"
	rm -r $tfile             || error "rm $tfile failed"
	$CHECKSTAT -a $DIR/$tfile || error "$tfile not gone"
}
run_test 26f "rm -r of a directory which has recursive symlink ="

test_27() {
	# series 27 was for setstripe and getstripe
	return 0
}
run_test 27 "do nothing ========================"

# createtest also checks that device nodes are created and
test_28() {
	mkdir $DIR/d28
	$CREATETEST $DIR/d28/ct || error
}
run_test 28 "create/mknod/mkdir with bad file types ============"

test_29() {
	return 0
}
run_test 29 "do nothing"

test_30() {
	cp /bin/ls $DIR
	$DIR/ls /
	rm $DIR/ls
}
run_test 30 "run binary from mtfs (execve) ==================="

test_31a() {
	$OPENUNLINK $DIR/f31 $DIR/f31 || error
	$CHECKSTAT -a $DIR/f31 || error
}
run_test 31a "open-unlink file =================================="

test_31b() {
	touch $DIR/f31 || error
	ln $DIR/f31 $DIR/f31b || error
	$MULTIOP $DIR/f31b Ouc || error
	$CHECKSTAT -t file $DIR/f31 || error
}
run_test 31b "unlink file with multiple links while open ======="

test_31c() {
	touch $DIR/f31 || error
	ln $DIR/f31 $DIR/f31c || error
	multiop_bg_pause $DIR/f31 O_uc || return 1
	MULTIPID=$!
	$MULTIOP $DIR/f31c Ouc
	kill -USR1 $MULTIPID
	wait $MULTIPID
}
run_test 31c "open-unlink file with multiple links ============="

test_31d() {
	if [ "$LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED" = "yes" ]; then
		echo "skiped"
	else
		$OPENDIRUNLINK $DIR/d31d $DIR/d31d || error
		$CHECKSTAT -a $DIR/d31d || error
	fi
}
run_test 31d "remove of open directory ========================="

test_31e() { # bug 88 
	#check_kernel_version 34 || return 0
	$OPENFILLEDDIRUNLINK $DIR/d31e || error
}
run_test 31e "remove of open non-empty directory ==============="

test_31f() { # bug 4554
	set -vx
	mkdir $DIR/d31f
	#$SETSTRIPE $DIR/d31f -s 1048576 -c 1
	cp /etc/hosts $DIR/d31f
	ls -l $DIR/d31f
	#$GETSTRIPE $DIR/d31f/hosts
	multiop_bg_pause $DIR/d31f D_c || return 1
	MULTIPID=$!

	rm -rv $DIR/d31f || error "first of $DIR/d31f"
	mkdir $DIR/d31f
	#$SETSTRIPE $DIR/d31f -s 1048576 -c 1
	cp /etc/hosts $DIR/d31f
	ls -l $DIR/d31f
	$DIR/d31f/hosts
	multiop_bg_pause $DIR/d31f D_c || return 1
	MULTIPID2=$!

	kill -USR1 $MULTIPID || error "first opendir $MULTIPID not running"
	wait $MULTIPID || error "first opendir $MULTIPID failed"

	sleep 6

	kill -USR1 $MULTIPID2 || error "second opendir $MULTIPID not running"
	wait $MULTIPID2 || error "second opendir $MULTIPID2 failed"
	set +vx
}
run_test 31f "remove of open directory with open-unlink file ==="

test_31g() {
	echo "-- cross directory link --"
	mkdir $DIR/d31g{a,b}
	touch $DIR/d31ga/f
	ln $DIR/d31ga/f $DIR/d31gb/g
	$CHECKSTAT -t file $DIR/d31ga/f || error "source"
	[ `stat -c%h $DIR/d31ga/f` == '2' ] || error "source nlink"
	$CHECKSTAT -t file $DIR/d31gb/g || error "target"
	[ `stat -c%h $DIR/d31gb/g` == '2' ] || error "target nlink"
}
run_test 31g "cross directory link==============="

test_31h() {
	echo "-- cross directory link --"
	mkdir $DIR/d31h
	mkdir $DIR/d31h/dir
	touch $DIR/d31h/f
	ln $DIR/d31h/f $DIR/d31h/dir/g
	$CHECKSTAT -t file $DIR/d31h/f || error "source"
	[ `stat -c%h $DIR/d31h/f` == '2' ] || error "source nlink"
	$CHECKSTAT -t file $DIR/d31h/dir/g || error "target"
	[ `stat -c%h $DIR/d31h/dir/g` == '2' ] || error "target nlink"
}
run_test 31h "cross directory link under child==============="

test_31i() {
	echo "-- cross directory link --"
	mkdir $DIR/d31i
	mkdir $DIR/d31i/dir
	touch $DIR/d31i/dir/f
	ln $DIR/d31i/dir/f $DIR/d31i/g
	$CHECKSTAT -t file $DIR/d31i/dir/f || error "source"
	[ `stat -c%h $DIR/d31i/dir/f` == '2' ] || error "source nlink"
	$CHECKSTAT -t file $DIR/d31i/g || error "target"
	[ `stat -c%h $DIR/d31i/g` == '2' ] || error "target nlink"
}
run_test 31i "cross directory link under parent==============="

test_31j() {
	mkdir $DIR/d31j
	mkdir $DIR/d31j/dir1
	ln $DIR/d31j/dir1 $DIR/d31j/dir2 && error "ln for dir"
	link $DIR/d31j/dir1 $DIR/d31j/dir3 && error "link for dir"
	$MLINK $DIR/d31j/dir1 $DIR/d31j/dir4 && error "mlink for dir"
	$MLINK $DIR/d31j/dir1 $DIR/d31j/dir1 && error "mlink to the same dir"
	return 0
}
run_test 31j "link for directory==============="

test_31k() {
	mkdir $DIR/d31k
	touch $DIR/d31k/s
	touch $DIR/d31k/exist
	$MLINK $DIR/d31k/s $DIR/d31k/t || error "mlink"
	$MLINK $DIR/d31k/s $DIR/d31k/exist && error "mlink to exist file"
	$MLINK $DIR/d31k/s $DIR/d31k/s && error "mlink to the same file"
	$MLINK $DIR/d31k/s $DIR/d31k && error "mlink to parent dir"
	$MLINK $DIR/d31k $DIR/d31k/s && error "mlink parent dir to target"
	$MLINK $DIR/d31k/not-exist $DIR/d31k/foo && error "mlink non-existing to new"
	$MLINK $DIR/d31k/not-exist $DIR/d31k/s && error "mlink non-existing to exist"
	return 0
}
run_test 31k "link to file: the same, non-existing, dir==============="

test_31m() {
	mkdir $DIR/d31m
	touch $DIR/d31m/s
	mkdir $DIR/d31m2
	touch $DIR/d31m2/exist
	$MLINK $DIR/d31m/s $DIR/d31m2/t || error "mlink"
	$MLINK $DIR/d31m/s $DIR/d31m2/exist && error "mlink to exist file"
	$MLINK $DIR/d31m/s $DIR/d31m2 && error "mlink to parent dir"
	$MLINK $DIR/d31m2 $DIR/d31m/s && error "mlink parent dir to target"
	$MLINK $DIR/d31m/not-exist $DIR/d31m2/foo && error "mlink non-existing to new"
	$MLINK $DIR/d31m/not-exist $DIR/d31m2/s && error "mlink non-existing to exist"
	return 0
}
run_test 31m "link to file: the same, non-existing, dir==============="

test_32x() {
	echo "== preparing for tests involving mounts ================="
	export EXT2_DEV=${EXT2_DEV:-$DIR/POSIX.LOOP}
	touch $EXT2_DEV
	mke2fs -j -F $EXT2_DEV 8000 > /dev/null || error
	echo # add a newline after mke2fs.
}
run_test 32x "mke2fs POSIX.LOOP ====================="

test_32a() {
	echo "== more mountpoints and symlinks ================="
	[ -e $DIR/d32a ] && rm -fr $DIR/d32a
	mkdir -p $DIR/d32a/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32a/ext2-mountpoint || error
	$CHECKSTAT -t dir $DIR/d32a/ext2-mountpoint/.. || error
	umount $DIR/d32a/ext2-mountpoint || error
}
run_test 32a "stat d32a/ext2-mountpoint/.. ====================="

test_32b() {
	[ -e $DIR/d32b ] && rm -fr $DIR/d32b
	mkdir -p $DIR/d32b/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32b/ext2-mountpoint || error
	ls -al $DIR/d32b/ext2-mountpoint/.. || error
	umount $DIR/d32b/ext2-mountpoint || error
}
run_test 32b "open d32b/ext2-mountpoint/.. ====================="

test_32c() {
	[ -e $DIR/d32c ] && rm -fr $DIR/d32c
	mkdir -p $DIR/d32c/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32c/ext2-mountpoint || error
	mkdir -p $DIR/d32c/d2/test_dir
	$CHECKSTAT -t dir $DIR/d32c/ext2-mountpoint/../d2/test_dir || error
	umount $DIR/d32c/ext2-mountpoint || error
}
run_test 32c "stat d32c/ext2-mountpoint/../d2/test_dir ========="

test_32d() {
	[ -e $DIR/d32d ] && rm -fr $DIR/d32d
	mkdir -p $DIR/d32d/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32d/ext2-mountpoint || error
	mkdir -p $DIR/d32d/d2/test_dir
	ls -al $DIR/d32d/ext2-mountpoint/../d2/test_dir || error
	umount $DIR/d32d/ext2-mountpoint || error
}
run_test 32d "open d32d/ext2-mountpoint/../d2/test_dir ========="

test_32e() {
	[ -e $DIR/d32e ] && rm -fr $DIR/d32e
	mkdir -p $DIR/d32e/tmp
	TMP_DIR=$DIR/d32e/tmp
	ln -s $DIR/d32e $TMP_DIR/symlink11
	ln -s $TMP_DIR/symlink11 $TMP_DIR/../symlink01
	$CHECKSTAT -t link $DIR/d32e/tmp/symlink11 || error
	$CHECKSTAT -t link $DIR/d32e/symlink01 || error
}
run_test 32e "stat d32e/symlink->tmp/symlink->mtfs-subdir ===="

test_32f() {
	[ -e $DIR/d32f ] && rm -fr $DIR/d32f
	mkdir -p $DIR/d32f/tmp
	TMP_DIR=$DIR/d32f/tmp
	ln -s $DIR/d32f $TMP_DIR/symlink11
	ln -s $TMP_DIR/symlink11 $TMP_DIR/../symlink01
	ls $DIR/d32f/tmp/symlink11  || error
	ls $DIR/d32f/symlink01 || error
}
run_test 32f "open d32f/symlink->tmp/symlink->mtfs-subdir ===="

test_32g() {
	TMP_DIR=$DIR/$tdir/tmp
	mkdir -p $TMP_DIR $DIR/${tdir}2
	ln -s $DIR/${tdir}2 $TMP_DIR/symlink12
	ln -s $TMP_DIR/symlink12 $TMP_DIR/../symlink02
	$CHECKSTAT -t link $TMP_DIR/symlink12 || error
	$CHECKSTAT -t link $DIR/$tdir/symlink02 || error
	$CHECKSTAT -t dir -f $TMP_DIR/symlink12 || error
	$CHECKSTAT -t dir -f $DIR/$tdir/symlink02 || error
}
run_test 32g "stat d32g/symlink->tmp/symlink->mtfs-subdir/${tdir}2"

test_32h() {
	rm -fr $DIR/$tdir $DIR/${tdir}2
	TMP_DIR=$DIR/$tdir/tmp
	mkdir -p $TMP_DIR $DIR/${tdir}2
	ln -s $DIR/${tdir}2 $TMP_DIR/symlink12
	ln -s $TMP_DIR/symlink12 $TMP_DIR/../symlink02
	ls $TMP_DIR/symlink12 || error
	ls $DIR/$tdir/symlink02  || error
}
run_test 32h "open d32h/symlink->tmp/symlink->mtfs-subdir/${tdir}2"

test_32i() {
	[ -e $DIR/d32i ] && rm -fr $DIR/d32i
	mkdir -p $DIR/d32i/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32i/ext2-mountpoint || error
	touch $DIR/d32i/test_file
	$CHECKSTAT -t file $DIR/d32i/ext2-mountpoint/../test_file || error
	umount $DIR/d32i/ext2-mountpoint || error
}
run_test 32i "stat d32i/ext2-mountpoint/../test_file ==========="

test_32j() {
	[ -e $DIR/d32j ] && rm -fr $DIR/d32j
	mkdir -p $DIR/d32j/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32j/ext2-mountpoint || error
	touch $DIR/d32j/test_file
	cat $DIR/d32j/ext2-mountpoint/../test_file || error
	umount $DIR/d32j/ext2-mountpoint || error
}
run_test 32j "open d32j/ext2-mountpoint/../test_file ==========="

test_32k() {
	rm -fr $DIR/d32k
	mkdir -p $DIR/d32k/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32k/ext2-mountpoint
	mkdir -p $DIR/d32k/d2
	touch $DIR/d32k/d2/test_file || error
	$CHECKSTAT -t file $DIR/d32k/ext2-mountpoint/../d2/test_file || error
	umount $DIR/d32k/ext2-mountpoint || error
}
run_test 32k "stat d32k/ext2-mountpoint/../d2/test_file ========"

test_32l() {
	rm -fr $DIR/d32l
	mkdir -p $DIR/d32l/ext2-mountpoint
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32l/ext2-mountpoint || error
	mkdir -p $DIR/d32l/d2
	touch $DIR/d32l/d2/test_file
	cat  $DIR/d32l/ext2-mountpoint/../d2/test_file || error
	umount $DIR/d32l/ext2-mountpoint || error
}
run_test 32l "open d32l/ext2-mountpoint/../d2/test_file ========"

test_32m() {
	rm -fr $DIR/d32m
	mkdir -p $DIR/d32m/tmp
	TMP_DIR=$DIR/d32m/tmp
	ln -s $DIR $TMP_DIR/symlink11
	ln -s $TMP_DIR/symlink11 $TMP_DIR/../symlink01
	$CHECKSTAT -t link $DIR/d32m/tmp/symlink11 || error
	$CHECKSTAT -t link $DIR/d32m/symlink01 || error
}
run_test 32m "stat d32m/symlink->tmp/symlink->mtfs-root ======"

test_32n() {
	rm -fr $DIR/d32n
	mkdir -p $DIR/d32n/tmp
	TMP_DIR=$DIR/d32n/tmp
	ln -s $DIR $TMP_DIR/symlink11
	ln -s $TMP_DIR/symlink11 $TMP_DIR/../symlink01
	ls -l $DIR/d32n/tmp/symlink11  || error
	ls -l $DIR/d32n/symlink01 || error
}
run_test 32n "open d32n/symlink->tmp/symlink->mtfs-root ======"

test_32o() {
	rm -fr $DIR/d32o $DIR/$tfile
	touch $DIR/$tfile
	mkdir -p $DIR/d32o/tmp
	TMP_DIR=$DIR/d32o/tmp
	ln -s $DIR/$tfile $TMP_DIR/symlink12
	ln -s $TMP_DIR/symlink12 $TMP_DIR/../symlink02
	$CHECKSTAT -t link $DIR/d32o/tmp/symlink12 || error
	$CHECKSTAT -t link $DIR/d32o/symlink02 || error
	$CHECKSTAT -t file -f $DIR/d32o/tmp/symlink12 || error
	$CHECKSTAT -t file -f $DIR/d32o/symlink02 || error
}
run_test 32o "stat d32o/symlink->tmp/symlink->mtfs-root/$tfile"

test_32p() {
    log 32p_1
	rm -fr $DIR/d32p
    log 32p_2
	rm -f $DIR/$tfile
    log 32p_3
	touch $DIR/$tfile
    log 32p_4
	mkdir -p $DIR/d32p/tmp
    log 32p_5
	TMP_DIR=$DIR/d32p/tmp
    log 32p_6
	ln -s $DIR/$tfile $TMP_DIR/symlink12
    log 32p_7
	ln -s $TMP_DIR/symlink12 $TMP_DIR/../symlink02
    log 32p_8
	cat $DIR/d32p/tmp/symlink12 || error
    log 32p_9
	cat $DIR/d32p/symlink02 || error
    log 32p_10
}
run_test 32p "open d32p/symlink->tmp/symlink->mtfs-root/$tfile"

test_32q() {
	[ -e $DIR/d32q ] && rm -fr $DIR/d32q
	mkdir -p $DIR/d32q
	touch $DIR/d32q/under_the_mount
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32q
	ls $DIR/d32q/under_the_mount && error || true
	umount $DIR/d32q || error
}
run_test 32q "stat follows mountpoints (should return error)"

test_32r() {
	[ -e $DIR/d32r ] && rm -fr $DIR/d32r
	mkdir -p $DIR/d32r
	touch $DIR/d32r/under_the_mount
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32r
	ls $DIR/d32r | grep -q under_the_mount && error || true
	umount $DIR/d32r || error
}
run_test 32r "opendir follows mountpoints (should return error)"

test_32s() {
	[ -e $DIR/d32s ] && rm -fr $DIR/d32s
	mkdir -p $DIR/d32s
	cleanup_and_setup
	mount -t ext2 -o loop $EXT2_DEV $DIR/d32s
	umount $DIR/d32s || error
}
run_test 32s "mount dev_file under MOUNT_POINT when not kown its length should not crash"

test_33() {
	rm -f $DIR/$tfile
	touch $DIR/$tfile
	chmod 444 $DIR/$tfile
	chown $RUNAS_ID $DIR/$tfile
	log 33_1
	echo "CMD: $RUNAS $OPENFILE -f O_RDWR $DIR/$tfile"
	$RUNAS $OPENFILE -f O_RDWR $DIR/$tfile && error || true
	log 33_2
}
run_test 33 "write file with mode 444 (should return error) ===="

test_33a() {
	rm -fr $DIR/d33
	mkdir -p $DIR/d33
	chown $RUNAS_ID $DIR/d33
	$RUNAS $OPENFILE -f O_RDWR:O_CREAT -m 0444 $DIR/d33/f33|| error "create"
	$RUNAS $OPENFILE -f O_RDWR:O_CREAT -m 0444 $DIR/d33/f33 && \
	error "open RDWR" || true
}
run_test 33a "test open file(mode=0444) with O_RDWR (should return error)"

test_33b() {
        rm -fr $DIR/d33
        mkdir -p $DIR/d33
        chown $RUNAS_ID $DIR/d33
        $RUNAS $OPENFILE -f 1286739555 $DIR/d33/f33 || true
}
run_test 33b "test open file with malformed flags (No panic)"

setup_all
FREE=$(filesystem_free $MTFS_MNT1)
TEST_34_SIZE=${TEST_34_SIZE:-2000000000000}
if [ $TEST_34_SIZE -ge $FREE ]; then
	TEST_34_SIZE=$FREE;
fi
cleanup_all

test_34a() {
	rm -f $DIR/f34
	$MCREATE $DIR/f34 || error
	#$GETSTRIPE $DIR/f34 2>&1 | grep -q "no stripe info" || error
	$TRUNCATE $DIR/f34 $TEST_34_SIZE || error
	#$GETSTRIPE $DIR/f34 2>&1 | grep -q "no stripe info" || error
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
}
run_test 34a "truncate file that has not been opened ==========="

test_34b() {
	[ ! -f $DIR/f34 ] && test_34a
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
	$OPENFILE -f O_RDONLY $DIR/f34
	#$GETSTRIPE $DIR/f34 2>&1 | grep -q "no stripe info" || error
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
}
run_test 34b "O_RDONLY opening file doesn't create objects ====="

test_34c() {
	[ ! -f $DIR/f34 ] && test_34a
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
	$OPENFILE -f O_RDWR $DIR/f34
	#$GETSTRIPE $DIR/f34 2>&1 | grep -q "no stripe info" && error
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
}
run_test 34c "O_RDWR opening file-with-size works =============="

test_34d() {
	[ ! -f $DIR/f34 ] && test_34a
	dd if=/dev/zero of=$DIR/f34 conv=notrunc bs=4k count=1 || error
	$CHECKSTAT -s $TEST_34_SIZE $DIR/f34 || error
	rm $DIR/f34
}
run_test 34d "write to sparse file ============================="

test_34e() {
	rm -f $DIR/f34e
	$MCREATE $DIR/f34e || error
	$TRUNCATE $DIR/f34e 1000 || error
	$CHECKSTAT -s 1000 $DIR/f34e || error
	$OPENFILE -f O_RDWR $DIR/f34e
	$CHECKSTAT -s 1000 $DIR/f34e || error
}
run_test 34e "create objects, some with size and some without =="

test_34f() { # bug 6242, 6243
	SIZE34F=48000
	rm -f $DIR/f34f
	$MCREATE $DIR/f34f || error
	$TRUNCATE $DIR/f34f $SIZE34F || error "truncating $DIR/f3f to $SIZE34F"
	dd if=$DIR/f34f of=$TMP/f34f
	$CHECKSTAT -s $SIZE34F $TMP/f34f || error "$TMP/f34f not $SIZE34F bytes"
	dd if=/dev/zero of=$TMP/f34fzero bs=$SIZE34F count=1
	cmp $DIR/f34f $TMP/f34fzero || error "$DIR/f34f not all zero"
	cmp $TMP/f34f $TMP/f34fzero || error "$TMP/f34f not all zero"
	rm $TMP/f34f $TMP/f34fzero $DIR/f34f
}
run_test 34f "read from a file with no objects until EOF ======="

test_34g() {
	dd if=/dev/zero of=$DIR/$tfile bs=1 count=100 seek=$TEST_34_SIZE || error
	$TRUNCATE $DIR/$tfile $((TEST_34_SIZE / 2))|| error
	$CHECKSTAT -s $((TEST_34_SIZE / 2)) $DIR/$tfile || error "truncate failed"
	#cancel_lru_locks osc
	$CHECKSTAT -s $((TEST_34_SIZE / 2)) $DIR/$tfile || \
		error "wrong size after lock cancel"

	$TRUNCATE $DIR/$tfile $TEST_34_SIZE || error
	$CHECKSTAT -s $TEST_34_SIZE $DIR/$tfile || \
		error "expanding truncate failed"
	#cancel_lru_locks osc
	$CHECKSTAT -s $TEST_34_SIZE $DIR/$tfile || \
		error "wrong expanded size after lock cancel"
}
run_test 34g "truncate long file ==============================="

test_35a() {
	cp /bin/sh $DIR/f35a
	chmod 444 $DIR/f35a
	chown $RUNAS_ID $DIR/f35a
	$RUNAS $DIR/f35a && error || true
	rm $DIR/f35a
}
run_test 35a "exec file with mode 444 (should return and not leak) ====="

test_36a() {
	rm -f $DIR/f36
	$UTIME $DIR/f36 || error
}
run_test 36a "MDS utime check (mknod, utime) ==================="

test_36b() {
	echo "" > $DIR/f36
	$UTIME $DIR/f36 || error
}
run_test 36b "OST utime check (open, utime) ===================="

test_36c() {
	rm -f $DIR/d36/f36
	mkdir $DIR/d36
	chown $RUNAS_ID $DIR/d36
	$RUNAS $UTIME $DIR/d36/f36 || error
}
run_test 36c "non-root MDS utime check (mknod, utime) =========="

test_36d() {
	[ ! -d $DIR/d36 ] && test_36c
	echo "" > $DIR/d36/f36
	$RUNAS $UTIME $DIR/d36/f36 || error
}
run_test 36d "non-root OST utime check (open, utime) ==========="

test_36e() {
	[ $RUNAS_ID -eq $UID ] && skip "RUNAS_ID = UID = $UID -- skipping" && return
	mkdir -p $DIR/$tdir
	touch $DIR/$tdir/$tfile
	$RUNAS $UTIME $DIR/$tdir/$tfile && \
		error "utime worked, expected failure" || true
}
run_test 36e "utime on non-owned file (should return error) ===="

test_36f() {
	return 0
}
run_test 36f "do nothing =========="

test_36g() {
	return 0
}
run_test 36g "do nothing =========="

test_37() {
	mkdir -p $DIR/$tdir
	echo f > $DIR/$tdir/fbugfile
	mount -t ext2 -o loop $EXT2_DEV $DIR/$tdir
	ls $DIR/$tdir | grep "\<fbugfile\>" && error
	umount $DIR/$tdir || error
	rm -f $DIR/$tdir/fbugfile || error
}
run_test 37 "ls a mounted file system to check old content ====="

test_38() {
	local file=$DIR/$tfile
	touch $file
	$OPENFILE -f O_DIRECTORY $file
	local RC=$?
	local ENOTDIR=20
	[ $RC -eq 0 ] && error "opened file $file with O_DIRECTORY" || true
	[ $RC -eq $ENOTDIR ] || error "error $RC should be ENOTDIR ($ENOTDIR)"
}
run_test 38 "open a regular file with O_DIRECTORY should return -ENOTDIR ==="

test_39() {
	touch $DIR/$tfile
	touch $DIR/${tfile}2
#	ls -l  $DIR/$tfile $DIR/${tfile}2
#	ls -lu  $DIR/$tfile $DIR/${tfile}2
#	ls -lc  $DIR/$tfile $DIR/${tfile}2
	sleep 2
	$OPENFILE -f O_CREAT:O_TRUNC:O_WRONLY $DIR/${tfile}2
	if [ ! $DIR/${tfile}2 -nt $DIR/$tfile ]; then
		echo "mtime"
		ls -l  $DIR/$tfile $DIR/${tfile}2
		echo "atime"
		ls -lu  $DIR/$tfile $DIR/${tfile}2
		echo "ctime"
		ls -lc  $DIR/$tfile $DIR/${tfile}2
		error "O_TRUNC didn't change timestamps"
	fi
}
run_test 39 "mtime changed on create ==========================="

test_39b() {
	mkdir -p $DIR/$tdir
	cp -p /etc/passwd $DIR/$tdir/fopen
	cp -p /etc/passwd $DIR/$tdir/flink
	cp -p /etc/passwd $DIR/$tdir/funlink
	cp -p /etc/passwd $DIR/$tdir/frename
	ln $DIR/$tdir/funlink $DIR/$tdir/funlink2

	sleep 1
	echo "aaaaaa" >> $DIR/$tdir/fopen
	echo "aaaaaa" >> $DIR/$tdir/flink
	echo "aaaaaa" >> $DIR/$tdir/funlink
	echo "aaaaaa" >> $DIR/$tdir/frename

	local open_new=`stat -c %Y $DIR/$tdir/fopen`
	local link_new=`stat -c %Y $DIR/$tdir/flink`
	local unlink_new=`stat -c %Y $DIR/$tdir/funlink`
	local rename_new=`stat -c %Y $DIR/$tdir/frename`

	cat $DIR/$tdir/fopen > /dev/null
	ln $DIR/$tdir/flink $DIR/$tdir/flink2
	rm -f $DIR/$tdir/funlink2
	mv -f $DIR/$tdir/frename $DIR/$tdir/frename2

	local open_new2=`stat -c %Y $DIR/$tdir/fopen`
	local link_new2=`stat -c %Y $DIR/$tdir/flink`
	local unlink_new2=`stat -c %Y $DIR/$tdir/funlink`
	local rename_new2=`stat -c %Y $DIR/$tdir/frename2`

	[ $open_new2 -eq $open_new ] || error "open file reverses mtime"
	[ $link_new2 -eq $link_new ] || error "link file reverses mtime"
	[ $unlink_new2 -eq $unlink_new ] || error "unlink file reverses mtime"
	[ $rename_new2 -eq $rename_new ] || error "rename file reverses mtime"
}
run_test 39b "mtime change on open, link, unlink, rename  ======"

# this should be set to past
TEST_39_MTIME=`date -d "1 year ago" +%s`

# bug 11063
test_39c() {
	touch $DIR1/$tfile
	sleep 2
	local mtime0=`stat -c %Y $DIR1/$tfile`

	touch -m -d @$TEST_39_MTIME $DIR1/$tfile
	local mtime1=`stat -c %Y $DIR1/$tfile`
	[ "$mtime1" = $TEST_39_MTIME ] || \
		error "mtime is not set to past: $mtime1, should be $TEST_39_MTIME"

	local d1=`date +%s`
	echo hello >> $DIR1/$tfile
	local d2=`date +%s`
	local mtime2=`stat -c %Y $DIR1/$tfile`
	[ "$mtime2" -ge "$d1" ] && [ "$mtime2" -le "$d2" ] || \
		error "mtime is not updated on write: $d1 <= $mtime2 <= $d2"

	mv $DIR1/$tfile $DIR1/$tfile-1

	for (( i=0; i < 2; i++ )) ; do
		local mtime3=`stat -c %Y $DIR1/$tfile-1`
		[ "$mtime2" = "$mtime3" ] || \
			error "mtime ($mtime2) changed (to $mtime3) on rename"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39c "mtime change on rename ==========================="

# bug 21114
test_39d() {
	touch $DIR1/$tfile

	touch -m -d @$TEST_39_MTIME $DIR1/$tfile

	for (( i=0; i < 2; i++ )) ; do
		local mtime=`stat -c %Y $DIR1/$tfile`
		[ $mtime = $TEST_39_MTIME ] || \
			error "mtime($mtime) is not set to $TEST_39_MTIME"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39d "create, utime, stat =============================="

# bug 21114
test_39e() {
	touch $DIR1/$tfile
	local mtime1=`stat -c %Y $DIR1/$tfile`

	touch -m -d @$TEST_39_MTIME $DIR1/$tfile
	
	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile`
		[ $mtime2 = $TEST_39_MTIME ] || \
			error "mtime($mtime2) is not set to $TEST_39_MTIME"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39e "create, stat, utime, stat ========================"

# bug 21114
test_39f() {
	touch $DIR1/$tfile
	mtime1=`stat -c %Y $DIR1/$tfile`

	sleep 2
	touch -m -d @$TEST_39_MTIME $DIR1/$tfile

	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile`
		[ $mtime2 = $TEST_39_MTIME ] || \
			error "mtime($mtime2) is not set to $TEST_39_MTIME"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39f "create, stat, sleep, utime, stat ================="

# bug 11063
test_39g() {
	echo hello >> $DIR1/$tfile
	local mtime1=`stat -c %Y $DIR1/$tfile`

	sleep 2
	chmod o+r $DIR1/$tfile
 
	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile`
		[ "$mtime1" = "$mtime2" ] || \
			error "lost mtime: $mtime2, should be $mtime1"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39g "write, chmod, stat ==============================="

# bug 11063
test_39h() {
	touch $DIR1/$tfile
	sleep 1

	local d1=`date`
	echo hello >> $DIR1/$tfile
	local mtime1=`stat -c %Y $DIR1/$tfile`

	touch -m -d @$TEST_39_MTIME $DIR1/$tfile
	local d2=`date`
	if [ "$d1" != "$d2" ]; then
		echo "write and touch not within one second"
	else
		for (( i=0; i < 2; i++ )) ; do
			local mtime2=`stat -c %Y $DIR1/$tfile`
			[ "$mtime2" = $TEST_39_MTIME ] || \
				error "lost mtime: $mtime2, should be $TEST_39_MTIME"

			#cancel_lru_locks osc
			#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
		done
	fi
}
run_test 39h "write, utime within one second, stat ============="

test_39i() {
	touch $DIR1/$tfile
	sleep 1

	echo hello >> $DIR1/$tfile
	local mtime1=`stat -c %Y $DIR1/$tfile`

	mv $DIR1/$tfile $DIR1/$tfile-1

	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile-1`

		[ "$mtime1" = "$mtime2" ] || \
			error "lost mtime: $mtime2, should be $mtime1"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39i "write, rename, stat =============================="

test_39j() {
	touch $DIR1/$tfile
	sleep 1

	multiop_bg_pause $DIR1/$tfile oO_RDWR:w2097152_c || error "multiop failed"
	local multipid=$!
	local mtime1=`stat -c %Y $DIR1/$tfile`

	mv $DIR1/$tfile $DIR1/$tfile-1

	kill -USR1 $multipid
	wait $multipid || error "multiop close failed"

	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile-1`
		[ "$mtime1" = "$mtime2" ] || \
			error "mtime is lost on close: $mtime2, should be $mtime1"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39j "write, rename, close, stat ======================="

test_39k() {
	touch $DIR1/$tfile
	sleep 1

	multiop_bg_pause $DIR1/$tfile oO_RDWR:w2097152_c || error "multiop failed"
	local multipid=$!
	local mtime1=`stat -c %Y $DIR1/$tfile`

	touch -m -d @$TEST_39_MTIME $DIR1/$tfile

	kill -USR1 $multipid
	wait $multipid || error "multiop close failed"

	for (( i=0; i < 2; i++ )) ; do
		local mtime2=`stat -c %Y $DIR1/$tfile`

		[ "$mtime2" = $TEST_39_MTIME ] || \
			error "mtime is lost on close: $mtime2, should be $TEST_39_MTIME"

		#cancel_lru_locks osc
		#if [ $i = 0 ] ; then echo "repeat after cancel_lru_locks"; fi
	done
}
run_test 39k "write, utime, close, stat ========================"

test_40() {
	dd if=/dev/zero of=$DIR/f40 bs=4096 count=1
	$RUNAS $OPENFILE -f O_WRONLY:O_TRUNC $DIR/f40 && error
	$CHECKSTAT -t file -s 4096 $DIR/f40 || error
}
run_test 40 "failed open(O_TRUNC) doesn't truncate ============="

test_41() {
	# bug 1553
	$SMALL_WRITE $DIR/f41 18
}
run_test 41 "test small file write + fstat ====================="

test_42() {
	return 0
}
run_test 42 "do nothing ====================="

test_43() {
	mkdir -p $DIR/$tdir
	cp -p /bin/ls $DIR/$tdir/$tfile
	$MULTIOP $DIR/$tdir/$tfile Ow_c &
	pid=$!
	# give multiop a chance to open
	sleep 1	

	$DIR/$tdir/$tfile && error || true
	kill -USR1 $pid
	# umount will fail if not wait
	wait $pid
}
run_test 43 "execution of file opened for write should return -ETXTBSY"

test_43a() {
	mkdir -p $DIR/d43
	#cp -p `which multiop` $DIR/d43/multiop
	cp -p $MULTIOP $DIR/d43/multiop
	MULTIOP_PROG=$DIR/d43/multiop multiop_bg_pause $TMP/test43.junk O_c || return 1
	MULTIOP_PID=$!
	$MULTIOP $DIR/d43/multiop Oc && error "expected error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	rm $TMP/test43.junk
}
run_test 43a "open(RDWR) of file being executed should return -ETXTBSY"

test_43b() {
	mkdir -p $DIR/d43
	#cp -p `which multiop` $DIR/d43/multiop
	cp -p $MULTIOP $DIR/d43/multiop
	MULTIOP_PROG=$DIR/d43/multiop multiop_bg_pause $TMP/test43.junk O_c || return 1
	MULTIOP_PID=$!
	$TRUNCATE $DIR/d43/multiop 0 && error "expected error, got success"
	kill -USR1 $MULTIOP_PID || return 2
	wait $MULTIOP_PID || return 3
	rm $TMP/test43.junk
}
run_test 43b "truncate of file being executed should return -ETXTBSY"

test_43c() {
	local testdir="$DIR/d43c"
	mkdir -p $testdir
	cp $SHELL $testdir/
	( cd $(dirname $SHELL) && md5sum $(basename $SHELL) ) | \
		( cd $testdir && md5sum -c)
}
run_test 43c "md5sum of copy into mtfs========================"

test_43d() { #BUG245
	cp -p /bin/ls $DIR
	cleanup_and_setup
	$DIR/ls / > /dev/null || error
}
run_test 43d "execution of file never lookuped should succeed"

test_44() {
	#[ "$OSTCOUNT" -lt "2" ] && skip "skipping 2-stripe test" && return
	dd if=/dev/zero of=$DIR/f1 bs=4k count=1 seek=1023
	dd if=$DIR/f1 of=/dev/null bs=4k count=1
}
run_test 44 "zero length read from a sparse stripe ============="

page_size() {
	getconf PAGE_SIZE
}

test_48a() { # bug 2399
	#check_kernel_version 34 || return 0
	mkdir -p $DIR/d48a
	cd $DIR/d48a
	mv $DIR/d48a $DIR/d48.new || error "move directory failed"
	mkdir $DIR/d48a || error "recreate directory failed"
	touch foo || error "'touch foo' failed after recreating cwd"
	mkdir bar || error "'mkdir foo' failed after recreating cwd"
	#if check_kernel_version 44; then
		touch .foo || error "'touch .foo' failed after recreating cwd"
		mkdir .bar || error "'mkdir .foo' failed after recreating cwd"
	#fi
	ls . > /dev/null || error "'ls .' failed after recreating cwd"
	ls .. > /dev/null || error "'ls ..' failed after removing cwd"
	cd . || error "'cd .' failed after recreating cwd"
	mkdir . && error "'mkdir .' worked after recreating cwd"
	rmdir . && error "'rmdir .' worked after recreating cwd"
	ln -s . baz || error "'ln -s .' failed after recreating cwd"
	cd .. || error "'cd ..' failed after recreating cwd"
}
run_test 48a "Access renamed working dir (should return errors)="

test_48b() {
	#check_kernel_version 34 || return 0
	mkdir -p $DIR/d48b
	cd $DIR/d48b
	rmdir $DIR/d48b || error "remove cwd $DIR/d48b failed"
	touch foo && error "'touch foo' worked after removing cwd"
	mkdir foo && error "'mkdir foo' worked after removing cwd"
	#if check_kernel_version 44; then
		touch .foo && error "'touch .foo' worked after removing cwd"
		mkdir .foo && error "'mkdir .foo' worked after removing cwd"
	#fi
	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "yes" ]; then
		# Can we say that if a lowerfs do no supply a proper d_revalidate
		# this test will certainly fail?
		ls . > /dev/null && error "'ls .' worked after removing cwd"
	fi
	ls .. > /dev/null || error "'ls ..' failed after removing cwd"
	#is_patchless || ( cd . && error "'cd .' worked after removing cwd" )
	#cd . && error "'cd .' worked after removing cwd"
	mkdir . && error "'mkdir .' worked after removing cwd"
	rmdir . && error "'rmdir .' worked after removing cwd"
	ln -s . foo && error "'ln -s .' worked after removing cwd"
	cd .. || echo "'cd ..' failed after removing cwd `pwd`"  #bug 3517
}
run_test 48b "Access removed working dir (should return errors)="

test_48c() {
	#check_kernel_version 36 || return 0
	#lctl set_param debug=-1
	#set -vx
	mkdir -p $DIR/d48c/dir
	cd $DIR/d48c/dir
	$TRACE rmdir $DIR/d48c/dir || error "remove cwd $DIR/d48c/dir failed"
	$TRACE touch foo && error "'touch foo' worked after removing cwd"
	$TRACE mkdir foo && error "'mkdir foo' worked after removing cwd"
	#if check_kernel_version 44; then
		touch .foo && error "'touch .foo' worked after removing cwd"
		mkdir .foo && error "'mkdir .foo' worked after removing cwd"
	#fi
	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "yes" ]; then
		$TRACE ls . && error "'ls .' worked after removing cwd"
	fi
	$TRACE ls .. || error "'ls ..' failed after removing cwd"
	#is_patchless || ( $TRACE cd . && error "'cd .' worked after removing cwd" )
	#$TRACE cd . && error "'cd .' worked after removing cwd"
	$TRACE mkdir . && error "'mkdir .' worked after removing cwd"
	$TRACE rmdir . && error "'rmdir .' worked after removing cwd"
	$TRACE ln -s . foo && error "'ln -s .' worked after removing cwd"
	$TRACE cd .. || echo "'cd ..' failed after removing cwd `pwd`" #bug 3415
}
run_test 48c "Access removed working subdir (should return errors)"

test_48d() { # bug 2350
	#check_kernel_version 36 || return 0
	#lctl set_param debug=-1
	#set -vx
	mkdir -p $DIR/d48d/dir
	cd $DIR/d48d/dir
	$TRACE rmdir $DIR/d48d/dir || error "remove cwd $DIR/d48d/dir failed"
	$TRACE rmdir $DIR/d48d || error "remove parent $DIR/d48d failed"
	$TRACE touch foo && error "'touch foo' worked after removing parent"
	$TRACE mkdir foo && error "'mkdir foo' worked after removing parent"
	# if check_kernel_version 44; then
		touch .foo && error "'touch .foo' worked after removing parent"
		mkdir .foo && error "'mkdir .foo' worked after removing parent"
	# fi
	if [ "$LOWERFS_DIR_INVALID_WHEN_REMOVED" = "yes" ]; then
		$TRACE ls . && error "'ls .' worked after removing parent"
		$TRACE ls .. && error "'ls ..' worked after removing parent"
	fi
	#is_patchless || ( $TRACE cd . && error "'cd .' worked after recreate parent" )
	#$TRACE cd . && error "'cd .' worked after recreate parent"
	$TRACE mkdir . && error "'mkdir .' worked after removing parent"
	$TRACE rmdir . && error "'rmdir .' worked after removing parent"
	$TRACE ln -s . foo && error "'ln -s .' worked after removing parent" || true
	#is_patchless || ( $TRACE cd .. && error "'cd ..' worked after removing parent" || true )
	#$TRACE cd .. && error "'cd ..' worked after removing parent" || true
}
run_test 48d "Access removed parent subdir (should return errors)"

test_48e() { # bug 4134
	check_kernel_version 41 || return 0
	#lctl set_param debug=-1
	#set -vx
	mkdir -p $DIR/d48e/dir
	cd $DIR/d48e/dir
	$TRACE rmdir $DIR/d48e/dir || error "remove cwd $DIR/d48e/dir failed"
	$TRACE rmdir $DIR/d48e || error "remove parent $DIR/d48e failed"
	$TRACE touch $DIR/d48e || error "'touch $DIR/d48e' failed"
	$TRACE chmod +x $DIR/d48e || error "'chmod +x $DIR/d48e' failed"
	# On a buggy kernel addition of "touch foo" after cd .. will
	# produce kernel oops in lookup_hash_it
	touch ../foo && error "'cd ..' worked after recreate parent"
	cd $DIR
	$TRACE rm $DIR/d48e || error "rm '$DIR/d48e' failed"
}
run_test 48e "Access to recreated parent subdir (should return errors)"

test_50() {
	mkdir $DIR/d50
	cd $DIR/d50
	ls /proc/$$/cwd || error
}
run_test 50 "special situations: /proc symlinks  ==============="

test_51a() {
	mkdir $DIR/d51
	touch $DIR/d51/foo
	$MCREATE $DIR/d51/bar
	rm $DIR/d51/foo
	$CREATEMANY -m $DIR/d51/longfile 201
	FNUM=202
	while [ `ls -sd $DIR/d51 | awk '{ print $1 }'` -eq 4 ]; do
		$MCREATE $DIR/d51/longfile$FNUM
		FNUM=$(($FNUM + 1))
		echo -n "+"
	done
	echo
	ls -l $DIR/d51 > /dev/null || error
}
run_test 51a "special situations: split htree with empty entry =="

export NUMTEST=70000
test_51b() {
	NUMFREE=`df -i -P $DIR | tail -n 1 | awk '{ print $4 }'`
	[ $NUMFREE -lt $NUMTEST ] && \
		skip "not enough free inodes ($NUMFREE)" && \
		return

	#check_kernel_version 40 || NUMTEST=31000
	[ $NUMFREE -lt $NUMTEST ] && NUMTEST=$(($NUMFREE - 50))

	mkdir -p $DIR/d51b
	$CREATEMANY -d $DIR/d51b/t- $NUMTEST
}
run_test 51b "mkdir .../t-0 --- .../t-$NUMTEST ===================="

test_51c() {
	[ ! -d $DIR/d51b ] && skip "$DIR/51b missing" && \
		return

	$UNLINKEMANY -d $DIR/d51b/t- $NUMTEST
}
run_test 51c "rmdir .../t-0 --- .../t-$NUMTEST ===================="

test_52a() {
	[ -f $DIR/d52a/foo ] && chattr -a $DIR/d52a/foo
	mkdir -p $DIR/d52a
	touch $DIR/d52a/foo
	chattr +a $DIR/d52a/foo || error "chattr +a failed"
	echo bar >> $DIR/d52a/foo || error "append bar failed"
	cp /etc/hosts $DIR/d52a/foo && error "cp worked"
	rm -f $DIR/d52a/foo 2>/dev/null && error "rm worked"
	link $DIR/d52a/foo $DIR/d52a/foo_link 2>/dev/null && error "link worked"
	echo foo >> $DIR/d52a/foo || error "append foo failed"
	$MRENAME $DIR/d52a/foo $DIR/d52a/foo_ren && error "rename worked"
	# new lsattr displays 'e' flag for extents
	lsattr $DIR/d52a/foo | egrep -q "^-+a[-e]+ $DIR/d52a/foo" || error "lsattr"
	chattr -a $DIR/d52a/foo || error "chattr -a failed"

	rm -fr $DIR/d52a || error "cleanup rm failed"
}
run_test 52a "append-only flag test (should return errors) ====="

test_52b() {
	[ -f $DIR/d52b/foo ] && chattr -i $DIR/d52b/foo
	mkdir -p $DIR/d52b
	touch $DIR/d52b/foo
	chattr +i $DIR/d52b/foo || error "chattr +i failed"
	cat test > $DIR/d52b/foo && error "cat test worked"
	cp /etc/hosts $DIR/d52b/foo && error "cp worked"
	rm -f $DIR/d52b/foo 2>/dev/null && error "rm worked"
	link $DIR/d52b/foo $DIR/d52b/foo_link 2>/dev/null && error "link worked"
	echo foo >> $DIR/d52b/foo && error "echo worked"
	$MRENAME $DIR/d52b/foo $DIR/d52b/foo_ren && error "rename worked"
	[ -f $DIR/d52b/foo ] || error
	[ -f $DIR/d52b/foo_ren ] && error
	lsattr $DIR/d52b/foo | egrep -q "^-+i[-e]+ $DIR/d52b/foo" || error "lsattr"
	chattr -i $DIR/d52b/foo || error "chattr failed"

	rm -fr $DIR/d52b || error "remove failed"
}
run_test 52b "immutable flag test (should return errors) ======="

test_54a() {
        [ ! -f "$SOCKETSERVER" ] && skip "no socketserver, skipping" && return
        [ ! -f "$SOCKETCLIENT" ] && skip "no socketclient, skipping" && return
     	$SOCKETSERVER $DIR/socket
     	$SOCKETCLIENT $DIR/socket || error
      	$MUNLINK $DIR/socket
}
run_test 54a "unix domain socket test =========================="

test_54b() {
	f="$DIR/f54b"
	mknod $f c 1 3
	chmod 0666 $f
	dd if=/dev/zero of=$f bs=`getconf PAGE_SIZE` count=1
}
run_test 54b "char device works in mtfs ======================"

find_loop_dev() {
	[ -b /dev/loop/0 ] && LOOPBASE=/dev/loop/
	[ -b /dev/loop0 ] && LOOPBASE=/dev/loop
	[ -z "$LOOPBASE" ] && echo "/dev/loop/0 and /dev/loop0 gone?" && return

	for i in `seq 3 7`; do
		losetup $LOOPBASE$i > /dev/null 2>&1 && continue
		LOOPDEV=$LOOPBASE$i
		LOOPNUM=$i
		break
	done
}

test_54c() {
	tfile="$DIR/f54c"
	tdir="$DIR/d54c"
	loopdev="$DIR/loop54c"

	find_loop_dev
	[ -z "$LOOPNUM" ] && echo "couldn't find empty loop device" && return
	mknod $loopdev b 7 $LOOPNUM
	echo "make a loop file system with $tfile on $loopdev ($LOOPNUM)..."
	dd if=/dev/zero of=$tfile bs=`getconf PAGE_SIZE` seek=1024 count=1 > /dev/null
	losetup $loopdev $tfile || error "can't set up $loopdev for $tfile"
	mkfs.ext2 $loopdev || error "mke2fs on $loopdev"
	mkdir -p $tdir
	mount -t ext2 $loopdev $tdir || error "error mounting $loopdev on $tdir"
	dd if=/dev/zero of=$tdir/tmp bs=`getconf PAGE_SIZE` count=30 || error "dd write"
	df $tdir
	dd if=$tdir/tmp of=/dev/zero bs=`getconf PAGE_SIZE` count=30 || error "dd read"
	umount $tdir
	losetup -d $loopdev
	rm $loopdev
}
run_test 54c "block device works in mtfs ====================="

test_54d() {
	f="$DIR/f54d"
	string="aaaaaa"
	mknod $f p
	[ "$string" = `echo $string > $f | cat $f` ] || error
}
run_test 54d "fifo device works in mtfs ======================"

test_54e() {
	#check_kernel_version 46 || return 0
	f="$DIR/f54e"
	string="aaaaaa"
	cp -aL /dev/console $f
	echo $string > $f || error
}
run_test 54e "console/tty device works in mtfs ======================"

test_59() {
	echo "touch 130 files"
	$CREATEMANY -o $DIR/f59- 130
	echo "rm 130 files"
	$UNLINKEMANY $DIR/f59- 130
	sync
	sleep 2
        # wait for commitment of removal
}
run_test 59 "verify cancellation of llog records async ========="

test_61() {
	f="$DIR/f61"
	dd if=/dev/zero of=$f bs=`page_size` count=1
	#cancel_lru_locks osc
	$MULTIOP $f OSMWUc || error
	sync
}
run_test 61 "mmap() writes don't make sync hang ================"

test_99a() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	mkdir -p $DIR/d99cvsroot || error "mkdir $DIR/d99cvsroot failed"
	chown $RUNAS_ID $DIR/d99cvsroot || error "chown $DIR/d99cvsroot failed"
	local oldPWD=$PWD	# bug 13584, use $TMP as working dir
	cd $TMP

	$RUNAS cvs -d $DIR/d99cvsroot init || error "cvs init failed"
	cd $oldPWD
}
run_test 99a "cvs init ========================================="

test_99b() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	[ ! -d $DIR/d99cvsroot ] && test_99a
	$RUNAS [ ! -w /tmp ] && skip "/tmp has wrong w permission -- skipping" && return
	cd /etc/init.d || error "cd /etc/init.d failed"
	# some versions of cvs import exit(1) when asked to import links or
	# files they can't read.  ignore those files.
	TOIGNORE=$(find . -type l -printf '-I %f\n' -o \
			! -perm +4 -printf '-I %f\n')
	$RUNAS cvs -d $DIR/d99cvsroot import -m "nomesg" $TOIGNORE \
		d99reposname vtag rtag > /dev/null || error "cvs import failed"
}
run_test 99b "cvs import ======================================="

test_99c() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	[ ! -d $DIR/d99cvsroot ] && test_99b
	cd $DIR || error "cd $DIR failed"
	mkdir -p $DIR/d99reposname || error "mkdir $DIR/d99reposname failed"
	chown $RUNAS_ID $DIR/d99reposname || \
		error "chown $DIR/d99reposname failed"
	echo "$RUNAS cvs -d $DIR/d99cvsroot co d99reposname"
	$RUNAS cvs -d $DIR/d99cvsroot co d99reposname > /dev/null || \
		error "cvs co d99reposname failed"
}
run_test 99c "cvs checkout ====================================="

test_99d() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	[ ! -d $DIR/d99cvsroot ] && test_99c
	cd $DIR/d99reposname
	$RUNAS touch foo99
	$RUNAS cvs add -m 'addmsg' foo99
}
run_test 99d "cvs add =========================================="

test_99e() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	[ ! -d $DIR/d99cvsroot ] && test_99c
	cd $DIR/d99reposname
	$RUNAS cvs update
}
run_test 99e "cvs update ======================================="

test_99f() {
	[ -z "$(which cvs 2>/dev/null)" ] && skip "could not find cvs" && return
	[ ! -d $DIR/d99cvsroot ] && test_99d
	cd $DIR/d99reposname
	$RUNAS cvs commit -m 'nomsg' foo99
	rm -fr $DIR/d99cvsroot
}
run_test 99f "cvs commit ======================================="

test_102a() {
	local testfile=$DIR/xattr_testfile

	rm -f $testfile
        touch $testfile

	[ "$UID" != 0 ] && skip "must run as root" && return
	[ -z "$(which setfattr 2>/dev/null)" ] && skip "could not find setfattr" && return

	echo "set/get xattr..."
        setfattr -n trusted.name1 -v value1 $testfile || error
        [ "`getfattr -n trusted.name1 $testfile 2> /dev/null | \
        grep "trusted.name1"`" == "trusted.name1=\"value1\"" ] || error

        setfattr -n user.author1 -v author1 $testfile || error
        [ "`getfattr -n user.author1 $testfile 2> /dev/null | \
        grep "user.author1"`" == "user.author1=\"author1\"" ] || error

	echo "listxattr..."
        setfattr -n trusted.name2 -v value2 $testfile || error
        setfattr -n trusted.name3 -v value3 $testfile || error
        [ `getfattr -d -m "^trusted" $testfile 2> /dev/null | \
        grep "trusted.name" | wc -l` -eq 3 ] || error


        setfattr -n user.author2 -v author2 $testfile || error
        setfattr -n user.author3 -v author3 $testfile || error
        [ `getfattr -d -m "^user" $testfile 2> /dev/null | \
        grep "user" | wc -l` -eq 3 ] || error

	echo "remove xattr..."
        setfattr -x trusted.name1 $testfile || error
        getfattr -d -m trusted $testfile 2> /dev/null | \
        grep "trusted.name1" && error || true

        setfattr -x user.author1 $testfile || error
        getfattr -d -m user $testfile 2> /dev/null | \
        grep "user.author1" && error || true

	rm -f $testfile
}
run_test 102a "user xattr test =================================="

test_105a() {
	# doesn't work on 2.4 kernels
	touch $DIR/$tfile
	if [ "$LOWERFS_NAME" = "lustre" ]; then
		if [ -n "`mount | grep \"$LOWERFS_MNT1.*flock\" | grep -v noflock`" ];
		then
			$FLOCKS_TEST 1 on -f $DIR/$tfile || error "unexpted flock failure with flock option on"
		else
			$FLOCKS_TEST 1 off -f $DIR/$tfile || error "unexpted flock success with flock option off"
		fi
	else
		if [ "$LOWERFS_SUPPORT_FLOCK" = "yes" ]; then
			$FLOCKS_TEST 1 on -f $DIR/$tfile || error "unexpted flock failure"
		else
			$FLOCKS_TEST 1 off -f $DIR/$tfile || error "unexpted flock success"
		fi
	fi
}
run_test 105a "flock test ========"

test_105b() {
	touch $DIR/$tfile
	if [ "$LOWERFS_NAME" = "lustre" ]; then
		if [ -n "`mount | grep \"$LOWERFS_MNT1.*flock\" | grep -v noflock`" ];
		then
			$FLOCKS_TEST 1 on -c $DIR/$tfile || error "unexpted fcntl failure with flock option on"
		else
			$FLOCKS_TEST 1 off -c $DIR/$tfile || error "unexpted fcntl success with flock option off"
		fi
	else
		if [ "$LOWERFS_SUPPORT_FCNTL" = "yes" ]; then
			$FLOCKS_TEST 1 on -c $DIR/$tfile || error "unexpted fcntl failure"
		else
			$FLOCKS_TEST 1 off -c $DIR/$tfile || error "unexpted fcntl success"
		fi
	fi
}
run_test 105b "fcntl test ========"

test_105c() {
	touch $DIR/$tfile
	if [ "$LOWERFS_NAME" = "lustre" ]; then
		if [ -n "`mount | grep \"$LOWERFS_MNT1.*flock\" | grep -v noflock`" ];
		then
			$FLOCKS_TEST 1 on -l $DIR/$tfile || error "unexpted lockf failure with flock option on"
		else
			$FLOCKS_TEST 1 off -l $DIR/$tfile || error "unexpted lockf success with flock option off"
		fi
	else
		if [ "$LOWERFS_SUPPORT_LOCKF" = "yes" ]; then
			$FLOCKS_TEST 1 on -c $DIR/$tfile || error "unexpted lockf failure"
		else
			$FLOCKS_TEST 1 off -c $DIR/$tfile || error "unexpted lockf success"
		fi
	fi
}
run_test 105c "lockf test ========"

test_106() { #bug 10921
	mkdir -p $DIR/$tdir
	$DIR/$tdir && error "exec $DIR/$tdir succeeded"
	chmod 777 $DIR/$tdir || error "chmod $DIR/$tdir failed"
}
run_test 106 "attempt exec of dir followed by chown of that dir"

test_107() {
        CDIR=`pwd`
        cd $DIR

        local file=core
        rm -f $file

        local save_pattern=$(sysctl -n kernel.core_pattern)
        local save_uses_pid=$(sysctl -n kernel.core_uses_pid)
        sysctl -w kernel.core_pattern=$file
        sysctl -w kernel.core_uses_pid=0

        ulimit -c unlimited
        sleep 60 &
        SLEEPPID=$!

        sleep 1

        kill -s 11 $SLEEPPID
        wait $SLEEPPID
        if [ -e $file ]; then
                size=`stat -c%s $file`
                [ $size -eq 0 ] && error "Zero length core file $file"
        else
                error "Fail to create core file $file"
        fi
        rm -f $file
        sysctl -w kernel.core_pattern=$save_pattern
        sysctl -w kernel.core_uses_pid=$save_uses_pid
        cd $CDIR
}
run_test 107 "Coredump on SIG"

test_110() {
	mkdir -p $DIR/d110
	mkdir $DIR/d110/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa || error "mkdir with 255 char fail"
	mkdir $DIR/d110/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb && error "mkdir with 256 char should fail, but not"
	touch $DIR/d110/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx || error "create with 255 char fail"
	touch $DIR/d110/yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy && error ""create with 256 char should fail, but not

	ls -l $DIR/d110
	rm -fr $DIR/d110
}
run_test 110 "filename length checking"

test_119b()
{
	#[ "$OSTCOUNT" -lt "2" ] && skip "skipping 2-stripe test" && return
	#$SETSTRIPE -c 2 $DIR/$tfile || error "setstripe failed"
	dd if=/dev/zero of=$DIR/$tfile bs=1M count=1 seek=1 || error "dd failed"
	sync
	echo "$MULTIOP $DIR/$tfile oO_RDONLY:O_DIRECT:r$((2048 * 1024)) "
	$MULTIOP $DIR/$tfile oO_RDONLY:O_DIRECT:r$((2048 * 1024)) || \
		error "direct read failed"
	rm -f $DIR/$tfile
}
[ "$LOWERFS_SUPPORT_DIRECTIO" = "yes" ] && run_test 119b "Sparse directIO read must return actual read amount"

test_119c()
{
        BSIZE=1048576
        $DIRCTIO write $DIR/$tfile 3 1 $BSIZE || error "direct write failed"
        $DIRCTIO readhole $DIR/$tfile 0 2 $BSIZE || error "reading hole failed"
        rm -f $DIR/$tfile
}
[ "$LOWERFS_SUPPORT_DIRECTIO" = "yes" ] && run_test 119c "Testing for direct read hitting hole"

# Test for writev/readv
test_131a() {
	$RWV -f $DIR/$tfile -w -n 3 524288 1048576 1572864 || \
	error "writev test failed"
	$RWV -f $DIR/$tfile -r -v -n 2 1572864 1048576 || \
	error "readv failed"
	rm -f $DIR/$tfile
}
run_test 131a "test iov's crossing stripe boundary for writev/readv"

test_131b() {
	$RWV -f $DIR/$tfile -w -a -n 3 524288 1048576 1572864 || \
	error "append writev test failed"
	$RWV -f $DIR/$tfile -w -a -n 2 1572864 1048576 || \
	error "append writev test failed"
	rm -f $DIR/$tfile
}
run_test 131b "test append writev"

test_131c() {
	# Used to create with flag O_LOV_DELAY_CREATE
	return 0
	#$RWV -f $DIR/$tfile -w -d -n 1 1048576 || return 0
	#error "NOT PASS"
}
#run_test 131c "test read/write on file w/o objects"
run_test 131c "do nothing"

test_131d() {
	$RWV -f $DIR/$tfile -w -n 1 1572864
	NOB=`$RWV -f $DIR/$tfile -r -n 3 524288 524288 1048576 | awk '/error/ {print $6}'`
	if [ "$NOB" != 1572864 ]; then
		error "Short read filed: read $NOB bytes instead of 1572864"
	fi
	rm -f $DIR/$tfile
}
run_test 131d "test short read"

test_131e() {
	$RWV -f $DIR/$tfile -w -s 1048576 -n 1 1048576
	$RWV -f $DIR/$tfile -r -z -s 0 -n 1 524288 || \
	error "read hitting hole failed"
	rm -f $DIR/$tfile
}
run_test 131e "test read hitting hole"

test_140() {
        mkdir -p $DIR/$tdir || error "Creating dir $DIR/$tdir"
        cd $DIR/$tdir || error "Changing to $DIR/$tdir"
        cp /usr/bin/stat . || error "Copying stat to $DIR/$tdir"

        # VFS limits max symlink depth to 5(4KSTACK) or 7(8KSTACK) or 8
        local i=0
        while i=`expr $i + 1`; do
                mkdir -p $i || error "Creating dir $i"
                cd $i || error "Changing to $i"
                ln -s ../stat stat || error "Creating stat symlink"
                # Read the symlink until ELOOP present,
                # not LBUGing the system is considered success,
                # we didn't overrun the stack.
                $OPENFILE -f O_RDONLY stat >/dev/null 2>&1; ret=$?
                [ $ret -ne 0 ] && {
                        if [ $ret -eq 40 ]; then
                                break  # -ELOOP
                        else
                                error "Open stat symlink"
                                return
                        fi
                }
        done
        i=`expr $i - 1`
        echo "The symlink depth = $i"
        [ $i -eq 5 -o $i -eq 7 -o $i -eq 8 ] || error "Invalid symlink depth"
}
run_test 140 "Check reasonable stack depth (shouldn't LBUG) ===="

test_150() {
	local TF="$TMP/$tfile"

	dd if=/dev/urandom of=$TF bs=6096 count=1 || error "dd failed"
	cp $TF $DIR/$tfile
	#cancel_lru_locks osc
	cmp $TF $DIR/$tfile || error "$TMP/$tfile $DIR/$tfile differ"
	#remount_client $MOUNT
	cleanup_and_setup
	#df -P $MOUNT
	df -P $MTFS_MNT1
	cmp $TF $DIR/$tfile || error "$TF $DIR/$tfile differ (remount)"

	$TRUNCATE $TF 6000
	$TRUNCATE $DIR/$tfile 6000
	#cancel_lru_locks osc
	cmp $TF $DIR/$tfile || error "$TF $DIR/$tfile differ (truncate1)"

	echo "12345" >>$TF
	echo "12345" >>$DIR/$tfile
	#cancel_lru_locks osc
	cmp $TF $DIR/$tfile || error "$TF $DIR/$tfile differ (append1)"

	echo "12345" >>$TF
	echo "12345" >>$DIR/$tfile
	#cancel_lru_locks osc
	cmp $TF $DIR/$tfile || error "$TF $DIR/$tfile differ (append2)"

	rm -f $TF
	true
}
run_test 150 "truncate/append tests"

test_153() {
	$MULTIOP $DIR/$tfile Ow4096Ycu || error "multiop failed"
}
run_test 153 "test if fdatasync does not crash ======================="

test_154() {
	# do directio so as not to populate the page cache
	log "creating a 10 Mb file"
	$MULTIOP $DIR/$tfile oO_CREAT:O_DIRECT:O_RDWR:w$((10*1048576))c || error "multiop failed while creating a file"
	log "starting reads"
	dd if=$DIR/$tfile of=/dev/null bs=4096 &
	log "truncating the file"
	$MULTIOP $DIR/$tfile oO_TRUNC:c || error "multiop failed while truncating the file"
	log "killing dd"
	kill %+ || true # reads might have finished
	echo "wait until dd is finished"
	wait
	log "removing the temporary file"
	rm -rf $DIR/$tfile || error "tmp file removal failed"
}
[ "$LOWERFS_SUPPORT_DIRECTIO" = "yes" ] && run_test 154 "parallel read and truncate should not deadlock ==="

test_155() {
	local temp=$TMP/$tfile
	local file=$DIR/$tfile
	#local list=$(comma_list $(osts_nodes))
	#local big=$(do_nodes $list grep "cache" /proc/cpuinfo | \
	#	awk '{sum+=$4} END{print sum}')
	##local big=1048576 #so big, as to cas lustre network problem
	local big=65536

	log big is $big K

	dd if=/dev/urandom of=$temp bs=6096 count=1 || \
		error "dd of=$temp bs=6096 count=1 failed"
	cp $temp $file
	#cancel_lru_locks osc
	cmp $temp $file || error "$temp $file differ"

	$TRUNCATE $temp 6000
	$TRUNCATE $file 6000
	cmp $temp $file || error "$temp $file differ (truncate1)"

	echo "12345" >>$temp
	echo "12345" >>$file
	cmp $temp $file || error "$temp $file differ (append1)"

	echo "12345" >>$temp
	echo "12345" >>$file
	cmp $temp $file || error "$temp $file differ (append2)"

	dd if=/dev/urandom of=$temp bs=$((big*2)) count=1k || \
		error "dd of=$temp bs=$((big*2)) count=1k failed"
	cp $temp $file
	ls -lh $temp $file
	#cancel_lru_locks osc
	cmp $temp $file || error "$temp $file differ"

	rm -f $temp
	true
}
leak_detect_state_push "no"
run_test 155 "Verification of correctness"
leak_detect_state_pop

test_212() {
	size=`date +%s`
	size=$((size % 8192 + 1))
	dd if=/dev/urandom of=$DIR/f212 bs=1k count=$size
	$SENDFILE $DIR/f212 $DIR/f212.xyz || error "sendfile wrong"
	rm -f $DIR/f212 $DIR/f212.xyz
}
run_test 212 "Sendfile test ============================================"

#
# LIXI added
# lustre-1.8.6 has some bug with jbd2
#
test_500() {
	$CORRECT $DIR/f500 -b 1024 -n 10000
	rm -f $DIR/f500
}
leak_detect_state_push "no"
run_test 500 "correctness test ==========================================="
leak_detect_state_pop

#
# LIXI added
# Should shrink memory
#
test_501() {
	$APPEND_MANY $DIR/$tfile
}
leak_detect_state_push "no"
run_test 501 "append many ================================================"
leak_detect_state_pop

cleanup_all
echo "=== $0: completed ==="
