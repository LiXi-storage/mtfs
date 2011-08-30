#!/bin/bash

DIR=$1
MAX=$2

concat(){
    cat $DIR/$new_file >> $DIR/$new_file
}

while /bin/true ; do 
    file=$((RANDOM % MAX))
    new_file=$((RANDOM % MAX))
    concat 2> /dev/null
done
