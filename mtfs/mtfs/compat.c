/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <compat.h>
#include <debug.h>
#include <memory.h>
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/fs.h>

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
#endif /* !HAVE_STRCASECMP */
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#ifndef HAVE_KALLSYMS_LOOKUP_NAME
#if defined (__linux__) && defined(__KERNEL__)
#define mtfs_strtoul simple_strtoul
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#define mtfs_strtoul strtoul
#endif
/*
 * Return 1 if matched, return 0 if not.
 * Reutrn -errno if error.
 */
static int mtfs_parse_kallsyms(char *line, const char *module_name,
                               const char *symbol_name, unsigned long *address)
{
	int ret = 0;
	int word_num = 0;
	unsigned long tmp_address = 0;
	char *word = NULL;
	char *endp = NULL;
	char *next_word = line;
	char *symbol = NULL;
	char *tmp_module_name = NULL;

	while ((word = strsep(&next_word, " "))) {
		if (!*word) {
			MERROR("empty word: %s\n", line);
			ret = -EINVAL;
			continue;
		}

		word_num++;
		if (word_num > 3) {
			MERROR("more words than expected: %s\n", line);
			ret = -EINVAL;
			break;
		}

		if (word_num == 1) {
			tmp_address = mtfs_strtoul(word, &endp, 16);
			if (*endp != '\0' && !isspace((unsigned char)*endp)) {
				MERROR("invalid address, number expected: %s\n",
				       line);
				ret = -EINVAL;
				break;
			}
		} else if (word_num == 3) {
			symbol = word;
			for (endp = symbol; *endp != '\0'; endp++) {
				/* There might be */
				if (*endp == '\t') {
					*endp = '\0';
					tmp_module_name = endp + 1;
					break;
				}
			}
			
			if (tmp_module_name) {
				if (*tmp_module_name != '[') {
					MERROR("invalid module name, should start with [: %s\n",
					       line);
					ret = -EINVAL;
					break;
				}
				tmp_module_name++;
				for (endp = tmp_module_name; *endp != '\0'; endp++);
				endp--;
				if (endp <= tmp_module_name || *endp != ']') {
					MERROR("invalid module name, should end with ]: %s\n",
					       line);
					ret = -EINVAL;
					break;
				}
				*endp = '\0';
			}

			if (module_name) {
				if (!tmp_module_name || strcmp(module_name, tmp_module_name)) {
					continue;
				}
			}

			if (strcmp(symbol_name, symbol) == 0) {
				ret = 1;
				*address = tmp_address;
				break;
			}
		}
	}
	MPRINT("%016lx%s\n", tmp_address, symbol);

	return ret;
}

#define PROC_KALLSYMS_PTAH "/proc/kallsyms"
#define MTFS_KALLSYMS_BUFF_SIZE 4096
/* Reutrn 0 when fail, return address when succeed */
unsigned long mtfs_kallsyms_lookup_name(const char *name)
{
#if defined (__linux__) && defined(__KERNEL__)
	mm_segment_t old_fs;
	struct file *file = NULL;
	loff_t pos = 0;
#else
	int fd = 0;
#endif
	int readlen = 0;
	char *buff = NULL;
	int offset = 0;
	int i = 0;
	unsigned long address = 0;
	int ret = 0;
	char *line = NULL;
	int keepon = 1;
	int line_found = 0;
	char *tmp_name = NULL;
	char *module_name = NULL;
	char *symbol_name = NULL;
	char *colon = NULL;

	MTFS_STRDUP(tmp_name, name);
	if (tmp_name == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	MTFS_ALLOC(buff, MTFS_KALLSYMS_BUFF_SIZE);
	if (buff == NULL) {
		MERROR("not enough memory\n");
		goto out_free_name;
	}

#if defined (__linux__) && defined(__KERNEL__)
	file = filp_open(PROC_KALLSYMS_PTAH, O_RDONLY, 0);
	if (IS_ERR(file)) {
		MERROR("failed to open %s, ret = %d\n",
		       PROC_KALLSYMS_PTAH, PTR_ERR(file));
		goto out_free_buff;
	}
	MASSERT(file->f_op->read);

	old_fs = get_fs();
	set_fs(get_ds());
#else
	fd = open(PROC_KALLSYMS_PTAH, O_RDONLY);
	if (fd < 0) {
		MERROR("failed to open %s\n", PROC_KALLSYMS_PTAH);
		goto out_free_buff;
	}
#endif

	if ((colon = strchr(tmp_name, ':')) != NULL) {
		*colon = '\0';
		module_name = tmp_name;
		symbol_name = colon + 1;
	} else {
		module_name = NULL;
		symbol_name = tmp_name;
	}

	while (keepon) {
#if defined (__linux__) && defined(__KERNEL__)
		readlen = file->f_op->read(file, buff + offset,
		                           MTFS_KALLSYMS_BUFF_SIZE - 1 - offset, &pos);
#else
		readlen = read(fd, buff + offset, MTFS_KALLSYMS_BUFF_SIZE - 1 - offset);
#endif
		if (readlen < 0) {
			MERROR("failed to read %s, ret = %d\n",
			       PROC_KALLSYMS_PTAH, readlen);
			break;
		}

		offset += readlen;
		if (offset == 0) {
			break;
		}

		if (offset >= MTFS_KALLSYMS_BUFF_SIZE) {
			MERROR("offset: %d, readlen: %d\n", offset, readlen);
		}
		MASSERT(offset < MTFS_KALLSYMS_BUFF_SIZE);
		buff[offset] = '\0';

		line = buff;
		for(i = 0; i < offset; i++) {
			if (buff[i] == '\n') {
				line_found = 1;
				buff[i] = '\0';
				ret = mtfs_parse_kallsyms(line, module_name, symbol_name, &address);
				if (ret == 1) {
					keepon = 0;
					break;
				} else if (ret < 0) {
					keepon = 0;
					break;
				} else {
					line = buff + i + 1; /* Point to next line */
				}
			}
		}

		if (keepon) {
			if (line != buff + offset) {
				MASSERT(line < buff + offset);
				if (line_found) {
					/* Try read again to complete */
					memmove(buff, line, buff + offset - line);
					line_found = 0;
					offset = buff + offset - line;
				} else {
					/* The last line */
					MASSERT(readlen < MTFS_KALLSYMS_BUFF_SIZE - 1);

					mtfs_parse_kallsyms(line, module_name, symbol_name, &address);
					/* Read again to make sure this is the EOF */
#if defined (__linux__) && defined(__KERNEL__)
					readlen = file->f_op->read(file, buff,
					                           MTFS_KALLSYMS_BUFF_SIZE - 1, &pos);
#else
					readlen = read(fd, buff + offset, MTFS_KALLSYMS_BUFF_SIZE - 1);
#endif
					if (readlen < 0) {
						MERROR("failed to read %s, ret = %d\n",
						       PROC_KALLSYMS_PTAH, readlen);
					} else if (readlen > 0) {
						MERROR("buffer size(%d) is not long engough\n",
						       MTFS_KALLSYMS_BUFF_SIZE);
					}
					break;
				}
			} else {
				offset = 0;
			}
		}
	}

#if defined (__linux__) && defined(__KERNEL__)
	set_fs(old_fs);
	filp_close(file, NULL);
#else
	close(fd);
#endif
out_free_buff:
	MTFS_FREE(buff, MTFS_KALLSYMS_BUFF_SIZE);
out_free_name:
	if (colon) {
		*colon = ':';
	}
	MTFS_FREE_STR(tmp_name);
out:
	return address;
}
EXPORT_SYMBOL(mtfs_kallsyms_lookup_name);

char *combine_str(const char *str1, const char *str2)
{
	size_t len = 0;
	char *new_str = NULL;

	len = strlen(str1);
	MTFS_ALLOC(new_str, len + strlen(str2) + 1);
	if (new_str == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	strcpy(new_str, str1);
	strcpy(new_str + len, str2);
out:
	return new_str;
}

char *combine_triple_str(const char *str1, const char *str2, const char *str3)
{
	size_t len1 = 0;
	size_t len2 = 0;
	char *new_str = NULL;
	
	len1 = strlen(str1);
	len2 = strlen(str2);
	MTFS_ALLOC(new_str, len1 + len2 + strlen(str3) + 1);
	if (new_str == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	strcpy(new_str, str1);
	strcpy(new_str + len1, str2);
	strcpy(new_str + len1 + len2, str3);
out:
	return new_str;
}

#if defined (__linux__) && defined(__KERNEL__)
int mtfs_symbol_get(const char *module_name,
                    const char *symbol_name,
                    unsigned long *address,
                    struct module **owner)
#else /* !defined (__linux__) && defined(__KERNEL__) */
int mtfs_symbol_get(const char *module_name,
                    const char *symbol_name,
                    unsigned long *address)
#endif /* !defined (__linux__) && defined(__KERNEL__) */
{
	int ret = 0;
	char *name = NULL;
	unsigned long tmp_address = 0;

	MASSERT(module_name);
	MASSERT(symbol_name);

	if (module_name) {
#if defined (__linux__) && defined(__KERNEL__)
		mutex_lock(&module_mutex);
		*owner = find_module(module_name);
		if (*owner == NULL) {
			mutex_unlock(&module_mutex);
			if (request_module(module_name) != 0) {
				MERROR("failed to load module %s\n", module_name);
				goto out;
			}
			mutex_lock(&module_mutex);
		}

		if (try_module_get(*owner) == 0) {
			MERROR("failed to get module %s\n",
			       module_name);
			ret = -ENOENT;
			goto out;
		}
		mutex_unlock(&module_mutex);
#endif /* defined (__linux__) && defined(__KERNEL__) */

		name = combine_triple_str(module_name, ":", symbol_name);
		if (name == NULL) {
			MERROR("failed to combine string\n");
			ret = -ENOMEM;
			goto out;
		}
		tmp_address = mtfs_kallsyms_lookup_name(name);
		MTFS_FREE_STR(name);
	} else {
		tmp_address = mtfs_kallsyms_lookup_name(symbol_name);
	}

	if (tmp_address == 0) {
		MERROR("failed to find symbol %s\n", symbol_name);
		ret = -ENOENT;
		goto out;
	}

	*address = tmp_address;
	/* Todo: Check address in the module: sprint_symbol() */
out:
	return ret;
}
EXPORT_SYMBOL(mtfs_symbol_get);

#if defined (__linux__) && defined(__KERNEL__)
void mtfs_symbol_put(struct module *owner)
{
	if (owner) {
		module_put(owner);
	}
}
EXPORT_SYMBOL(mtfs_symbol_put);
#endif /* !defined (__linux__) && defined(__KERNEL__) */


#endif /* !define HAVE_KALLSYMS_LOOKUP_NAME */
