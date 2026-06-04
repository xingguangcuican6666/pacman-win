#include "backend.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>

#include "json.hpp"
#include "localdb.hpp"
#include "pwpkg.hpp"

using pacman::windows::LocalDatabase;
using pacman::windows::ExtractedPackage;
using pacman::windows::InstalledPackage;
using pacman::windows::InstallReason;
using pacman::windows::InstallerTrace;
using pacman::windows::OwnedPath;
using pacman::windows::OwnedRegistryValue;
using pacman::windows::PackageSourceType;
using pacman::windows::PathChange;
using pacman::windows::RegistryEntry;
using pacman::windows::RegistryChange;
using pacman::windows::TraceChangeType;
using pacman::windows::FileSnapshot;
using pacman::windows::CommandExecution;

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

static std::string relative_path_string(const std::filesystem::path& root, const std::filesystem::path& path)
{
	std::error_code ec;
	auto relative = std::filesystem::relative(path, root, ec);
	if(ec) {
		return path.lexically_normal().string();
	}
	return relative.lexically_normal().string();
}

static std::string bytes_to_hex(const std::string& data)
{
	static const char hex[] = "0123456789abcdef";
	std::string out;
	out.reserve(data.size() * 2);
	for(unsigned char c : data) {
		out.push_back(hex[(c >> 4) & 0x0F]);
		out.push_back(hex[c & 0x0F]);
	}
	return out;
}

static std::string read_small_file_hex(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if(!file) {
		return "";
	}
	std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return bytes_to_hex(data);
}

static FileSnapshot snapshot_path(const std::filesystem::path& path)
{
	FileSnapshot snapshot;
	std::error_code ec;
	auto status = std::filesystem::symlink_status(path, ec);
	if(ec || status.type() == std::filesystem::file_type::not_found) {
		return snapshot;
	}
	snapshot.exists = true;
	snapshot.directory = std::filesystem::is_directory(status);
	snapshot.symlink = std::filesystem::is_symlink(status);
	snapshot.mode = static_cast<unsigned int>(status.permissions());
	if(snapshot.symlink) {
		snapshot.link_target = std::filesystem::read_symlink(path, ec).string();
		return snapshot;
	}
	if(snapshot.directory) {
		return snapshot;
	}
	if(std::filesystem::is_regular_file(status)) {
		snapshot.hash = read_small_file_hex(path);
		snapshot.content_hex = snapshot.hash;
	}
	return snapshot;
}

static std::map<std::string, FileSnapshot> snapshot_tree(
	const std::filesystem::path& root,
	const std::vector<std::string>& relative_roots)
{
	std::map<std::string, FileSnapshot> snapshot;
	for(const auto& rel : relative_roots) {
		auto base = rooted_destination(root, rel);
		auto rel_name = relative_path_string(root, base);
		auto base_snapshot = snapshot_path(base);
		if(!base_snapshot.exists) {
			continue;
		}
		snapshot[rel_name] = base_snapshot;
		std::error_code ec;
		if(!std::filesystem::is_directory(base, ec)) {
			continue;
		}
		for(const auto& entry : std::filesystem::recursive_directory_iterator(base, ec)) {
			if(ec) {
				break;
			}
			snapshot[relative_path_string(root, entry.path())] = snapshot_path(entry.path());
		}
	}
	return snapshot;
}

static std::vector<PathChange> diff_snapshot(
	const std::map<std::string, FileSnapshot>& before,
	const std::map<std::string, FileSnapshot>& after)
{
	std::set<std::string> keys;
	for(const auto& [path, _] : before) {
		keys.insert(path);
	}
	for(const auto& [path, _] : after) {
		keys.insert(path);
	}

	std::vector<PathChange> changes;
	for(const auto& key : keys) {
		auto before_it = before.find(key);
		auto after_it = after.find(key);
		const bool existed_before = before_it != before.end() && before_it->second.exists;
		const bool exists_after = after_it != after.end() && after_it->second.exists;
		if(existed_before && exists_after) {
			if(before_it->second.hash == after_it->second.hash
					&& before_it->second.mode == after_it->second.mode
					&& before_it->second.directory == after_it->second.directory
					&& before_it->second.symlink == after_it->second.symlink
					&& before_it->second.link_target == after_it->second.link_target) {
				continue;
			}
			changes.push_back({key, TraceChangeType::Modified, before_it->second, after_it->second});
		} else if(!existed_before && exists_after) {
			changes.push_back({key, TraceChangeType::Created, {}, after_it->second});
		} else if(existed_before && !exists_after) {
			changes.push_back({key, TraceChangeType::Deleted, before_it->second, {}});
		}
	}
	return changes;
}

static std::map<std::string, FileSnapshot> snapshot_declared_paths(
	const std::filesystem::path& root,
	const std::vector<OwnedPath>& paths)
{
	std::map<std::string, FileSnapshot> snapshot;
	for(const auto& owned_path : paths) {
		auto abs_path = rooted_destination(root, owned_path.path);
		auto rel_path = relative_path_string(root, abs_path);
		snapshot[rel_path] = snapshot_path(abs_path);
	}
	return snapshot;
}

static std::vector<OwnedPath> manifest_owned_paths(const pacman::windows::PackageManifest& manifest)
{
	std::vector<OwnedPath> paths;
	for(const auto& entry : manifest.files) {
		paths.push_back({entry.destination, entry.directory});
	}
	return paths;
}

static std::vector<OwnedRegistryValue> snapshot_registry_values(const std::vector<RegistryEntry>& manifest_registry)
{
	std::vector<OwnedRegistryValue> values;
	for(const auto& reg : manifest_registry) {
		values.push_back({reg.hive, reg.key, reg.name, reg.type, reg.value});
	}
	return values;
}

static std::vector<RegistryChange> diff_registry_values(
	const std::vector<OwnedRegistryValue>& before,
	const std::vector<OwnedRegistryValue>& after)
{
	auto map_key = [](const OwnedRegistryValue& value) {
		return value.hive + "|" + value.key + "|" + value.name;
	};
	std::map<std::string, OwnedRegistryValue> before_map;
	std::map<std::string, OwnedRegistryValue> after_map;
	for(const auto& item : before) {
		before_map[map_key(item)] = item;
	}
	for(const auto& item : after) {
		after_map[map_key(item)] = item;
	}

	std::set<std::string> keys;
	for(const auto& [key, _] : before_map) {
		keys.insert(key);
	}
	for(const auto& [key, _] : after_map) {
		keys.insert(key);
	}

	std::vector<RegistryChange> changes;
	for(const auto& key : keys) {
		auto before_it = before_map.find(key);
		auto after_it = after_map.find(key);
		if(before_it != before_map.end() && after_it != after_map.end()) {
			if(before_it->second.type == after_it->second.type && before_it->second.value == after_it->second.value) {
				continue;
			}
			changes.push_back({
				TraceChangeType::Modified,
				true,
				true,
				before_it->second,
				after_it->second
			});
		} else if(before_it == before_map.end() && after_it != after_map.end()) {
			changes.push_back({
				TraceChangeType::Created,
				false,
				true,
				{},
				after_it->second
			});
		} else if(before_it != before_map.end() && after_it == after_map.end()) {
			changes.push_back({
				TraceChangeType::Deleted,
				true,
				false,
				before_it->second,
				{}
			});
		}
	}
	return changes;
}

static void add_trace_paths(InstallerTrace& trace, const std::vector<OwnedPath>& paths)
{
	std::set<std::string> seen;
	for(const auto& path : trace.paths) {
		seen.insert((path.directory ? "d:" : "f:") + path.path);
	}
	for(const auto& path : paths) {
		auto key = (path.directory ? "d:" : "f:") + path.path;
		if(seen.insert(key).second) {
			trace.paths.push_back(path);
		}
	}
}

static void add_trace_registry_values(InstallerTrace& trace, const std::vector<OwnedRegistryValue>& values)
{
	std::set<std::string> seen;
	for(const auto& value : trace.registry_values) {
		seen.insert(value.hive + "|" + value.key + "|" + value.name + "|" + value.type + "|" + value.value);
	}
	for(const auto& value : values) {
		auto key = value.hive + "|" + value.key + "|" + value.name + "|" + value.type + "|" + value.value;
		if(seen.insert(key).second) {
			trace.registry_values.push_back(value);
		}
	}
}

static std::string shell_quote(const std::string& value)
{
	std::string quoted = "'";
	for(char c : value) {
		if(c == '\'') {
			quoted += "'\\''";
		} else {
			quoted += c;
		}
	}
	quoted += "'";
	return quoted;
}

static void add_registry_changes(InstallerTrace& trace, const std::vector<RegistryChange>& changes)
{
	trace.registry_changes = changes;
}

static void set_path_changes(InstallerTrace& trace, const std::vector<PathChange>& changes)
{
	trace.path_changes = changes;
	for(const auto& change : changes) {
		if(change.change_type == TraceChangeType::Created || change.change_type == TraceChangeType::Modified) {
			add_trace_paths(trace, {{change.path, change.after.directory}});
		}
	}
}

static void ensure_trace_roots_from_paths(InstallerTrace& trace)
{
	if(!trace.trace_roots.empty()) {
		return;
	}

	std::set<std::string> seen;
	for(const auto& path : trace.paths) {
		if(seen.insert(path.path).second) {
			trace.trace_roots.push_back(path.path);
		}
	}
}

static std::vector<PathChange> tracked_path_changes_for_cleanup(const InstalledPackage& package)
{
	if(!package.trace.path_changes.empty()) {
		return package.trace.path_changes;
	}

	std::vector<PathChange> changes;
	std::vector<OwnedPath> paths = package.trace.paths;
	if(paths.empty()) {
		paths = manifest_owned_paths(package.manifest);
	}
	for(const auto& path : paths) {
		PathChange change;
		change.path = path.path;
		change.change_type = TraceChangeType::Created;
		change.after.exists = true;
		change.after.directory = path.directory;
		changes.push_back(std::move(change));
	}
	return changes;
}

static std::vector<PathChange> sorted_cleanup_changes(const InstalledPackage& package)
{
	auto changes = tracked_path_changes_for_cleanup(package);
	std::sort(changes.begin(), changes.end(), [](const PathChange& left, const PathChange& right) {
		if(left.after.directory != right.after.directory) {
			return !left.after.directory && right.after.directory;
		}
		return left.path.size() > right.path.size();
	});
	return changes;
}

static int execute_command(
	const std::string& phase,
	const std::string& executable,
	const std::vector<std::string>& arguments,
	const std::string& working_directory,
	CommandExecution& execution)
{
	execution.phase = phase;
	execution.executable = executable;
	execution.arguments = arguments;
	execution.working_directory = working_directory;
	std::string command;
	if(!working_directory.empty()) {
		command = "cd " + shell_quote(working_directory) + " && ";
	}
	command += shell_quote(executable);
	for(const auto& arg : arguments) {
		command += " " + shell_quote(arg);
	}
	int exit_code = std::system(command.c_str());
	execution.exit_code = exit_code;
	execution.status = exit_code == 0 ? "ok" : "failed";
	return exit_code;
}

static bool ensure_cached_sync_package(alpm_handle_t *handle, alpm_pkg_t *pkg, std::string& error)
{
	const char *filename = alpm_pkg_get_filename(pkg);
	if(filename == NULL) {
		error = "sync target has no filename";
		return false;
	}

	const alpm_list_t *cachedirs = alpm_option_get_cachedirs(handle);
	if(cachedirs == NULL) {
		error = "no cache directory configured";
		return false;
	}

	for(const alpm_list_t *i = cachedirs; i; i = i->next) {
		auto path = std::filesystem::path((const char *)i->data) / filename;
		std::error_code ec;
		if(std::filesystem::is_regular_file(path, ec)) {
			return true;
		}
	}

	alpm_list_t *urls = NULL;
	alpm_list_t *fetched = NULL;
	alpm_db_t *db = alpm_pkg_get_db(pkg);
	auto append_urls = [&](alpm_list_t *servers) {
		for(alpm_list_t *i = servers; i; i = i->next) {
			std::string url = (const char *)i->data;
			if(!url.empty() && url.back() != '/') {
				url += '/';
			}
			urls = alpm_list_add(urls, strdup((url + filename).c_str()));
		}
	};
	append_urls(alpm_db_get_cache_servers(db));
	append_urls(alpm_db_get_servers(db));
	if(urls == NULL) {
		error = "no server configured for sync package download";
		return false;
	}
	if(alpm_fetch_pkgurl(handle, urls, &fetched) != 0) {
		error = "failed to download sync package";
		FREELIST(urls);
		FREELIST(fetched);
		return false;
	}
	FREELIST(urls);
	FREELIST(fetched);
	return true;
}

static std::filesystem::path resolve_cached_sync_package_path(alpm_handle_t *handle, alpm_pkg_t *pkg)
{
	const char *filename = alpm_pkg_get_filename(pkg);
	if(filename == NULL) {
		return {};
	}

	auto root = handle_root(handle);
	for(const alpm_list_t *i = alpm_option_get_cachedirs(handle); i; i = i->next) {
		std::filesystem::path cachedir((const char *)i->data);
		std::filesystem::path candidate = cachedir / filename;
		std::error_code ec;
		if(std::filesystem::is_regular_file(candidate, ec)) {
			return candidate;
		}
		if(!cachedir.is_absolute()) {
			candidate = root / cachedir / filename;
			ec.clear();
			if(std::filesystem::is_regular_file(candidate, ec)) {
				return candidate;
			}
		}
	}
	return {};
}

static bool cleanup_tracked_path(const std::filesystem::path& root, const PathChange& change)
{
	std::error_code ec;
	auto path = rooted_destination(root, change.path);
	if(!std::filesystem::exists(path, ec) && !std::filesystem::is_symlink(path, ec)) {
		return true;
	}
	if(change.after.directory) {
		if(change.change_type != TraceChangeType::Created) {
			return true;
		}
		std::filesystem::remove(path, ec);
		return !ec;
	}
	std::filesystem::remove(path, ec);
	return !ec;
}

static bool registry_state_contains_tracked_values(
	const std::filesystem::path& root,
	const std::string& package_name,
	const std::vector<OwnedRegistryValue>& values)
{
	auto registry_path = root / "var" / "lib" / "pacman" / "windows-registry" / (package_name + ".json");
	std::ifstream file(registry_path);
	if(!file) {
		return false;
	}

	nlohmann::json json;
	try {
		file >> json;
	} catch(...) {
		return true;
	}

	std::set<std::string> current;
	for(const auto& item : json) {
		current.insert(
			item.value("hive", "") + "|" +
			item.value("key", "") + "|" +
			item.value("name", "") + "|" +
			item.value("type", "") + "|" +
			item.value("value", ""));
	}
	for(const auto& value : values) {
		auto key = value.hive + "|" + value.key + "|" + value.name + "|" + value.type + "|" + value.value;
		if(current.count(key)) {
			return true;
		}
	}
	return false;
}

static bool write_registry_state(LocalDatabase& db, const InstalledPackage& package, std::string& error)
{
	auto regdir = db.root() / "var" / "lib" / "pacman" / "windows-registry";
	auto regpath = regdir / (package.manifest.name + ".json");
	std::error_code ec;

	if(package.trace.registry_values.empty()) {
		std::filesystem::remove(regpath, ec);
		if(ec) {
			error = "failed to remove windows registry state file";
			return false;
		}
		return true;
	}

	nlohmann::json registry = nlohmann::json::array();
	for(const auto& value : package.trace.registry_values) {
		registry.push_back({
			{"hive", value.hive},
			{"key", value.key},
			{"name", value.name},
			{"type", value.type},
			{"value", value.value}
		});
	}

	std::filesystem::create_directories(regdir, ec);
	if(ec) {
		error = "failed to create windows registry state directory";
		return false;
	}

	std::ofstream regfile(regpath);
	if(!regfile) {
		error = "failed to write windows registry state file";
		return false;
	}
	regfile << registry.dump(2);
	return true;
}

static bool remove_installed_package(LocalDatabase& db, const InstalledPackage& installed_view, std::string& error)
{
	InstalledPackage installed = installed_view;
	auto persist_failure = [&](const std::string& status, const std::string& message) {
		installed.uninstall_status = status;
		installed.last_error = message;
		std::vector<std::string> save_errors;
		if(!db.save_package(installed, save_errors) && !save_errors.empty()) {
			error = save_errors.front();
		} else {
			error = message;
		}
		return false;
	};

	if(installed.has_explicit_uninstall && !installed.uninstall.command.empty()) {
		CommandExecution execution;
		std::string working_directory;
		if(!installed.uninstall.working_directory.empty()) {
			working_directory = rooted_destination(db.root(), installed.uninstall.working_directory).string();
		}
		int exit_code = execute_command(
			"uninstall",
			installed.uninstall.command,
			installed.uninstall.arguments,
			working_directory,
			execution);
		installed.trace.command_results.push_back(execution);
		if(exit_code != 0) {
			return persist_failure("command-failed", "explicit uninstall command failed");
		}
	}

	auto cleanup_changes = sorted_cleanup_changes(installed);
	if(installed.trace.paths.empty()) {
		add_trace_paths(installed.trace, manifest_owned_paths(installed.manifest));
	}
	if(installed.trace.trace_roots.empty()) {
		ensure_trace_roots_from_paths(installed.trace);
	}

	auto before_cleanup = snapshot_tree(db.root(), installed.trace.trace_roots);
	bool cleanup_failed = false;
	for(const auto& change : cleanup_changes) {
		if(!cleanup_tracked_path(db.root(), change)) {
			cleanup_failed = true;
		}
	}
	for(const auto& shortcut : installed.trace.shortcuts) {
		std::error_code ec;
		std::filesystem::remove(rooted_destination(db.root(), shortcut), ec);
		if(ec) {
			cleanup_failed = true;
		}
	}
	auto after_cleanup = snapshot_tree(db.root(), installed.trace.trace_roots);
	auto residual_changes = diff_snapshot(before_cleanup, after_cleanup);
	std::vector<PathChange> residual_present;
	for(const auto& change : residual_changes) {
		if(change.after.exists) {
			residual_present.push_back(change);
		}
	}
	if(cleanup_failed || !residual_present.empty()) {
		return persist_failure(
			cleanup_failed ? "cleanup-failed" : "residuals-remain",
			cleanup_failed
				? "failed to clean tracked filesystem paths"
				: "tracked filesystem residuals remain after uninstall");
	}

	auto registry_state_path = db.root() / "var" / "lib" / "pacman" / "windows-registry" / (installed.manifest.name + ".json");
	if(!installed.trace.registry_values.empty()
			&& registry_state_contains_tracked_values(db.root(), installed.manifest.name, installed.trace.registry_values)) {
		return persist_failure("registry-residuals-remain", "tracked registry residuals remain after uninstall");
	}

	std::error_code ec;
	std::filesystem::remove(registry_state_path, ec);
	if(ec) {
		return persist_failure("registry-cleanup-failed", "failed to remove registry state file");
	}

	std::vector<std::string> remove_errors;
	if(!db.remove_package(installed.manifest.name, remove_errors)) {
		error = remove_errors.empty() ? "failed to remove package state" : remove_errors.front();
		return false;
	}

	return true;
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
	if(const InstalledPackage *existing = db.find_installed(extracted.manifest.name)) {
		std::string remove_error;
		if(!remove_installed_package(db, *existing, remove_error)) {
			return result_with_error(remove_error.c_str());
		}
	}

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
	if(extracted.manifest.trace_scope.has_value()) {
		installed.trace.trace_roots = extracted.manifest.trace_scope->roots;
		installed.trace.trace_registry_keys = extracted.manifest.trace_scope->registry_keys;
	}

	if(extracted.manifest.source_type == PackageSourceType::Payload) {
		installed.trace.paths = manifest_owned_paths(extracted.manifest);
		auto before_paths = snapshot_declared_paths(db.root(), installed.trace.paths);
		for(const auto& entry : extracted.manifest.files) {
			std::string error;
			auto src = extracted.payload_root / entry.source;
			auto dst = rooted_destination(db.root(), entry.destination);
			if(!copy_payload_item(src, dst, entry.directory, error)) {
				return result_with_error(error.c_str());
			}
		}
		auto after_paths = snapshot_declared_paths(db.root(), installed.trace.paths);
		set_path_changes(installed.trace, diff_snapshot(before_paths, after_paths));
		add_registry_changes(installed.trace, diff_registry_values({}, installed.trace.registry_values));
		ensure_trace_roots_from_paths(installed.trace);
	} else if(extracted.manifest.installer.has_value()) {
		const auto& installer = *extracted.manifest.installer;
		installed.has_explicit_uninstall = installer.uninstall.has_value();
		if(installer.uninstall.has_value()) {
			installed.uninstall = *installer.uninstall;
		}
		auto before_paths = snapshot_tree(db.root(), installed.trace.trace_roots);
		auto before_registry = std::vector<OwnedRegistryValue>{};
		CommandExecution execution;
		auto executable = (extracted.extracted_root / installer.executable).string();
		auto working_directory = installer.working_directory.empty()
			? extracted.extracted_root.string()
			: (extracted.extracted_root / installer.working_directory).string();
		int exit_code = execute_command("install", executable, installer.arguments, working_directory, execution);
		installed.trace.command_results.push_back(execution);
		installed.trace.child_processes.push_back(executable);
		auto after_paths = snapshot_tree(db.root(), installed.trace.trace_roots);
		auto after_registry = snapshot_registry_values(extracted.manifest.registry);
		set_path_changes(installed.trace, diff_snapshot(before_paths, after_paths));
		auto registry_changes = diff_registry_values(before_registry, after_registry);
		add_registry_changes(installed.trace, registry_changes);
		std::vector<OwnedRegistryValue> active_registry;
		for(const auto& change : registry_changes) {
			if(change.exists_after) {
				active_registry.push_back(change.after);
			}
		}
		add_trace_registry_values(installed.trace, active_registry);
		ensure_trace_roots_from_paths(installed.trace);
		if(exit_code != 0) {
			installed.install_status = "install-command-failed";
			installed.last_error = "wrapped installer returned non-zero status";
			std::string registry_error;
			write_registry_state(db, installed, registry_error);
			std::vector<std::string> save_errors;
			db.save_package(installed, save_errors);
			return result_with_error("wrapped installer returned non-zero status");
		}
	}

	installed.install_status = "installed";
	installed.uninstall_status = "not-run";
	installed.last_error.clear();

	std::string registry_error;
	if(!write_registry_state(db, installed, registry_error)) {
		return result_with_error(registry_error.c_str());
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
		std::string ensure_error;
		if(!ensure_cached_sync_package(handle, pkg, ensure_error)) {
			pm_windows_result_free(result);
			return result_with_error(ensure_error.c_str());
		}
		const char *filename = alpm_pkg_get_filename(pkg);
		if(filename == NULL) {
			pm_windows_result_free(result);
			return result_with_error("sync target has no filename");
		}
		auto pkg_path = resolve_cached_sync_package_path(handle, pkg);
		if(pkg_path.empty()) {
			pm_windows_result_free(result);
			return result_with_error("sync package not found in cache after download");
		}
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
			for(const auto& [name, pkg] : db.packages()) {
				if(pkg.reason != InstallReason::Dependency || removing.count(name)) {
					continue;
				}
				if(!db.is_required_by_other(name, removing)) {
					removing.insert(name);
					changed = true;
				}
			}
		}
	}

	for(const auto& target_name : removing) {
		const InstalledPackage *installed_view = db.find_installed(target_name);
		if(!installed_view) {
			return result_with_error("target not installed");
		}
		if(db.is_required_by_other(target_name, removing) && !(flags & ALPM_TRANS_FLAG_RECURSE)) {
			return result_with_error("package is still required by another installed package");
		}
		std::string remove_error;
		if(!remove_installed_package(db, *installed_view, remove_error)) {
			return result_with_error(remove_error.c_str());
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
			result->messages = alpm_list_add(result->messages, strdup(("Release         : " + pkg.manifest.release).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Build           : " + pkg.manifest.build).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Installer Hash  : " + pkg.manifest.installer_hash).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Install Reason  : " + std::string(pkg.reason == InstallReason::Dependency ? "dependency" : "explicit")).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Install Status  : " + pkg.install_status).c_str()));
			result->messages = alpm_list_add(result->messages, strdup(("Uninstall State : " + pkg.uninstall_status).c_str()));
			if(pkg.has_explicit_uninstall) {
				result->messages = alpm_list_add(result->messages, strdup(("Uninstall Cmd   : " + pkg.uninstall.command).c_str()));
			}
			if(!pkg.trace.registry_values.empty()) {
				result->messages = alpm_list_add(result->messages, strdup("Registry Values : yes"));
			}
			if(!pkg.trace.trace_roots.empty()) {
				result->messages = alpm_list_add(result->messages, strdup(("Trace Roots     : " + std::to_string(pkg.trace.trace_roots.size())).c_str()));
			}
			if(!pkg.trace.command_results.empty()) {
				result->messages = alpm_list_add(result->messages, strdup(("Tracked Procs   : " + std::to_string(pkg.trace.command_results.size())).c_str()));
			}
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
