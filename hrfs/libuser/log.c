/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <stdarg.h>
#include "log.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <debug.h>

hrfs_log_t hrfs_global_log;

char *combine_str(const char *str1, const char *str2)
{
	size_t len  = 0;
	char *msg  = NULL;
	
	len = strlen(str1);
	msg = malloc(len + strlen (str2) + 1);
	if (msg == NULL) {
		goto err;
	}
	strcpy (msg, str1);
	strcpy (msg + len, str2);
err:
	return msg;
}

void _hrfs_print_log(hrfs_log_t *log, const char *msg)
{
	pthread_mutex_lock(&log->logfile_mutex);
	{
		fprintf(log->logfile, "%s", msg);
		fflush(log->logfile);
	}
	pthread_mutex_unlock(&log->logfile_mutex);
}

int hrfs_print_log(const char *file, const char *function, int line,
                   hrfs_loglevel_t level, const char *fmt, ...)
{
	time_t utime = 0;
	struct tm *tm = NULL;
	char timestr[256];
	char *str1 = NULL;
	char *str2 = NULL;
	char *msg  = NULL;
	const char  *basename = NULL;	
	int ret  = 0;
	va_list ap;
	static char *level_strings[] = {"",  /* NONE */
				"C", /* CRITICAL */
				"E", /* ERROR */
				"W", /* WARNING */
				"N", /* NORMAL */
				"D", /* DEBUG */
				"T", /* TRACE */
				""};

	if (!file || !function || !line || !fmt) {
		HERROR("logging: %s:%s():%d: invalid argument\n", 
		       __FILE__, __PRETTY_FUNCTION__, __LINE__);
		ret = -1;
		goto arg_err;
	}

	if (level > hrfs_global_log.loglevel) {
		goto arg_err;
	}

	utime = time (NULL);
	tm = localtime (&utime);
	strftime(timestr, 256, "%Y-%m-%d %H:%M:%S", tm);

	basename = strrchr(file, '/');
	if (basename) {
		basename++;
	} else {
		basename = file;
	}
	ret = asprintf(&str1, "[%s] %s [%s:%d:%s] ",
	               timestr, level_strings[level],
	               basename, line, function);
	if (-1 == ret) {
		goto free_err;
	}

	va_start (ap, fmt);
	{
		ret = vasprintf (&str2, fmt, ap);
		if (-1 == ret) {
			goto free_err;
		}
	}
	va_end (ap);
	msg = combine_str(str1, str2);
	if (msg == NULL) {
		goto free_err;
	}

	_hrfs_print_log(&hrfs_global_log, msg);
free_err:
	if (msg) {
		free(msg);
	}
	if (str1) {
		free(str1);
	}
	if (str2) {
		free(str2);
	}
arg_err:
	return ret;
}

/* logs that level bigger than $default_devel is dropped */
int _hrfs_log_init(hrfs_log_t *log, const char *file, hrfs_loglevel_t default_level)
{
	if (!log){
		HERROR("no log specified\n");
		return -1;
	}

	if (!file){
		HERROR("no filename specified\n");
		return -1;
	}

	pthread_mutex_init(&log->logfile_mutex, NULL);

	log->filename = strdup(file);
	if (!log->filename) {
		HERROR ("strdup error\n");
		return -1;
	}

	log->logfile = fopen(file, "a");
	if (log->logfile == NULL){
		HERROR("failed to open logfile \"%s\": %s\n", file, strerror (errno));
		return -1;
	}

	log->loglevel = default_level;
	return 0;
}

int hrfs_log_init(const char *file, hrfs_loglevel_t default_level)
{
	return _hrfs_log_init(&hrfs_global_log, file, default_level);
}

void _hrfs_log_finit(hrfs_log_t *log)
{
	pthread_mutex_destroy(&log->logfile_mutex);

	free(log->filename);
	fclose(log->logfile);
}

void hrfs_log_finit(void)
{
	_hrfs_log_finit(&hrfs_global_log);
}
