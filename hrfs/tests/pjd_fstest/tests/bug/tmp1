#!/bin/sh
# Only for temporary test

desc="Only for temporary test"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..70"

n0=`namegen`
n1=`namegen`
n2=`namegen`
n3=`namegen`
n4=`namegen`

expect 0 mkdir ${n4} 0755
cdir=`pwd`
cd ${n4}


expect 0 mkdir ${n0} 0755
expect 0 chown ${n0} 65534 65534

expect 0 mkdir ${n1} 0755
expect 0 chmod ${n1} 01777

# User owns both: the sticky directory and the destination file.
expect 0 chown ${n1} 65534 65534

# !!!START!!!!
# User owns both: the sticky directory and the destination file.
expect 0 chown ${n1} 65534 65534
expect 0 -u 65534 -g 65534 mkdir ${n0}/${n2} 0755
expect 0 -u 65534 -g 65534 mkdir ${n1}/${n3} 0755
expect 0 -u 65534 -g 65534 rename ${n0}/${n2} ${n1}/${n3}
expect ENOENT lstat ${n0}/${n2} type
expect 0 rmdir ${n1}/${n3}
# User owns the sticky directory, but doesn't own the destination file.
expect 0 chown ${n1} 65534 65534
expect 0 -u 65534 -g 65534 mkdir ${n0}/${n2} 0755
expect 0 -u 65534 -g 65534 mkdir ${n1}/${n3} 0755
expect 0 -u 65534 -g 65534 rename ${n0}/${n2} ${n1}/${n3}
expect ENOENT lstat ${n0}/${n2} type
expect 0 rmdir ${n1}/${n3}

cd ${cdir}
expect 0 rmdir ${n4}
