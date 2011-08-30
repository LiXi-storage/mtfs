/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef _ULIB_PARSER_H_
#define _ULIB_PARSER_H_
#include <unistd.h>

#ifdef HAVE_LIBREADLINE
#define READLINE_LIBRARY
#include <stdio.h>
#include <readline/readline.h>

/* completion_matches() is #if 0-ed out in modern glibc */

#ifndef completion_matches
#define completion_matches rl_completion_matches
#endif
extern void using_history(void);
extern void stifle_history(int);
extern void add_history(char *);
#endif /* HAVE_LIBREADLINE */


#define HISTORY	100		/* Don't let history grow unbounded    */
#define MAXARGS 512

#define CMD_COMPLETE	0
#define CMD_INCOMPLETE	1
#define CMD_NONE	2
#define CMD_AMBIG	3
#define CMD_HELP	4

typedef struct parser_cmd {
	char 	*pc_name;
	int 	(* pc_func)(int, char **);
	struct parser_cmd * pc_sub_cmd;
	char *pc_help;
} command_t;

typedef struct argcmd {
	char    *ac_name;
	int      (*ac_func)(int, char **);
	char     *ac_help;
} argcmd_t;

typedef struct network {
	char	*type;
	char	*server;
	int	port;
} network_t;

int  Parser_quit(int argc, char **argv);
void Parser_init(char *, command_t *);	/* Set prompt and load command list */
int Parser_commands(void);			/* Start the command parser */
void Parser_qhelp(int, char **);	/* Quick help routine */
int Parser_help(int, char **);		/* Detailed help routine */
void Parser_ignore_errors(int ignore);	/* Set the ignore errors flag */
void Parser_printhelp(char *);		/* Detailed help routine */
void Parser_exit(int, char **);		/* Shuts down command parser */
int Parser_execarg(int argc, char **argv, command_t cmds[]);
int execute_line(char * line);

/* Converts a string to an integer */
int Parser_int(char *, int *);

/* Prompts for a string, with default values and a maximum length */
char *Parser_getstr(const char *prompt, const char *deft, char *res, 
		    size_t len);

/* Prompts for an integer, with minimum, maximum and default values and base */
int Parser_getint(const char *prompt, long min, long max, long deft,
		  int base);

/* Prompts for a yes/no, with default */
int Parser_getbool(const char *prompt, int deft);

/* Extracts an integer from a string, or prompts if it cannot get one */
long Parser_intarg(const char *inp, const char *prompt, int deft,
		   int min, int max, int base);

/* Extracts a word from the input, or propmts if it cannot get one */
char *Parser_strarg(char *inp, const char *prompt, const char *deft,
		    char *answer, int len);

/* Extracts an integer from a string  with a base */
int Parser_arg2int(const char *inp, long *result, int base);

/* Convert human readable size string to and int; "1k" -> 1000 */
int Parser_size(int *sizep, char *str);

/* Convert a string boolean to an int; "enable" -> 1 */
int Parser_bool(int *b, char *str);

#endif
