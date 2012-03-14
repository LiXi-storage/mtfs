#! /bin/bash

if [ "$1" = "-c" ]; then
	make clean
	make distclean
fi

aclocal -I build/autoconf/ -I hrfs/autoconf -I libcfs/autoconf
autoconf
automake
LDFLAGS=-L/usr/local/lib ./configure --with-linux=/home/lixi/linux-2.6.18-238.19.1 --disable-swgfs-support --with-lustre=/home/lixi/lustre-2.1.52
make
