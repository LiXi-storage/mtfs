#
# There are four ways this Makefile can be called:
#
# 
# 1.  As a subdirectory from the toplevel, for automake
#
# 2.  A wrapper around the kernel's makefile when building modules, to
#     possibly override the .config file
#
# 3.  At configure time, as the toplevel module dir for building
#     kernel tests
#
# 4.  At configure time, to determine the kernel's idea of $(ARCH)
#

ifeq ($(PATCHLEVEL),)

ifeq ($(MTFS_LINUX_CONFIG),)

# case #1
include autoMakefile

else

# case #2
# Note that this comes from make -C $LINUX -f $MTFS/build/Makefile
# so "include Makefile" below includes $LINUX/Makefile, not this file
include $(MTFS_LINUX_CONFIG)
include Makefile

endif # MTFS_LINUX_CONFIG

else # PATCHLEVEL

# case 3

ifneq ($(MTFS_KERNEL_TEST),)
ifeq ($(PATCHLEVEL),4)
all: $(MTFS_KERNEL_TEST)
else
extra-y = $(MTFS_KERNEL_TEST)
endif
endif

obj-m := conftest.o

ifeq ($(PATCHLEVEL),4)
include $(TOPDIR)/Rules.make
endif

endif # PATCHLEVEL

# case 4

echoarch:
	echo $(ARCH) >$(ARCHFILE)
