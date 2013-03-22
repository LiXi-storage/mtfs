# MTFS test Makefile
AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS)
AM_LDFLAGS :=

SUBDIRS = src pjd_fstest
#DIST_SUBDIRS := 
noinst_DATA = 
noinst_SCRIPTS = acceptance-small.sh test-framework.sh
noinst_SCRIPTS += compile.sh example.sh mount.sh multi_mnt.sh pjd_fstest.sh posix.sh racer.sh raid.sh setup.sh single_branch.sh
noinst_SCRIPTS += lockorder.sh
nobase_noinst_SCRIPTS = cfg/trace_ext2.sh cfg/sync_replica_ext2.sh cfg/sync_replica_ext3.sh
nobase_noinst_SCRIPTS += cfg/sync_replica_ext4.sh cfg/async_replica_ext2.sh cfg/async_cache_ext2.sh
nobase_noinst_SCRIPTS += cfg/sync_replica_tmpfs.sh cfg/sync_replica_nfs.sh cfg/lustre.sh 
nobase_noinst_SCRIPTS += cfg/async_replica_ext2_nfs.sh
nobase_noinst_SCRIPTS += racer/dir_create.sh racer/file_create.sh racer/file_list.sh
nobase_noinst_SCRIPTS += racer/file_rm.sh racer/racer.sh racer/file_concat.sh
nobase_noinst_SCRIPTS += racer/file_link.sh racer/file_rename.sh racer/file_symlink.sh
nobase_noinst_DATA = 


EXTRA_DIST = $(noinst_SCRIPTS) $(noinst_DATA) \
             $(nobase_noinst_SCRIPTS) $(nobase_noinst_DATA)
if TESTS
testdir = $(libdir)/mtfs/tests
test_SCRIPTS = $(noinst_SCRIPTS)
nobase_test_SCRIPTS = $(nobase_noinst_SCRIPTS)
nobase_test_DATA = $(nobase_noinst_DATA)
endif
