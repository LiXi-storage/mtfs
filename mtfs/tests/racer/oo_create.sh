#!/bin/bash

DIR=$1
MAX=1

create(){
    echo "asdf" > $DIR/1
}

while /bin/true ; do 
    create 2> /dev/null
done
