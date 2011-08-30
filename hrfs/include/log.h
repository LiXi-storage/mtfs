/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef _HRFS_LOG_H_
#define _HRFS_LOG_H_
#include <pthread.h>
#include <stdio.h>

#define HRFS_LOG_FILENAME "/var/log/hrfs.log"

#define HLOG(level, format...) do { \
	hrfs_print_log(__FILE__, __FUNCTION__, __LINE__, level, ##format); \
} while(0)

#define HLOG_ERROR(format...) HLOG(HRFS_LOGLEVEL_ERROR, ##format)
#define HLOG_DEBUG(format...) HLOG(HRFS_LOGLEVEL_DEBUG, ##format)
#define HLOG_WARN(format...) HLOG(HRFS_LOGLEVEL_WARNING, ##format)

typedef enum {
	HRFS_LOGLEVEL_NONE,
	HRFS_LOGLEVEL_CRITICAL,   /* fatal errors */
	HRFS_LOGLEVEL_ERROR,      /* major failures (not necessarily fatal) */
	HRFS_LOGLEVEL_WARNING,    /* info about normal operation */
	HRFS_LOGLEVEL_INFO,       /* Normal information */
	HRFS_LOGLEVEL_DEBUG,      /* internal errors */
    HRFS_LOGLEVEL_TRACE,      /* full trace of operation */
} hrfs_loglevel_t;

typedef struct hrfs_log {
	pthread_mutex_t logfile_mutex;
	char *filename;
	FILE *logfile;
	hrfs_loglevel_t loglevel;
} hrfs_log_t;

int _hrfs_log_init(hrfs_log_t *log, const char *file, hrfs_loglevel_t default_level);
int hrfs_log_init(const char *file, hrfs_loglevel_t level);
void _hrfs_log_finit(hrfs_log_t *log);
void hrfs_log_finit(void);

void _hrfs_print_log(hrfs_log_t *log, const char *msg);
int
hrfs_print_log(const char *file, const char *function, int line,
	 hrfs_loglevel_t level, const char *fmt, ...);
char *combine_str(const char *str1, const char *str2);
#endif //_HRFS_LOG_H_
