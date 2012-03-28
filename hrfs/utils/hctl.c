/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <cmd_parser.h>
#include <stdio.h>

#define TEST_VERSION "0.0.0"
int test_get_version(int argc, char **argv)
{
	printf("version: %s\n", TEST_VERSION);
	return 0;
}

static int test_quit(int argc, char **argv)
{
	Parser_quit(argc, argv);
	return 0;
}

static int test_noop(int argc, char **argv)
{
	return 0;
}

static int test_ignore_errors(int argc, char **argv)
{
	Parser_ignore_errors(1);
	return 0;
}

command_t cmdlist[] = {
	/* Metacommands */
	{"===== metacommands =======", test_noop, 0, "metacommands"},
	{"--ignore_errors", test_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "--ignore_errors"},

	/* User interface commands */
	{"======== control =========", test_noop, 0, "control commands"},
	{"help", Parser_help, 0, "help"},
	{"version", test_get_version, 0,
         "print the version of this test\n"
         "usage: version"},
	{"exit", test_quit, 0, "quit"},
	{"quit", test_quit, 0, "quit"},

	/* other commands */
	{ 0, 0, 0, NULL }
};

int main(int argc, char **argv)
{
	int rc;
	setlinebuf(stdout); //?
	Parser_init("cmd > ", cmdlist);
	if (argc > 1) {
		rc = Parser_execarg(argc - 1, argv + 1, cmdlist);
	} else {
		rc = Parser_commands();
	}

	Parser_exit(argc, argv);
	return rc;
}
