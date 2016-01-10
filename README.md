Overview
MTFS(MulTi File System) is a stackable file system, which takes multiple directories as its lower branches in order to add new features to existing file systems, including  replication, unification, tracing and more.
Mtfs is originally designed and mainly used as a reliability improvement mechanism for Lustre file system. It is also aimed to become a general platform for stackable file system development.

Features
 * Can be mounted on top of any combination of network, distributed, disk-based, and memory-based file systems. A series of file systems can be used as its lower file system including Lustre, ext2(3, 4), nfs, ntfs-3g and tmpfs.
 * Can be easily added support for a new kind of file system.
 * Can take full advantage of characteristic features of a specific lower file system.
 * Have a test framework with plenty of test suites for the POSIX I/O interface, which is very helpful for its correctness.

When used as a reliability improvement mechanism for storage systems, mtfs is able to:
 * Replicate file and directories across multiple branches.
 * Selfheal if any branch is broken.
 * Replicate file synchronously or asynchronously.
 * Apply various replication policies according to file-name pattern.

What's New
Mtfs is under heavy development. Changes are taking place all the time. Complete sourcecode packages are comming soon.

Quick start
To use mtfs, you simply need to do the following:
 * Check whether the file system types of lower directories are supported by mtfs
 * Install the kernel modules and userspace utils of mtfs
 * Mount mtfs file system to an existing mount point

Mount command is simple. For example:
mount -t mtfs /path/to/dir1:/path/to/dir2 /mnt/mtfs
