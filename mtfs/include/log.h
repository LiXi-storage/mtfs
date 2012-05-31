/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_LOG_H_
#define _MTFS_LOG_H_
#include <pthread.h>
#include <stdio.h>

#define MTFS_LOG_FILENAME "/var/log/mtfs.log"

#define HLOG(level, format...) do { \
	mtfs_print_log(__FILE__, __FUNCTION__, __LINE__, level, ##format); \
} while(0)

#define HLOG_ERROR(format...) HLOG(MTFS_LOGLEVEL_ERROR, ##format)
#define HLOG_DEBUG(format...) HLOG(MTFS_LOGLEVEL_DEBUG, ##format)
#define HLOG_WARN(format...) HLOG(MTFS_LOGLEVEL_WARNING, ##format)

typedef enum {
	MTFS_LOGLEVEL_NONE,
	MTFS_LOGLEVEL_CRITICAL,   /* fatal errors */
	MTFS_LOGLEVEL_ERROR,      /* major failures (not necessarily fatal) */
	MTFS_LOGLEVEL_WARNING,    /* info about normal operation */
	MTFS_LOGLEVEL_INFO,       /* Normal information */
	MTFS_LOGLEVEL_DEBUG,      /* internal errors */
    MTFS_LOGLEVEL_TRACE,      /* full trace of operation */
} mtfs_loglevel_t;

typedef struct mtfs_log {
	pthread_mutex_t logfile_mutex;
	char *filename;
	FILE *logfile;
	mtfs_loglevel_t loglevel;
} mtfs_log_t;

int _mtfs_log_init(mtfs_log_t *log, const char *file, mtfs_loglevel_t default_level);
int mtfs_log_init(const char *file, mtfs_loglevel_t level);
void _mtfs_log_finit(mtfs_log_t *log);
void mtfs_log_finit(void);

void _mtfs_print_log(mtfs_log_t *log, const char *msg);
int
mtfs_print_log(const char *file, const char *function, int line,
	 mtfs_loglevel_t level, const char *fmt, ...);
char *combine_str(const char *str1, const char *str2);
#endif //_MTFS_LOG_H_
