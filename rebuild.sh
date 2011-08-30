#! /bin/bash

if [ "$1" = "-c" ]; then
	make clean
	make distclean
fi

aclocal -I build/autoconf/ -I hrfs/autoconf -I libcfs/autoconf
autoconf
automake
LDFLAGS=-L/usr/local/lib ./configure  --with-swgfs=/root/hrfs/swgfs-1.8.1.1_client --with-linux=/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp
make
