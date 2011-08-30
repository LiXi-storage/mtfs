#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#


TESTS_DIR=${TESTS_DIR:-$(cd $(dirname $0); echo $PWD)}
echo $TESTS_DIR
prove -r -v $TESTS_DIR -d
