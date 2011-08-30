#!/bin/bash

DIR=$1
MAX=$2
THREAD_ID=$3

if [ "$THREAD_ID" = "0" ]; then
	file=0
	new_file=1
else
	file=1
	new_file=0
fi

concat(){
	cat $DIR/$file >> $DIR/$new_file
}

while /bin/true ; do 
    concat 2> /dev/null
done
