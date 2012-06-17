/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <compat.h>
#ifndef HAVE_STRCASECMP
static int strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == NULL || s2 == NULL) {
		return 1;
	}

        if (n == 0) {
		return 0;
	}

	while (n-- != 0 && tolower(*s1) == tolower(*s2)) {
                if (n == 0 || *s1 == '\0' || *s2 == '\0') {
                        break;
		}
		s1++;
		s2++;
	}

	return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}
EXPORT_SYMBOL(strncasecmp);
#endif
