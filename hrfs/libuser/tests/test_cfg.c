#include <cfg.h>
#include <debug.h>
#include <stdlib.h>

#define MAX_TYPE_LENGTH 1024
#define MAX_PARAM_LENGTH 1024

int main()
{
	int logundefined = 0; /* Disable noisy log */
	char *type = NULL;
	char *param = NULL;
	char *string = NULL;
	int number = 0;
	int8_t int8 = 0;
	uint8_t uint8 = 0;
	int16_t int16 = 0;	
	uint16_t uint16 = 0;
	int32_t int32 = 0;
	uint32_t uint32 = 0;
	int64_t int64 = 0;
	uint64_t uint64 = 0;
	double double1 = 0;
	int ret = 0;

        if (cfg_load("/dev/stdin", logundefined) == -1) {
                HERROR("can't load config file - using defaults\n");
                ret = -EINVAL;
                goto out;
        }

	type = cfg_getstr("type", "unknown");
	if (strcmp(type, "unknown") == 0) {
		HERROR("type is unknown\n");
		ret = -EINVAL;
		goto out;
	}

	param = cfg_getstr("param", "unknown");
	if (strcmp(param, "unknown") == 0) {
		HERROR("type is unknown\n");
		ret = -EINVAL;
		goto out;
	}

	HDEBUG("type %s, param %s\n", type, param);

	if (strcmp(type, "string") == 0) {
		string = cfg_getstr(param, "XXXXXXX");
		HPRINT("%s = %s\n", param, string);
		free(string);
	} else if (strcmp(type, "number") == 0) {
		number = cfg_getnum(param, -66666666);
		HPRINT("%s = %d\n", param, number);
	} else if (strcmp(type, "int8") == 0) {
		int8 = cfg_getint8(param, -66);
		HPRINT("%s = %"PRId8"\n", param, int8);
	} else if (strcmp(type, "uint8") == 0) {
		uint8 = cfg_getuint8(param, 66);
		HPRINT("%s = %"PRIu8"\n", param, uint8);	
	} else if (strcmp(type, "int16") == 0) {
		int16 = cfg_getint16(param, -6666);
		HPRINT("%s = %"PRId16"\n", param, int16);
	} else if (strcmp(type, "uint16") == 0) {
		uint16 = cfg_getuint16(param, 6666);
		HPRINT("%s = %"PRIu16"\n", param, uint16);
	} else if (strcmp(type, "int32") == 0) {
		int32 = cfg_getint32(param, -66666666);
		HPRINT("%s = %"PRId32"\n", param, int32);
	} else if (strcmp(type, "uint32") == 0) {
		uint32 = cfg_getuint32(param, 66666666);	
		HPRINT("%s = %"PRIu32"\n", param, uint32);
	} else if (strcmp(type, "int64") == 0) {
		int64 = cfg_getint64(param, -6666666666666666);	
		HPRINT("%s = %"PRId64"\n", param, int64);
	} else if (strcmp(type, "uint64") == 0) {
		uint64 = cfg_getuint64(param, 6666666666666666);
		HPRINT("%s = %"PRIu64"\n", param, uint64);	
	} else if (strcmp(type, "double") == 0) {
		double1 = cfg_getdouble(param, -0.6666666666666666);
		HPRINT("%s = %.3lf\n", param, double1);
	} else {
		HERROR("unknown type %s\n", type);
	}
	free(type);
	free(param);
out:
	return ret;
}
