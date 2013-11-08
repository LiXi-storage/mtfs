# Base define area
SUBJECT_NAME=${SUBJECT_NAME:-sync_replica}
LOWERFS_NAME=${LOWERFS_MODULE:-tmpfs}
REMOVE_LOWERFS_MODULE=${REMOVE_LOWERFS_MODULE:-no}
MOUNT_LOWERFS=${MOUNT_LOWERFS:-no}
LOWERFS_MODULE=${LOWERFS_MODULE:-${LOWERFS_NAME}}
LOWERFS_MOUNT_CMD=${LOWERFS_MOUNT_CMD:-undefined}
MTFS_DIR=${MTFS_DIR:-`pwd`/../mtfs}
UTILS_DIR=${UTILS_DIR:-`pwd`/../utils}
DEBUG_DIR=${DEBUG_DIR:-`pwd`/../debug}
DEBUG_MODULE=${DEBUG_MODULE:-mtfs_debug}
DEBUG_MODULE_PATH=${DEBUG_MODULE_PATH:-${DEBUG_DIR}/${DEBUG_MODULE}.ko}
MTFS_MODULE=${MTFS_MODULE:-mtfs}
MTFS_MODULE_PATH=${MTFS_MODULE_PATH:-${MTFS_DIR}/${MTFS_MODULE}.ko}
LOWERFS_SUPPORT_DIR=${LOWERFS_SUPPORT_DIR:-`pwd`/../lowerfs/${LOWERFS_NAME}}
LOWERFS_SUPPORT_MODULE=${LOWERFS_SUPPORT_MODULE:-mtfs_lowerfs_tmpfs}
LOWERFS_SUPPORT_MODULE_PATH=${LOWERFS_SUPPORT_MODULE_PATH:-${LOWERFS_SUPPORT_DIR}/${LOWERFS_SUPPORT_MODULE}.ko}
SUBJECT_NAME_UPPER=${SUBJECT_NAME_UPPER:-`echo ${SUBJECT_NAME} | awk '{printf("%s"), toupper($0)}'`}
SUBJECT_DIR=${SUBJECT_DIR:-`pwd`/../subjects/${SUBJECT_NAME}}
SUBJECT_MODULE=${SUBJECT_MODULE:-mtfs_subject_${SUBJECT_NAME}}
SUBJECT_MODULE_PATH=${SUBJECT_MODULE_PATH:-${SUBJECT_DIR}/${SUBJECT_MODULE}.ko}
JUNCTION_DIR=${JUNCTION_DIR:-`pwd`/../junctions/${SUBJECT_NAME}_ext}
JUNCTION_MODULE=${JUNCTION_MODULE:-mtfs_${SUBJECT_NAME}_ext}
JUNCTION_MODULE_PATH=${JUNCTION_MODULE_PATH:-${JUNCTION_DIR}/${JUNCTION_MODULE}.ko}
JUNCTION_NAME_UPPER=${SUBJECT_NAME_UPPER:-`echo ${SUBJECT_NAME} | awk '{printf("%s"), toupper($0)}'`}
MTFS_OPTION=${MTFS_OPTION:-"-o subject=${SUBJECT_NAME_UPPER}"}

# Mount point 1 area
LOWERFS_MNT1=${LOWERFS_MNT1:-/dev/shm}
LOWERFS_DEV=${LOWERFS_DEV:-undefined}
LOWERFS_OPTION=${LOWERFS_OPTION:-undefined}
MTFS_DIR1=${MTFS_DIR1:-${LOWERFS_MNT1}/mtfs/dir1}
MTFS_DIR2=${MTFS_DIR2:-${LOWERFS_MNT1}/mtfs/dir2}
MTFS_DEV=${MTFS_DEV:-${MTFS_DIR1}:${MTFS_DIR2}}
MTFS_MNT1=${MTFS_MNT1:-/mnt/mtfs1}

DIR_SUB=${DIR_SUB:-test}
DIR=${DIR:-${MTFS_MNT1}/${DIR_SUB}}
DIR1=${DIR1:-${MTFS_MNT1}/${DIR_SUB}}
BRANCH_0=${BRANCH_0:-${MTFS_DIR1}/${DIR_SUB}}
BRANCH_1=${BRANCH_1:-${MTFS_DIR2}/${DIR_SUB}}

# Mount point 2 area
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
DEBUG=${DEBUG:-yes}

# Lowerfs feature define area
LOWERFS_DIR_INVALID_WHEN_REMOVED=${LOWERFS_DIR_INVALID_WHEN_REMOVED:-no}
LOWERFS_SUPPORT_FLOCK=${LOWERFS_SUPPORT_FLOCK:-yes}
LOWERFS_SUPPORT_FCNTL=${LOWERFS_SUPPORT_FCNTL:-yes}
LOWERFS_SUPPORT_LOCKF=${LOWERFS_SUPPORT_LOCKF:-yes}
#
# Whether the lowerfs bave a real backup device
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
LOWERFS_SUPPORT_DIRECTIO=${LOWERFS_SUPPORT_DIRECTIO:-no}

#
# When dir is opened and unlinked, whether it will be unreacheable at all.
# Especially for nfs. Other filesystems usually keep the dentry untill the dir is closed.
#
LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED=${LOWERFS_DIR_UNREACHEABLE_WHEN_REMOVED_THOUGH_OPENED:-no}

#
# Nfs will let the server control timestamp.
#
LOWERFS_STRICT_TIMESTAMP=${LOWERFS_STRICT_TIMESTAMP:-yes}

#
# Skip tests that abandon branch[0].
#
SKIP_ABANDON_BRANCH0=${SKIP_ABANDON_BRANCH0:-no}

#
# Skip tests that abandon branch[1].
#
SKIP_ABANDON_BRANCH1=${SKIP_ABANDON_BRANCH1:-no}