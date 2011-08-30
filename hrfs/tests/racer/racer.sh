#!/bin/bash

MAX_FILES=${MAX_FILES:-20}
DIR=${DIR:-$1}
DIR=${DIR:-"/mnt/hrfs/racer"}
DURATION=${DURATION:-$((60*5))}

NUM_THREADS=${NUM_THREADS:-$2}
NUM_THREADS=${NUM_THREADS:-3}
RACER_PROGS=${RACER_PROGS:-"file_create dir_create file_rm file_rename file_link file_symlink file_list file_concat"}
mkdir -p $DIR

racer_cleanup()
{
	for P in $RACER_PROGS; do
		killall $P.sh
	done
	trap 0
}

echo "Running {[$RACER_PROGS] * $NUM_THREADS} for $DURATION seconds. CTRL-C to exit"
trap "
	echo \"Cleaning up\" 
	racer_cleanup
	exit 0
" 2 15

cd `dirname $0`
#
# LIXI: add THREAD_ID is back-compatible
#
THREAD_ID=0
for N in `seq 1 $NUM_THREADS`; do
	for P in $RACER_PROGS; do
		./$P.sh $DIR $MAX_FILES $THREAD_ID&
	done
	THREAD_ID=$((THREAD_ID + 1))
done

sleep $DURATION
racer_cleanup

# Check our to see whether our test DIR is still available.
df $DIR
RC=$?
if [ $RC -eq 0 ]; then
    echo "Survived [$RACER_PROGS] tests for $DURATION seconds."
fi
exit $RC
