# Base define area
LOWERFS_NAME=${LOWERFS_MODULE:-nfs}
REMOVE_LOWERFS_MODULE=${REMOVE_LOWERFS_MODULE:-no}
MOUNT_LOWERFS=${MOUNT_LOWERFS:-yes}
LOWERFS_MODULE=${LOWERFS_MODULE:-${LOWERFS_NAME}}
LOWERFS_MOUNT_CMD=${LOWERFS_MOUNT_CMD:-"/bin/mount -t ${LOWERFS_NAME}"}
MTFS_DIR=${MTFS_DIR:-`pwd`/../mtfs}
UTILS_DIR=${UTILS_DIR:-`pwd`/../utils}
MTFS_DEBUG=${MTFS_DEBUG:-20}
MTFS_MODULE=${MTFS_MODULE:-mtfs}
MTFS_MODULE_PATH=${MTFS_MODULE_PATH:-${MTFS_DIR}/${MTFS_MODULE}.ko}
SUPPORT_DIR=${SUPPORT_DIR:-`pwd`/../${LOWERFS_NAME}_support}
SUPPORT_MODULE=${SUPPORT_MODULE:-mtfs_${LOWERFS_NAME}}
SUPPORT_MODULE_PATH=${SUPPORT_MODULE_PATH:-${SUPPORT_DIR}/${SUPPORT_MODULE}.ko}

# Mount point 1 area
LOWERFS_MNT1=${LOWERFS_MNT1:-/mnt/${LOWERFS_NAME}1}
LOWERFS_DEV=${LOWERFS_DEV:-120.219.90.150:/home/lixi/export/}
LOWERFS_OPTION=${LOWERFS_OPTION:-""}
MTFS_DIR1=${MTFS_DIR1:-${LOWERFS_MNT1}/mtfs/dir1}
MTFS_DIR2=${MTFS_DIR2:-${LOWERFS_MNT1}/mtfs/dir2}
MTFS_DEV=${MTFS_DEV:-${MTFS_DIR1}:${MTFS_DIR2}}
MTFS_MNT1=${MTFS_MNT1:-/mnt/mtfs1}

DIR_SUB=${DIR_SUB:-test}
DIR=${DIR:-${MTFS_MNT1}/${DIR_SUB}}
DIR1=${DIR1:-${MTFS_MNT1}/${DIR_SUB}}

# Mount point 1 area
LOWERFS_MNT2=${LOWERFS_MNT2:-/mnt/${LOWERFS_NAME}2}
MTFS_MNT2=${MTFS_MNT2:-/mnt/mtfs2}
MTFS2_DIR1=${MTFS2_DIR1:-${LOWERFS_MNT2}/mtfs/dir1}
MTFS2_DIR2=${MTFS2_DIR2:-${LOWERFS_MNT2}/mtfs/dir2}
MTFS2_DEV=${MTFS2_DEV:-${MTFS2_DIR1}:${MTFS2_DIR2}}
DIR2=${DIR2:-${MTFS_MNT2}/${DIR_SUB}}

# Test control area
FAIL_ON_ERROR=${FAIL_ON_ERROR:-yes}
CLEANUP_PER_TEST=${CLEANUP_PER_TEST:-yes}
CLIENTS=${CLIENTS:-$(hostname)}
PDSH=${PDSH:-ssh}
DETECT_LEAK=${DETECT_LEAK-yes}
MTFS_LOG=${MTFS_LOG-/tmp/mtfs.log}

# Lowerfs feature define area
LOWERFS_DIR_INVALID_WHEN_REMOVED=${LOWERFS_DIR_INVALID_WHEN_REMOVED:-no}
LOWERFS_SUPPORT_FLOCK=${LOWERFS_SUPPORT_FLOCK:-yes}
LOWERFS_SUPPORT_FCNTL=${LOWERFS_SUPPORT_FCNTL:-yes}
LOWERFS_SUPPORT_LOCKF=${LOWERFS_SUPPORT_LOCKF:-yes}
#
# Whether the filesytem bave a real backup device
# If so, lowerfs's multiple mount points will have same super block.
# Which means mtfs should not mount on same directories of these mount points for multiple times.
# And because of this, all multi_mnt tests are unable to run.
#
LOWERFS_HAVE_DEV=${LOWERFS_HAVE_DEV:-yes} 

#
# Whether the lowerfs support chattr ioctl
#
LOWERFS_SUPPORT_CHATTR=${LOWERFS_SUPPORT_CHATTR:-no}

#
# Whether the lowerfs support directio
#
LOWERFS_SUPPORT_DIRECTIO=${LOWERFS_SUPPORT_DIRECTIO:-yes}

#
# When dir is opened and unlinked, whether it will be unreacheable at all.
# Especially for nfs and fuse. Other filesystems usually keep the dentry untill the dir is closed.
#
LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED=${LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED:-yes}

#
# Nfs will let the server control timestamp.
#
LOWERFS_STRICT_TIMESTAMP=${LOWERFS_STRICT_TIMESTAMP:-no}

#
# Skip tests that abandon branch[0].
#
SKIP_ABANDON_BRANCH0=${SKIP_ABANDON_BRANCH0:-no}
