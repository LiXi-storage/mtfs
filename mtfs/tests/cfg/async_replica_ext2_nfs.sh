# Base define area
SUBJECT_NAME=${SUBJECT_NAME:-async_replica}

# Mount lowerfs area
MOUNT_LOWERFS=${MOUNT_LOWERFS:-yes}
LOWERFS_NAME_ARRAY=([0]=ext2
                    [1]=nfs)
LOWERFS_MNT_ARRAY=([0]="/mnt/${LOWERFS_NAME_ARRAY[0]}0"
                   [1]="/mnt/${LOWERFS_NAME_ARRAY[1]}1")
LOWERFS_MOUNT_CMD_ARRAY=([0]="mount"
                         [1]="mount")
LOWERFS_DEV_ARRAY=([0]="/dev/sda4"
                   [1]="192.168.2.103:/mnt")
LOWERFS_OPTION_ARRAY=([0]=""
                      [1]="-o vers=3")

# Lowerfs module area
LOWERFS_MODULE_COMMON_PATH=${LOWERFS_COMMON_DIR:-`pwd`/../lowerfs}
LOWERFS_MODULE_ARRAY=([0]=mtfs_lowerfs_ext
                      [1]=mtfs_lowerfs_nfs)
LOWERFS_MODULE_PATH_ARRAY=([0]="${LOWERFS_MODULE_COMMON_PATH}/ext/${LOWERFS_MODULE_ARRAY[0]}.ko"
                           [1]="${LOWERFS_MODULE_COMMON_PATH}/nfs/${LOWERFS_MODULE_ARRAY[1]}.ko")

MTFS_DIR=${MTFS_DIR:-`pwd`/../mtfs}
UTILS_DIR=${UTILS_DIR:-`pwd`/../utils}
DEBUG_DIR=${DEBUG_DIR:-`pwd`/../debug}
DEBUG_MODULE=${DEBUG_MODULE:-mtfs_debug}
DEBUG_MODULE_PATH=${DEBUG_MODULE_PATH:-${DEBUG_DIR}/${DEBUG_MODULE}.ko}
MTFS_MODULE=${MTFS_MODULE:-mtfs}
MTFS_MODULE_PATH=${MTFS_MODULE_PATH:-${MTFS_DIR}/${MTFS_MODULE}.ko}

SUBJECT_NAME_UPPER=${SUBJECT_NAME_UPPER:-`echo ${SUBJECT_NAME} | awk '{printf("%s"), toupper($0)}'`}
SUBJECT_DIR=${SUBJECT_DIR:-`pwd`/../subjects/${SUBJECT_NAME}}
SUBJECT_MODULE=${SUBJECT_MODULE:-mtfs_subject_${SUBJECT_NAME}}
SUBJECT_MODULE_PATH=${SUBJECT_MODULE_PATH:-${SUBJECT_DIR}/${SUBJECT_MODULE}.ko}

JUNCTION_COMMON_PATH=${JUNCTION_COMMON_PATH:-`pwd`/../junctions}
JUNCTION_MODULE=${JUNCTION_MODULE:-mtfs_junction_async_replica_ext_nfs}
JUNCTION_MODULE_PATH=${JUNCTION_MODULE_PATH:-${JUNCTION_COMMON_PATH}/async_replica_ext_nfs/${JUNCTION_MODULE}.ko}
MTFS_OPTION=${MTFS_OPTION:-"-o subject=${SUBJECT_NAME_UPPER},checksum"}

# Mtfs Mount area
BRANCH_DIR_ARRAY=([0]="${LOWERFS_MNT_ARRAY[0]}/mtfs/dir0"
                  [1]="${LOWERFS_MNT_ARRAY[1]}/mtfs/dir1")
MTFS_MNT1=${MTFS_MNT1:-/mnt/mtfs1}

DIR_SUB=${DIR_SUB:-test}
DIR=${DIR:-${MTFS_MNT1}/${DIR_SUB}}
DIR1=${DIR1:-${MTFS_MNT1}/${DIR_SUB}}
BRANCH_0=${BRANCH_0:-${MTFS_DIR1}/${DIR_SUB}}
BRANCH_1=${BRANCH_1:-${MTFS_DIR2}/${DIR_SUB}}

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
DEBUG=${DEBUG:-no}

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

#
# Skip tests that abandon branch[0].
#
SKIP_ABANDON_BRANCH0=${SKIP_ABANDON_BRANCH0:-yes}

#
# Skip tests that abandon branch[1].
#
SKIP_ABANDON_BRANCH1=${SKIP_ABANDON_BRANCH1:-no}
