#ifndef PM_REGEX_COMPAT_H
#define PM_REGEX_COMPAT_H

#ifdef _WIN32

#include <stddef.h>

typedef struct {
	char *pattern;
	int cflags;
} pm_regex_t;

typedef struct {
	int rm_so;
	int rm_eo;
} regmatch_t;

#ifndef REG_EXTENDED
#define REG_EXTENDED 1
#endif
#ifndef REG_ICASE
#define REG_ICASE 2
#endif
#ifndef REG_NOSUB
#define REG_NOSUB 4
#endif
#ifndef REG_NEWLINE
#define REG_NEWLINE 8
#endif
#ifndef REG_NOMATCH
#define REG_NOMATCH 1
#endif

int pm_regcomp(pm_regex_t *preg, const char *regex, int cflags);
int pm_regexec(const pm_regex_t *preg, const char *string, size_t nmatch,
		regmatch_t pmatch[], int eflags);
void pm_regfree(pm_regex_t *preg);

#else

#include <regex.h>

typedef regex_t pm_regex_t;
#define pm_regcomp regcomp
#define pm_regexec regexec
#define pm_regfree regfree

#endif

#endif /* PM_REGEX_COMPAT_H */
