#include "backend.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

#include "json.hpp"
#include "localdb.hpp"
#include "pwpkg.hpp"

using pacman::windows::LocalDatabase;
using pacman::windows::ExtractedPackage;
using pacman::windows::InstalledPackage;
using pacman::windows::InstallReason;
using pacman::windows::PackageSourceType;

static pm_winpkg_result_t *result_with_error(const char *message)
{
	pm_winpkg_result_t *result = (pm_winpkg_result_t *)calloc(1, sizeof(pm_winpkg_result_t));
	if(!result) {
		return NULL;
	}
	result->success = 0;
	result->errors = alpm_list_add(NULL, strdup(message));
	return result;
}

static pm_winpkg_result_t *result_ok(void)
{
	pm_winpkg_result_t *result = (pm_winpkg_result_t *)calloc(1, sizeof(pm_winpkg_result_t));
	if(!result) {
		return NULL;
	}
	result->success = 1;
	return result;
}

static pm_winpkg_result_t *append_message(pm_winpkg_result_t *result, const std::string& message)
{
	if(result) {
		result->messages = alpm_list_add(result->messages, strdup(message.c_str()));
	}
	return result;
}

static std::filesystem::path handle_root(alpm_handle_t *handle)
{
	const char *root = alpm_option_get_root(handle);
	if(root == nullptr || root[0] == '\0') {
		return std::filesystem::current_path();
	}
	std::filesystem::path path(root);
	if(path == std::filesystem::path("/")) {
		return std::filesystem::current_path();
	}
	return path;
}

static bool copy_payload_item(
	const std::filesystem::path& src,
	const std::filesystem::path& dst,
	bool directory,
	std::string& error)
{
	std::error_code ec;
	if(directory) {
		std::filesystem::create_directories(dst, ec);
		if(ec) {
			error = "failed to create directory: " + dst.string();
			return false;
		}
		return true;
	}
	std::filesystem::create_directories(dst.parent_path(), ec);
	if(ec) {
		error = "failed to create parent directory: " + dst.parent_path().string();
		return false;
	}
	std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
	if(ec) {
		error = "failed to copy file to " + dst.string();
		return false;
	}
	return true;
}

static std::filesystem::path rooted_destination(const std::filesystem::path& root, const std::string& destination)
{
	if(destination.empty()) {
		return root;
	}
	if(destination[0] == '/' || destination[0] == '\\') {
		return root / destination.substr(1);
	}
	return root / destination;
}

int pm_windows_backend_enabled(void)
{
	const char *forced = getenv("PACMAN_WIN_BACKEND");
#ifdef _WIN32
	return 1;
#else
	return forced && strcmp(forced, "1") == 0;
#endif
}

int pm_windows_is_pwpkg_target(const char *target)
{
	return pacman::windows::windows_pkg_is_pwpkg(target);
}

int pm_windows_sync_should_handle(alpm_list_t *targets)
{
	(void)targets;
	return pm_windows_backend_enabled();
}

int pm_windows_upgrade_should_handle(alpm_list_t *targets)
{
	alpm_list_t *i;
	if(!pm_windows_backend_enabled() || targets == NULL) {
		return 0;
	}
	for(i = targets; i; i = i->next) {
		if(!pm_windows_is_pwpkg_target((const char *)i->data)) {
			return 0;
		}
	}
	return 1;
}

int pm_windows_remove_should_handle(alpm_list_t *targets)
{
	(void)targets;
	return pm_windows_backend_enabled();
}

int pm_windows_query_should_handle(int info, int list_mode)
{
	(void)info;
	(void)list_mode;
	return pm_windows_backend_enabled();
}

static pm_winpkg_result_t *install_extracted(LocalDatabase& db, const ExtractedPackage& extracted, int flags)
{
	InstalledPackage installed;
	installed.manifest = extracted.manifest;
	installed.reason = (flags & ALPM_TRANS_FLAG_ALLDEPS) ? InstallReason::Dependency : InstallReason::Explicit;
	for(const auto& reg : extracted.manifest.registry) {
		installed.trace.registry_values.push_back({
			reg.hive, reg.key, reg.name, reg.type, reg.value
		});
	}
	for(const auto& shortcut : extracted.manifest.shortcuts) {
		installed.trace.shortcuts.push_back(shortcut.path);
	}

	if(extracted.manifest.source_type == PackageSourceType::Payload) {
		for(const auto& entry : extracted.manifest.files) {
			std::string error;
			auto src = extracted.payload_root / entry.source;
			auto dst = rooted_destination(db.root(), entry.destination);
			if(!copy_payload_item(src, dst, entry.directory, error)) {
				return result_with_error(error.c_str());
			}
			installed.trace.paths.push_back({entry.destination, entry.directory});
		}
	} else if(extracted.manifest.installer.has_value()) {
		const auto& installer = *extracted.manifest.installer;
		installed.has_explicit_uninstall = installer.uninstall.has_value();
		if(installer.uninstall.has_value()) {
			installed.uninstall = *installer.uninstall;
		}
		std::string command = "\"" + (extracted.extracted_root / installer.executable).string() + "\"";
		for(const auto& arg : installer.arguments) {
			command += " \"" + arg + "\"";
		}
		if(std::system(command.c_str()) != 0) {
			return result_with_error("wrapped installer returned non-zero status");
		}
		installed.trace.child_processes.push_back(command);
		installed.trace.paths.push_back({
			installer.working_directory.empty()
				? extracted.extracted_root.string()
				: installer.working_directory,
			true
		});
	}

	if(!installed.trace.registry_values.empty()) {
		nlohmann::json registry = nlohmann::json::array();
		for(const auto& value : installed.trace.registry_values) {
			registry.push_back({
				{"hive", value.hive},
				{"key", value.key},
				{"name", value.name},
				{"type", value.type},
				{"value", value.value}
			});
		}
		std::error_code ec;
		auto regdir = db.root() / "var" / "lib" / "pacman" / "windows-registry";
		std::filesystem::create_directories(regdir, ec);
		if(ec) {
			return result_with_error("failed to create windows registry state directory");
		}
		std::ofstream regfile(regdir / (installed.manifest.name + ".json"));
		if(!regfile) {
			return result_with_error("failed to write windows registry state file");
		}
		regfile << registry.dump(2);
	}

	std::vector<std::string> save_errors;
	if(!db.save_package(installed, save_errors)) {
		return result_with_error(save_errors.empty() ? "failed to save package state" : save_errors.front().c_str());
	}
	pm_winpkg_result_t *result = result_ok();
	return append_message(result, installed.manifest.name + " " + installed.manifest.version);
}

pm_winpkg_result_t *pm_windows_sync_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags)
{
	std::vector<std::string> errors;
	LocalDatabase db(handle_root(handle));
	db.load(errors);
	if(!errors.empty()) {
		return result_with_error(errors.front().c_str());
	}

	const alpm_list_t *cachedirs = alpm_option_get_cachedirs(handle);
	if(cachedirs == NULL) {
		return result_with_error("no cache directory configured");
	}
	pm_winpkg_result_t *result = result_ok();
	for(alpm_list_t *i = targets; i; i = i->next) {
		alpm_pkg_t *pkg = (alpm_pkg_t *)i->data;
		const char *filename = alpm_pkg_get_filename(pkg);
		if(filename == NULL) {
			pm_windows_result_free(result);
			return result_with_error("sync target has no filename");
		}
		auto pkg_path = std::filesystem::path((const char *)cachedirs->data) / filename;
		ExtractedPackage extracted;
		std::vector<std::string> extract_errors;
		auto workdir = db.root() / "var" / "tmp" / ("extract-" + std::string(alpm_pkg_get_name(pkg)));
		if(!pacman::windows::windows_pkg_extract(pkg_path, workdir, extracted, extract_errors)) {
			pm_windows_result_free(result);
			return result_with_error(extract_errors.empty() ? "failed to extract package" : extract_errors.front().c_str());
		}
		pm_winpkg_result_t *install_result = install_extracted(db, extracted, flags);
		if(!install_result || !install_result->success) {
			pm_windows_result_free(result);
			return install_result ? install_result : result_with_error("failed to install package");
		}
		for(alpm_list_t *m = install_result->messages; m; m = m->next) {
			result->messages = alpm_list_add(result->messages, strdup((const char *)m->data));
		}
		pm_windows_result_free(install_result);
	}
	return result;
}

pm_winpkg_result_t *pm_windows_upgrade_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags)
{
	std::vector<std::string> errors;
	LocalDatabase db(handle_root(handle));
	db.load(errors);
	if(!errors.empty()) {
		return result_with_error(errors.front().c_str());
	}

	for(alpm_list_t *i = targets; i; i = i->next) {
		const char *target = (const char *)i->data;
		ExtractedPackage extracted;
		std::vector<std::string> extract_errors;
		auto workdir = db.root() / "var" / "tmp" / ("extract-" + std::filesystem::path(target).stem().string());
		if(!pacman::windows::windows_pkg_extract(target, workdir, extracted, extract_errors)) {
			return result_with_error(extract_errors.empty() ? "failed to extract package" : extract_errors.front().c_str());
		}

		pm_winpkg_result_t *install_result = install_extracted(db, extracted, flags);
		if(!install_result || !install_result->success) {
			return install_result ? install_result : result_with_error("failed to install package");
		}
		pm_windows_result_free(install_result);
	}

	(void)flags;
	return result_ok();
}

pm_winpkg_result_t *pm_windows_remove_execute(alpm_handle_t *handle, alpm_list_t *targets, int flags)
{
	std::vector<std::string> errors;
	LocalDatabase db(handle_root(handle));
	db.load(errors);
	if(!errors.empty()) {
		return result_with_error(errors.front().c_str());
	}

	std::set<std::string> removing;
	for(alpm_list_t *i = targets; i; i = i->next) {
		removing.insert((const char *)i->data);
	}

	if(flags & ALPM_TRANS_FLAG_RECURSE) {
		bool changed = true;
		while(changed) {
			changed = false;
			for(const auto *orphan : db.orphans()) {
				if(!removing.count(orphan->manifest.name)) {
					removing.insert(orphan->manifest.name);
					changed = true;
				}
			}
		}
	}

	for(const auto& target_name : removing) {
		const InstalledPackage *installed = db.find_installed(target_name);
		if(!installed) {
			return result_with_error("target not installed");
		}
		if(db.is_required_by_other(target_name, removing) && !(flags & ALPM_TRANS_FLAG_RECURSE)) {
			return result_with_error("package is still required by another installed package");
		}
		if(installed->has_explicit_uninstall && !installed->uninstall.command.empty()) {
			std::string command = "\"" + installed->uninstall.command + "\"";
			for(const auto& arg : installed->uninstall.arguments) {
				command += " \"" + arg + "\"";
			}
			std::system(command.c_str());
		}
		for(const auto& path : installed->trace.paths) {
			std::error_code ec;
			std::filesystem::remove_all(rooted_destination(db.root(), path.path), ec);
		}
		for(const auto& shortcut : installed->trace.shortcuts) {
			std::error_code ec;
			std::filesystem::remove(rooted_destination(db.root(), shortcut), ec);
		}
		if(!installed->trace.registry_values.empty()) {
			std::error_code ec;
			std::filesystem::remove(db.root() / "var" / "lib" / "pacman" / "windows-registry" / (installed->manifest.name + ".json"), ec);
		}
		std::vector<std::string> remove_errors;
		if(!db.remove_package(target_name, remove_errors)) {
			return result_with_error(remove_errors.empty() ? "failed to remove package state" : remove_errors.front().c_str());
		}
	}

	(void)flags;
	return result_ok();
}

pm_winpkg_result_t *pm_windows_query_execute(alpm_handle_t *handle, alpm_list_t *targets, int info, int list_mode, int quiet, int explicit_only, int deps_only)
{
	std::vector<std::string> errors;
	LocalDatabase db(handle_root(handle));
	pm_winpkg_result_t *result = result_ok();
	if(!result) {
		return NULL;
	}
	db.load(errors);
	if(!errors.empty()) {
		pm_windows_result_free(result);
		return result_with_error(errors.front().c_str());
	}

	auto emit_package = [&](const InstalledPackage& pkg) {
		if(info) {
			result->messages = alpm_list_add(result->messages, strdup(("Name            : " + pkg.manifest.name).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Version         : " + pkg.manifest.version).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Description     : " + pkg.manifest.desc).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Architecture    : " + pkg.manifest.arch).c_str()));
		} else if(list_mode) {
			for(const auto& path : pkg.trace.paths) {
				result->messages = alpm_list_add(result->messages, strdup((pkg.manifest.name + " " + path.path).c_str()));
			}
		} else if(quiet) {
			result->messages = alpm_list_add(result->messages, strdup(pkg.manifest.name.c_str()));
		} else {
			result->messages = alpm_list_add(result->messages, strdup((pkg.manifest.name + " " + pkg.manifest.version).c_str()));
		}
	};

	if(targets == NULL) {
		for(const auto& [_, pkg] : db.packages()) {
			if(explicit_only && pkg.reason != InstallReason::Explicit) {
				continue;
			}
			if(deps_only && pkg.reason != InstallReason::Dependency) {
				continue;
			}
			emit_package(pkg);
		}
		return result;
	}

	for(alpm_list_t *i = targets; i; i = i->next) {
		const char *target = (const char *)i->data;
		const InstalledPackage *pkg = db.find_installed(target);
		if(!pkg) {
			pm_windows_result_free(result);
			return result_with_error("package not found in Windows local state");
		}
		if(explicit_only && pkg->reason != InstallReason::Explicit) {
			continue;
		}
		if(deps_only && pkg->reason != InstallReason::Dependency) {
			continue;
		}
		emit_package(*pkg);
	}
	return result;
}

void pm_windows_result_free(pm_winpkg_result_t *result)
{
	if(!result) {
		return;
	}
	FREELIST(result->messages);
	FREELIST(result->warnings);
	FREELIST(result->errors);
	free(result);
}
