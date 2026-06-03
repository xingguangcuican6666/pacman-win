#ifndef PACMAN_WINDOWS_BACKEND_H
#define PACMAN_WINDOWS_BACKEND_H

#include <alpm.h>
#include <alpm_list.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pm_winpkg_result_t {
	int success;
	alpm_list_t *messages;
	alpm_list_t *warnings;
	alpm_list_t *errors;
} pm_winpkg_result_t;

int pm_windows_backend_enabled(void);
int pm_windows_is_pwpkg_target(const char *target);
int pm_windows_sync_should_handle(alpm_list_t *targets);
int pm_windows_upgrade_should_handle(alpm_list_t *targets);
int pm_windows_remove_should_handle(alpm_list_t *targets);
int pm_windows_query_should_handle(int info, int list_mode);

pm_winpkg_result_t *pm_windows_sync_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags);
pm_winpkg_result_t *pm_windows_upgrade_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags);
pm_winpkg_result_t *pm_windows_remove_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags);
pm_winpkg_result_t *pm_windows_query_execute(alpm_handle_t *handle, alpm_list_t *targets, int info, int list_mode, int quiet, int explicit_only, int deps_only);

void pm_windows_result_free(pm_winpkg_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
