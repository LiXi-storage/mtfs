/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_RAID_H__
#define __HRFS_RAID_H__

#define RAID_TYPE_NONE 0x0fff
#define RAID_TYPE_RAID0 0
#define RAID_TYPE_RAID1 1
#define RAID_TYPE_RAID5 5
#define RAID_TYPE_MAX 15

typedef int raid_type_t;

static inline char *raid_type2string(raid_type_t raid_type)
{
	switch(raid_type) {
	case RAID_TYPE_RAID0:	
		return "RAID_TYPE_RAID0";
	case RAID_TYPE_RAID1:	
		return "RAID_TYPE_RAID1";		
//	case RAID_TYPE_RAID5:	
//		return "RAID_TYPE_RAID5";	
	default:
		return "UNKNOWN";	
	}
	return "UNKNOWN";	
}

static inline int raid_type_is_valid(raid_type_t raid_type)
{
	int ret = 1;
	
	switch(raid_type) {
	case RAID_TYPE_RAID0:
		break;
	case RAID_TYPE_RAID1:	
		break;
//	case RAID_TYPE_RAID5:	
//		break;
	default:
		ret = 0;
		break;
	}

	return ret;	
}

#endif /* __HRFS_RAID_H__ */
