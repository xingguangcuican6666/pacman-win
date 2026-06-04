/*
 *  util-common.c
 *
 *  Copyright (c) 2006-2025 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util-common.h"


/** Create a string representing bytes in hexadecimal.
 * @param bytes the bytes to represent in hexadecimal
 * @param size number of bytes to consider
 * @return a NULL terminated string with the hexadecimal representation of
 * bytes or NULL on error. This string must be freed.
 */
char *hex_representation(const unsigned char *bytes, size_t size)
{
	static const char *hex_digits = "0123456789abcdef";
	char *str = malloc(2 * size + 1);
	size_t i;

	if(!str) {
		return NULL;
	}

	for(i = 0; i < size; i++) {
		str[2 * i] = hex_digits[bytes[i] >> 4];
		str[2 * i + 1] = hex_digits[bytes[i] & 0x0f];
	}

	str[2 * size] = '\0';

	return str;
}

/** Parse the basename of a program from a path.
* @param path path to parse basename from
*
* @return everything following the final '/'
*/
const char *mbasename(const char *path)
{
	const char *last = strrchr(path, '/');
	if(last) {
		return last + 1;
	}
	return path;
}

/** Parse the dirname of a program from a path.
* The path returned should be freed.
* @param path path to parse dirname from
*
* @return everything preceding the final '/'
*/
char *mdirname(const char *path)
{
	char *ret, *last;

	/* null or empty path */
	if(path == NULL || *path == '\0') {
		return strdup(".");
	}

	if((ret = strdup(path)) == NULL) {
		return NULL;
	}

	last = strrchr(ret, '/');

	if(last != NULL) {
		/* we found a '/', so terminate our string */
		if(last == ret) {
			/* return "/" for root */
			last++;
		}
		*last = '\0';
		return ret;
	}

	/* no slash found */
	free(ret);
	return strdup(".");
}

static int pm_fnmatch_charclass(const char *name, unsigned char ch)
{
	if(strcmp(name, "alnum") == 0) {
		return isalnum(ch) != 0;
	} else if(strcmp(name, "alpha") == 0) {
		return isalpha(ch) != 0;
	} else if(strcmp(name, "digit") == 0) {
		return isdigit(ch) != 0;
	} else if(strcmp(name, "lower") == 0) {
		return islower(ch) != 0;
	} else if(strcmp(name, "upper") == 0) {
		return isupper(ch) != 0;
	} else if(strcmp(name, "space") == 0) {
		return isspace(ch) != 0;
	}
	return 0;
}

static int pm_fnmatch_bracket(const char **pattern, unsigned char ch)
{
	const char *p = *pattern;
	int negate = 0;
	int matched = 0;

	if(*p == '!' || *p == '^') {
		negate = 1;
		p++;
	}

	while(*p != '\0' && *p != ']') {
		if(p[0] == '[' && p[1] == ':') {
			const char *end = strstr(p + 2, ":]");
			if(end != NULL) {
				size_t len = (size_t)(end - (p + 2));
				char cls[16];
				if(len < sizeof(cls)) {
					memcpy(cls, p + 2, len);
					cls[len] = '\0';
					if(pm_fnmatch_charclass(cls, ch)) {
						matched = 1;
					}
					p = end + 2;
					continue;
				}
			}
		}

		if(p[1] == '-' && p[2] != '\0' && p[2] != ']') {
			unsigned char start = (unsigned char)p[0];
			unsigned char end = (unsigned char)p[2];
			if(start <= ch && ch <= end) {
				matched = 1;
			}
			p += 3;
			continue;
		}

		if((unsigned char)p[0] == ch) {
			matched = 1;
		}
		p++;
	}

	if(*p == ']') {
		*pattern = p + 1;
	} else {
		*pattern = p;
	}

	return negate ? !matched : matched;
}

static int pm_fnmatch_match(const char *pattern, const char *string)
{
	while(*pattern != '\0') {
		switch(*pattern) {
		case '*':
			while(*pattern == '*') {
				pattern++;
			}
			if(*pattern == '\0') {
				return 1;
			}
			while(*string != '\0') {
				if(pm_fnmatch_match(pattern, string)) {
					return 1;
				}
				string++;
			}
			return pm_fnmatch_match(pattern, string);
		case '?':
			if(*string == '\0') {
				return 0;
			}
			pattern++;
			string++;
			break;
		case '[':
			pattern++;
			if(*string == '\0' || !pm_fnmatch_bracket(&pattern, (unsigned char)*string)) {
				return 0;
			}
			string++;
			break;
		default:
			if(*pattern != *string) {
				return 0;
			}
			pattern++;
			string++;
			break;
		}
	}

	return *string == '\0';
}

int pm_fnmatch(const char *pattern, const char *string, int flags)
{
	(void)flags;
	return pm_fnmatch_match(pattern, string) ? 0 : 1;
}

char *pm_strndup(const char *s, size_t n)
{
	size_t len = 0;
	char *new;

	while(len < n && s[len] != '\0') {
		len++;
	}

	new = malloc(len + 1);

	if(new == NULL) {
		return NULL;
	}

	new[len] = '\0';
	return memcpy(new, s, len);
}

int pm_mkdir(const char *path, mode_t mode)
{
#ifdef _WIN32
	(void)mode;
	return mkdir(path);
#else
	return mkdir(path, mode);
#endif
}

char *pm_realpath(const char *path, char *resolved_path)
{
#ifdef _WIN32
	return _fullpath(resolved_path, path, resolved_path ? PATH_MAX : 0);
#else
	return realpath(path, resolved_path);
#endif
}

ssize_t pm_readlink(const char *path, char *buf, size_t bufsiz)
{
#ifdef _WIN32
	(void)path;
	(void)buf;
	(void)bufsiz;
	errno = ENOSYS;
	return -1;
#else
	return readlink(path, buf, bufsiz);
#endif
}

#ifdef _WIN32
uid_t pm_getuid(void)
{
	return 1;
}

int pm_lstat(const char *path, struct stat *buf)
{
	return stat(path, buf);
}
#endif

char *cwdsave(void)
{
	size_t size = 128;

	for(;;) {
		char *cwd = malloc(size);
		if(cwd == NULL) {
			return NULL;
		}
		if(getcwd(cwd, size) != NULL) {
			return cwd;
		}
		free(cwd);
		if(errno != ERANGE) {
			return NULL;
		}
		if(size > ((size_t)-1) / 2) {
			errno = ENOMEM;
			return NULL;
		}
		size *= 2;
	}
}

int cwdrestore(const char *path)
{
	if(path == NULL) {
		errno = EINVAL;
		return -1;
	}
	return chdir(path);
}

/** lstat wrapper that treats /path/dirsymlink/ the same as /path/dirsymlink.
 * Linux lstat follows POSIX semantics and still performs a dereference on
 * the first, and for uses of lstat in libalpm this is not what we want.
 * @param path path to file to lstat
 * @param buf structure to fill with stat information
 * @return the return code from lstat
 */
int llstat(char *path, struct stat *buf)
{
	int ret;
	char *c = NULL;
	size_t len = strlen(path);

	while(len > 1 && path[len - 1] == '/') {
		--len;
		c = path + len;
	}

	if(c) {
		*c = '\0';
		ret = lstat(path, buf);
		*c = '/';
	} else {
		ret = lstat(path, buf);
	}

	return ret;
}

#ifndef HAVE_STRSEP
/** Extracts tokens from a string.
 * Replaces strsep which is not portable.
 * Copyright (c) 2001 by Francois Gouget <fgouget_at_codeweavers.com>
 * @param str string containing delimited tokens to parse
 * @param delims delimiters that terminate the current token
 * @return pointer to the current token or NULL when no tokens remain
 */
char *strsep(char **str, const char *delims)
{
	char *token;

	if(*str == NULL) {
		return NULL;
	}

	token = *str;
	while(**str != '\0') {
		if(strchr(delims, **str) != NULL) {
			**str = '\0';
			(*str)++;
			return token;
		}
		(*str)++;
	}

	*str = NULL;
	return token;
}
#endif

/** Wrapper around fgets() which properly handles EINTR
 * @param s string to read into
 * @param size maximum length to read
 * @param stream stream to read from
 * @return value returned by fgets()
 */
char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;
	int errno_save = errno, ferror_save = ferror(stream);
	while((ret = fgets(s, size, stream)) == NULL && !feof(stream)) {
		if(errno == EINTR) {
			/* clear any errors we set and try again */
			errno = errno_save;
			if(!ferror_save) {
				clearerr(stream);
			}
		} else {
			break;
		}
	}
	return ret;
}

/* Trim whitespace and newlines from a string
 */
size_t strtrim(char *str)
{
	char *end, *pch = str;

	if(str == NULL || *str == '\0') {
		/* string is empty, so we're done. */
		return 0;
	}

	while(isspace((unsigned char)*pch)) {
		pch++;
	}
	if(pch != str) {
		size_t len = strlen(pch);
		/* check if there wasn't anything but whitespace in the string. */
		if(len == 0) {
			*str = '\0';
			return 0;
		}
		memmove(str, pch, len + 1);
		pch = str;
	}

	end = (str + strlen(str) - 1);
	while(isspace((unsigned char)*end)) {
		end--;
	}
	*++end = '\0';

	return end - pch;
}

#ifndef HAVE_STRNLEN
/* A quick and dirty implementation derived from glibc */
/** Determines the length of a fixed-size string.
 * @param s string to be measured
 * @param max maximum number of characters to search for the string end
 * @return length of s or max, whichever is smaller
 */
static size_t strnlen(const char *s, size_t max)
{
	register const char *p;
	for(p = s; *p && max--; ++p);
	return (p - s);
}
#endif

void wordsplit_free(char **ws)
{
	if(ws) {
		char **c;
		for(c = ws; *c; c++) {
			free(*c);
		}
		free(ws);
	}
}

char **wordsplit(const char *str)
{
	const char *c = str, *end;
	char **out = NULL, **outsave;
	size_t count = 0;

	if(str == NULL) {
		errno = EINVAL;
		return NULL;
	}

	for(c = str; isspace(*c); c++);
	while(*c) {
		size_t wordlen = 0;

		/* extend our array */
		outsave = out;
		if((out = realloc(out, (count + 1) * sizeof(char*))) == NULL) {
			out = outsave;
			goto error;
		}

		/* calculate word length and check for unbalanced quotes */
		for(end = c; *end && !isspace(*end); end++) {
			if(*end == '\'' || *end == '"') {
				char quote = *end;
				while(*(++end) && *end != quote) {
					if(*end == '\\' && *(end + 1) == quote) {
						end++;
					}
					wordlen++;
				}
				if(*end != quote) {
					errno = EINVAL;
					goto error;
				}
			} else {
				if(*end == '\\' && (end[1] == '\'' || end[1] == '"')) {
					end++; /* skip the '\\' */
				}
				wordlen++;
			}
		}

		if(wordlen == (size_t) (end - c)) {
			/* no internal quotes or escapes, copy it the easy way */
			if((out[count++] = pm_strndup(c, wordlen)) == NULL) {
				goto error;
			}
		} else {
			/* manually copy to remove quotes and escapes */
			char *dest = out[count++] = malloc(wordlen + 1);
			if(dest == NULL) { goto error; }
			while(c < end) {
				if(*c == '\'' || *c == '"') {
					char quote = *c;
					/* we know there must be a matching end quote,
					 * no need to check for '\0' */
					for(c++; *c != quote; c++) {
						if(*c == '\\' && *(c + 1) == quote) {
							c++;
						}
						*(dest++) = *c;
					}
					c++;
				} else {
					if(*c == '\\' && (c[1] == '\'' || c[1] == '"')) {
						c++; /* skip the '\\' */
					}
					*(dest++) = *(c++);
				}
			}
			*dest = '\0';
		}

		if(*end == '\0') {
			break;
		} else {
			for(c = end + 1; isspace(*c); c++);
		}
	}

	outsave = out;
	if((out = realloc(out, (count + 1) * sizeof(char*))) == NULL) {
		out = outsave;
		goto error;
	}

	out[count++] = NULL;

	return out;

error:
	/* can't use wordsplit_free here because NULL has not been appended */
	while(count) {
		free(out[--count]);
	}
	free(out);
	return NULL;
}
