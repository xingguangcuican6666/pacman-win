/*
 *  util-common.h
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

#ifndef PM_UTIL_COMMON_H
#define PM_UTIL_COMMON_H

#include <limits.h>
#include <stdio.h>
#include <sys/types.h> /* uid_t */
#include <sys/stat.h> /* struct stat */

char *hex_representation(const unsigned char *bytes, size_t size);
const char *mbasename(const char *path);
char *mdirname(const char *path);
char *pm_strndup(const char *s, size_t n);
int pm_mkdir(const char *path, mode_t mode);
char *pm_realpath(const char *path, char *resolved_path);
char *cwdsave(void);
int cwdrestore(const char *path);

int llstat(char *path, struct stat *buf);

char *safe_fgets(char *s, int size, FILE *stream);

void wordsplit_free(char **ws);
char **wordsplit(const char *str);

size_t strtrim(char *str);

#ifndef HAVE_STRSEP
char *strsep(char **str, const char *delims);
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof (a) / sizeof (a[0]))
#endif

#ifndef NAME_MAX
#define NAME_MAX PATH_MAX
#endif

#ifdef _WIN32
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

uid_t pm_getuid(void);
int pm_lstat(const char *path, struct stat *buf);

#define getuid pm_getuid
#define lstat pm_lstat
#define realpath pm_realpath
#endif

#endif /* PM_UTIL_COMMON_H */
