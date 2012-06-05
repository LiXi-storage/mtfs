#ifndef __MTFS_CFG_H__
#define __MTFS_CFG_H__

#include <inttypes.h>

#define _CONFIG_MAKE_PROTOTYPE(fname,type) type cfg_get##fname(const char *name,type def)

int cfg_load (const char *fname,int logundefined);

_CONFIG_MAKE_PROTOTYPE(str,char*);
_CONFIG_MAKE_PROTOTYPE(num,int);
_CONFIG_MAKE_PROTOTYPE(uint8,uint8_t);
_CONFIG_MAKE_PROTOTYPE(int8,int8_t);
_CONFIG_MAKE_PROTOTYPE(uint16,uint16_t);
_CONFIG_MAKE_PROTOTYPE(int16,int16_t);
_CONFIG_MAKE_PROTOTYPE(uint32,uint32_t);
_CONFIG_MAKE_PROTOTYPE(int32,int32_t);
_CONFIG_MAKE_PROTOTYPE(uint64,uint64_t);
_CONFIG_MAKE_PROTOTYPE(int64,int64_t);
_CONFIG_MAKE_PROTOTYPE(double,double);

#endif /* __MTFS_CFG_H__ */
