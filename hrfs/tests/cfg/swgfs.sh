LOWERFS_NAME=${LOWERFS_MODULE:-swgfs}
REMOVE_LOWERFS_MODULE=${REMOVE_LOWERFS_MODULE:-no}
MOUNT_LOWERFS=${MOUNT_LOWERFS:-yes}
LOWERFS_MODULE=${LOWERFS_MODULE:-${LOWERFS_NAME}}
LOWERFS_MOUNT_CMD=${LOWERFS_MOUNT_CMD:-/sbin/mount.$LOWERFS_NAME}
HRFS_DIR=${HRFS_DIR:-`pwd`/../hrfs}
UTILS_DIR=${UTILS_DIR:-`pwd`/../utils}
HRFS_DEBUG=${HRFS_DEBUG:-20}
HRFS_MODULE=${HRFS_MODULE:-hrfs}
HRFS_MODULE_PATH=${HRFS_MODULE_PATH:-${HRFS_DIR}/${HRFS_MODULE}.ko}
SUPPORT_DIR=${SUPPORT_DIR:-`pwd`/../${LOWERFS_NAME}_support}
SUPPORT_MODULE=${SUPPORT_MODULE:-hrfs_${LOWERFS_NAME}}
SUPPORT_MODULE_PATH=${SUPPORT_MODULE_PATH:-${SUPPORT_DIR}/${SUPPORT_MODULE}.ko}

LOWERFS_MNT1=${LOWERFS_MNT1:-/mnt/${LOWERFS_NAME}1}
LOWERFS_DEV=${LOWERFS_DEV:-120.219.90.150@tcp:/lxfs}
LOWERFS_OPTION=${LOWERFS_OPTION:-"-o force"}
HRFS_DIR1=${HRFS_DIR1:-${LOWERFS_MNT1}/hrfs/dir1}
HRFS_DIR2=${HRFS_DIR2:-${LOWERFS_MNT1}/hrfs/dir2}
HRFS_DEV=${HRFS_DEV:-${HRFS_DIR1}:${HRFS_DIR2}}
HRFS_MNT1=${HRFS_MNT1:-/mnt/hrfs1}

DIR_SUB=${DIR_SUB:-test}
DIR=${DIR:-${HRFS_MNT1}/${DIR_SUB}}
DIR1=${DIR1:-${HRFS_MNT1}/${DIR_SUB}}

LOWERFS_MNT2=${LOWERFS_MNT2:-/mnt/${LOWERFS_NAME}2}
HRFS_MNT2=${HRFS_MNT2:-/mnt/hrfs2}
HRFS2_DIR1=${HRFS2_DIR1:-${LOWERFS_MNT2}/hrfs/dir1}
HRFS2_DIR2=${HRFS2_DIR2:-${LOWERFS_MNT2}/hrfs/dir2}
HRFS2_DEV=${HRFS2_DEV:-${HRFS2_DIR1}:${HRFS2_DIR2}}
DIR2=${DIR2:-${HRFS_MNT2}/${DIR_SUB}}

FAIL_ON_ERROR=${FAIL_ON_ERROR:-yes}
CLEANUP_PER_TEST=${CLEANUP_PER_TEST:-yes}
CLIENTS=${CLIENTS:-$(hostname)}
PDSH=${PDSH:-ssh}
DETECT_LEAK=${DETECT_LEAK-yes}
HRFS_LOG=${HRFS_LOG-/tmp/hrfs.log}

# lowerfs feature define area
LOWERFS_DIR_INVALID_WHEN_REMOVED=${LOWERFS_DIR_INVALID_WHEN_REMOVED:-yes}
LOWERFS_SUPPORT_FLOCK=${LOWERFS_SUPPORT_FLOCK:-undefined}
LOWERFS_SUPPORT_FCNTL=${LOWERFS_SUPPORT_FCNTL:-undefined}
LOWERFS_SUPPORT_LOCKF=${LOWERFS_SUPPORT_LOCKF:-undefined}

#
# Whether the filesytem bave a real backup device
# If so, lowerfs's multiple mount points will have same super block.
# Which means hrfs should not mount on same directories of these mount points for multiple times.
# And because of this, all multi_mnt tests are unable to run.
#
LOWERFS_HAVE_DEV=${LOWERFS_HAVE_DEV:-no} 

#
# Whether the lowerfs support chattr ioctl
#
LOWERFS_SUPPORT_CHATTR=${LOWERFS_SUPPORT_CHATTR:-yes}

#
# Whether the lowerfs support directio
#
LOWERFS_SUPPORT_DIRECTIO=${LOWERFS_SUPPORT_DIRECTIO:-yes}

#
# When dir is opened and unlinked, whether it will be unreacheable at all.
# Especially for nfs. Other filesystems usually keep the dentry untill the dir is closed.
#
LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED=${LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED:-no}

#
# Nfs will let the server control timestamp.
#
LOWERFS_STRICT_TIMESTAMP=${LOWERFS_STRICT_TIMESTAMP:-yes}
