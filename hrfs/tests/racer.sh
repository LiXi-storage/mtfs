#!/bin/bash
set -e
ONLY=${ONLY:-"$*"}

EXCEPT_SLOW="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
ALWAYS_EXCEPT=${ALWAYS_EXCEPT:-"$BUG $EXAMPLE_EXCEPT"}

TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
. $TESTS_DIR/test-framework.sh
init_test_env

echo "==== $0: started ===="

RACER_DIR=${RACER_DIR:-$TESTS_DIR/racer}
DURATION=${DURATION:-600}
CLIENTS=${CLIENTS:-$(hostname)}	
RACERDIRS=${RACERDIRS:-$DIR}
MAX_RACER_THREADS=${MAX_RACER_THREADS:-3}
racer=${racer:-$RACER_DIR/racer.sh}

[ -x "$racer" ] || echo racer is not installed || exit 1

for d in ${RACERDIRS}; do
	RDIRS="$RDIRS $d/racer"
	[ -e $d/racer ] && rm $d/racer -rf
	mkdir -p $d/racer
done

assert_env CLIENTS

timer_on () {
	sleep $1 && kill -s ALRM $$ &
    	TIMERPID=$!
    	echo TIMERPID=$TIMERPID
}

do_racer_cleanup () {
	trap 0

	local WAIT=0
	local INTERVAL=5
        local pids
	local rc=0

	local RDIR=$1

	echo "DOING RACER CLEANUP ... "

	# Check if all processes are killed

	local clients=$CLIENTS

	# 1.Let chance to racer to kill all it's processes
	# FIXME: not sure how long does it take for racer to kill all processes
	# 80 is sometimes are enough for 2 clients; sometimes it takes more than 150 sec
	while [ $WAIT -lt 90 ]; do
		running=$(do_nodes $clients "ps uax | grep $RDIR " | egrep -v "(acceptance|grep|pdsh|bash)" || true)
		[ -z "$running" ] && rc=0 && break
		echo "clients $clients are still running the racer processes. Waited $WAIT secs"
		echo $running
		rc=1
		[ $INTERVAL -lt 40 ] && INTERVAL=$((INTERVAL + INTERVAL))
		sleep $INTERVAL
		WAIT=$((WAIT + INTERVAL))
	done

	# 2. Kill the remaining processes
	if [ $rc -ne 0 ]; then
		for C in ${clients//,/ } ; do
			pids=$(do_node $C "ps uax | grep $RDIR " | egrep -v "(acceptance|grep|PATH)" | awk '{print $2}' || true)
			if [ ! -z "$pids" ]; then
				echo "client $C still running racer processes after $WAIT seconds. Killing $pids"
				do_node $C "ps uax | grep $RDIR " | egrep -v "(acceptance|grep|PATH)"
				do_node $C kill -TERM $pids || true
				# let processes to be killed
				sleep 2
	# 3. Check if the processes were killed
	# exit error if the processes still exist
				for pid in $pids; do
					do_node $C "ps -P $pid" && RC=1 || true
				done
			else
				echo "All processes on client $C exited after $WAIT seconds. OK."
			fi
		done
	else
		echo "No racer processes running after $WAIT seconds. OK."
		wait_remote_prog $racer 10
	fi
}

racer_cleanup () {
	if [ "$timeout" == "timeout" ]; then
		echo $timeout killing RACERPID=$RACERPID
		kill $RACERPID || true
		sleep 2	# give chance racer to kill it's processes
		local dir
		for dir in $RDIRS; do
			do_racer_cleanup $dir
		done
	else
		echo "Racer completed before DURATION=$DURATION expired. Cleaning up..."
		kill $TIMERPID
		for dir in $RDIRS; do
			do_racer_cleanup $dir
		done
	fi
}

racer_timeout () {
	timeout="timeout"
	racer_cleanup
	echo "$0: completed $RC"
	exit $RC
}

run_racer() {
	NUM_RACER_THREADS=$1
	# run racer
	log "Start racer on clients: $CLIENTS DURATION=$DURATION"
	RC=0

	trap racer_timeout ALRM

	timer_on $((DURATION + 5))

	RACERPID=""
	for rdir in $RDIRS; do
		log "DURATION=$DURATION $racer $rdir $NUM_RACER_THREADS"
		do_nodes $CLIENTS "DURATION=$DURATION $racer $rdir $NUM_RACER_THREADS" &
		pid=$!
		RACERPID="$RACERPID $pid"
	done

	echo RACERPID=$RACERPID
	for rpid in $RACERPID; do
		wait $rpid
		rc=$?
		echo "rpid=$rpid rc=$rc"
		if [ $rc != 0 ]; then
			RC=$((RC + 1))
		fi
	done

	racer_cleanup
}

ALL_RACER_PROGS="file_create dir_create file_rm file_rename file_link file_symlink file_list file_concat"
#ALL_RACER_PROGS="dir_create file_rename"
test_0()
{
	for RACER_PROG in $ALL_RACER_PROGS; do
		RACER_PROGS=$RACER_PROG run_racer 1
	done
}
run_test 0 "run [$ALL_RACER_PROGS] individually for 1 thread"
# test_0 run successfully

test_1()
{
	for RACER_PROG in $ALL_RACER_PROGS; do
		RACER_PROGS=$RACER_PROG run_racer 2
	done
}
run_test 1 "run [$ALL_RACER_PROGS] individually for 2 threads"
# test_1 run successfully

test_2()
{
	for RACER_PROG in $ALL_RACER_PROGS; do
		RACER_PROGS=$RACER_PROG run_racer $MAX_RACER_THREADS
	done
}
run_test 2 "run [$ALL_RACER_PROGS] individually for $MAX_RACER_THREADS threads"
# test_2 will fail

test_3()
{
	RACER_PROGS=dir_create run_racer 2
}
run_test 3 "run [dir_create] for 2 threads"
# test_3 run successfully

test_4()
{
        RACER_PROGS=file_create run_racer 2
}
run_test 4 "run [file_create] for 2 threads"

test_5()
{
        RACER_PROGS=file_concat run_racer 2
}
run_test 5 "run [file_concat] for 2 threads"
# test_5 will fail

test_6()
{
        RACER_PROGS=file_concat_simpler run_racer 2
}
run_test 6 "run [file_concat_simpler] for 2 threads"
# test_6 will fail

test_7()
{
        RACER_PROGS=file_concat_simplest run_racer 2
}
run_test 7 "run [file_concat_simplest] for 2 threads"
# test_7 run successfully

test_8()
{
        RACER_PROGS=file_concat_same run_racer 2
}
run_test 8 "run [file_concat_same] for 2 threads"
# test_8 run successfully

test_9()
{
        RACER_PROGS=file_concat_opposite run_racer 2
}
run_test 9 "run [file_concat_opposite] for 2 threads"
# test_9 run successfully

test_10()
{
        RACER_PROGS="file_rename file_create" run_racer 1
}
run_test 10 "run [file_rename file_create] for 1 threads"

test_11()
{
        RACER_PROGS="file_rename" run_racer 2
}
run_test 11 "run file_create for 2 threads"

test_12()
{
        RACER_PROGS="file_create file_rm file_list" run_racer 2
}
run_test 12 "run [file_create file_rm file_list] for 2 threads"
#OK

test_13()
{
        RACER_PROGS=$ALL_RACER_PROGS run_racer 2
}
run_test 13 "run [$ALL_RACER_PROGS] for 2 threads"
#BUG


#file_create dir_create file_rm file_rename file_link file_symlink file_list file_concat
test_14()
{
        RACER_PROGS="dir_create file_rm file_rename file_link file_symlink file_list file_concat" run_racer 2
}
run_test 14 "run [$ALL_RACER_PROGS] for 2 threads"
#NO file_create OK

test_15()
{
        RACER_PROGS="file_create file_rm file_rename file_link file_symlink file_list file_concat" run_racer 2
}
run_test 15 "run [$ALL_RACER_PROGS] for 2 threads"
#NO dir_create OK

test_16()
{
        RACER_PROGS="file_create dir_create file_rm file_rename file_list file_concat" run_racer 2
}
run_test 16 "run [$ALL_RACER_PROGS] for 2 threads"
#

test_100()
{
	RACER_PROGS=$ALL_RACER_PROGS run_racer $MAX_RACER_THREADS
}
run_test 100 "run [$ALL_RACER_PROGS] for $MAX_RACER_THREADS threads"

cleanup_all
echo "=== $0: completed ==="
exit $RC
