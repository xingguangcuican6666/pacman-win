#include "localdb.hpp"

#include <fstream>

#include "json.hpp"

namespace pacman {
namespace windows {
namespace {

std::string trace_change_type_to_string(TraceChangeType type)
{
	switch(type) {
		case TraceChangeType::Created:
			return "created";
		case TraceChangeType::Modified:
			return "modified";
		case TraceChangeType::Deleted:
			return "deleted";
	}
	return "created";
}

TraceChangeType trace_change_type_from_string(const std::string& type)
{
	if(type == "modified") {
		return TraceChangeType::Modified;
	}
	if(type == "deleted") {
		return TraceChangeType::Deleted;
	}
	return TraceChangeType::Created;
}

nlohmann::json deps_to_json(const std::vector<Dependency>& deps)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& dep : deps) {
		json.push_back({
			{"name", dep.name},
			{"original", dep.original}
		});
	}
	return json;
}

std::vector<Dependency> deps_from_json(const nlohmann::json& json)
{
	std::vector<Dependency> deps;
	for(const auto& item : json) {
		if(item.is_string()) {
			deps.push_back({item.get<std::string>(), item.get<std::string>()});
			continue;
		}
		deps.push_back({
			item.value("name", ""),
			item.value("original", item.value("name", ""))
		});
	}
	return deps;
}

nlohmann::json files_to_json(const std::vector<FileEntry>& files)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& file : files) {
		json.push_back({
			{"source", file.source},
			{"destination", file.destination},
			{"directory", file.directory}
		});
	}
	return json;
}

std::vector<FileEntry> files_from_json(const nlohmann::json& json)
{
	std::vector<FileEntry> files;
	for(const auto& item : json) {
		files.push_back({
			item.value("source", ""),
			item.value("destination", ""),
			item.value("directory", false)
		});
	}
	return files;
}

nlohmann::json registry_to_json(const std::vector<RegistryEntry>& values)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& value : values) {
		json.push_back({
			{"hive", value.hive},
			{"key", value.key},
			{"name", value.name},
			{"type", value.type},
			{"value", value.value}
		});
	}
	return json;
}

std::vector<RegistryEntry> registry_from_json(const nlohmann::json& json)
{
	std::vector<RegistryEntry> values;
	for(const auto& item : json) {
		values.push_back({
			item.value("hive", ""),
			item.value("key", ""),
			item.value("name", ""),
			item.value("type", ""),
			item.value("value", "")
		});
	}
	return values;
}

nlohmann::json shortcuts_to_json(const std::vector<ShortcutSpec>& shortcuts)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& shortcut : shortcuts) {
		json.push_back({
			{"path", shortcut.path},
			{"target", shortcut.target}
		});
	}
	return json;
}

std::vector<ShortcutSpec> shortcuts_from_json(const nlohmann::json& json)
{
	std::vector<ShortcutSpec> shortcuts;
	for(const auto& item : json) {
		shortcuts.push_back({
			item.value("path", ""),
			item.value("target", "")
		});
	}
	return shortcuts;
}

nlohmann::json file_snapshot_to_json(const FileSnapshot& snapshot)
{
	return {
		{"exists", snapshot.exists},
		{"directory", snapshot.directory},
		{"symlink", snapshot.symlink},
		{"mode", snapshot.mode},
		{"hash", snapshot.hash},
		{"content_hex", snapshot.content_hex},
		{"link_target", snapshot.link_target}
	};
}

FileSnapshot file_snapshot_from_json(const nlohmann::json& json)
{
	FileSnapshot snapshot;
	snapshot.exists = json.value("exists", false);
	snapshot.directory = json.value("directory", false);
	snapshot.symlink = json.value("symlink", false);
	snapshot.mode = json.value("mode", 0U);
	snapshot.hash = json.value("hash", "");
	snapshot.content_hex = json.value("content_hex", "");
	snapshot.link_target = json.value("link_target", "");
	return snapshot;
}

nlohmann::json path_changes_to_json(const std::vector<PathChange>& changes)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& change : changes) {
		json.push_back({
			{"path", change.path},
			{"change_type", trace_change_type_to_string(change.change_type)},
			{"before", file_snapshot_to_json(change.before)},
			{"after", file_snapshot_to_json(change.after)}
		});
	}
	return json;
}

std::vector<PathChange> path_changes_from_json(const nlohmann::json& json)
{
	std::vector<PathChange> changes;
	for(const auto& item : json) {
		PathChange change;
		change.path = item.value("path", "");
		change.change_type = trace_change_type_from_string(item.value("change_type", "created"));
		if(item.contains("before")) {
			change.before = file_snapshot_from_json(item["before"]);
		}
		if(item.contains("after")) {
			change.after = file_snapshot_from_json(item["after"]);
		}
		changes.push_back(std::move(change));
	}
	return changes;
}

nlohmann::json registry_change_to_json(const RegistryChange& change)
{
	return {
		{"change_type", trace_change_type_to_string(change.change_type)},
		{"existed_before", change.existed_before},
		{"exists_after", change.exists_after},
		{"before", {
			{"hive", change.before.hive},
			{"key", change.before.key},
			{"name", change.before.name},
			{"type", change.before.type},
			{"value", change.before.value}
		}},
		{"after", {
			{"hive", change.after.hive},
			{"key", change.after.key},
			{"name", change.after.name},
			{"type", change.after.type},
			{"value", change.after.value}
		}}
	};
}

std::vector<RegistryChange> registry_changes_from_json(const nlohmann::json& json)
{
	std::vector<RegistryChange> changes;
	for(const auto& item : json) {
		RegistryChange change;
		change.change_type = trace_change_type_from_string(item.value("change_type", "created"));
		change.existed_before = item.value("existed_before", false);
		change.exists_after = item.value("exists_after", false);
		if(item.contains("before")) {
			change.before = {
				item["before"].value("hive", ""),
				item["before"].value("key", ""),
				item["before"].value("name", ""),
				item["before"].value("type", ""),
				item["before"].value("value", "")
			};
		}
		if(item.contains("after")) {
			change.after = {
				item["after"].value("hive", ""),
				item["after"].value("key", ""),
				item["after"].value("name", ""),
				item["after"].value("type", ""),
				item["after"].value("value", "")
			};
		}
		changes.push_back(std::move(change));
	}
	return changes;
}

nlohmann::json command_results_to_json(const std::vector<CommandExecution>& results)
{
	nlohmann::json json = nlohmann::json::array();
	for(const auto& result : results) {
		json.push_back({
			{"phase", result.phase},
			{"executable", result.executable},
			{"arguments", result.arguments},
			{"working_directory", result.working_directory},
			{"exit_code", result.exit_code},
			{"status", result.status}
		});
	}
	return json;
}

std::vector<CommandExecution> command_results_from_json(const nlohmann::json& json)
{
	std::vector<CommandExecution> results;
	for(const auto& item : json) {
		CommandExecution result;
		result.phase = item.value("phase", "");
		result.executable = item.value("executable", "");
		result.arguments = item.value("arguments", std::vector<std::string>{});
		result.working_directory = item.value("working_directory", "");
		result.exit_code = item.value("exit_code", -1);
		result.status = item.value("status", "");
		results.push_back(std::move(result));
	}
	return results;
}

nlohmann::json package_to_json(const InstalledPackage& package)
{
	nlohmann::json json;
	json["name"] = package.manifest.name;
	json["version"] = package.manifest.version;
	json["release"] = package.manifest.release;
	json["build"] = package.manifest.build;
	json["installer_hash"] = package.manifest.installer_hash;
	json["repo"] = package.manifest.repo;
	json["desc"] = package.manifest.desc;
	json["arch"] = package.manifest.arch;
	json["filename"] = package.manifest.filename;
	json["source_type"] = package.manifest.source_type == PackageSourceType::WrappedInstaller
		? "wrapped-installer" : "payload";
	json["depends"] = deps_to_json(package.manifest.depends);
	json["provides"] = deps_to_json(package.manifest.provides);
	json["files"] = files_to_json(package.manifest.files);
	json["registry"] = registry_to_json(package.manifest.registry);
	json["shortcuts"] = shortcuts_to_json(package.manifest.shortcuts);
	if(package.manifest.trace_scope.has_value()) {
		json["trace_scope"] = {
			{"roots", package.manifest.trace_scope->roots},
			{"registry_keys", package.manifest.trace_scope->registry_keys}
		};
	}
	json["reason"] = package.reason == InstallReason::Dependency ? "dependency" : "explicit";
	json["trace"]["paths"] = nlohmann::json::array();
	for(const auto& path : package.trace.paths) {
		json["trace"]["paths"].push_back({
			{"path", path.path},
			{"directory", path.directory}
		});
	}
	json["trace"]["registry_values"] = nlohmann::json::array();
	for(const auto& reg : package.trace.registry_values) {
		json["trace"]["registry_values"].push_back({
			{"hive", reg.hive},
			{"key", reg.key},
			{"name", reg.name},
			{"type", reg.type},
			{"value", reg.value}
		});
	}
	json["trace"]["shortcuts"] = package.trace.shortcuts;
	json["trace"]["child_processes"] = package.trace.child_processes;
	json["trace"]["trace_roots"] = package.trace.trace_roots;
	json["trace"]["trace_registry_keys"] = package.trace.trace_registry_keys;
	json["trace"]["path_changes"] = path_changes_to_json(package.trace.path_changes);
	json["trace"]["registry_changes"] = nlohmann::json::array();
	for(const auto& change : package.trace.registry_changes) {
		json["trace"]["registry_changes"].push_back(registry_change_to_json(change));
	}
	json["trace"]["command_results"] = command_results_to_json(package.trace.command_results);
	json["uninstall"]["command"] = package.uninstall.command;
	json["uninstall"]["working_directory"] = package.uninstall.working_directory;
	json["uninstall"]["arguments"] = package.uninstall.arguments;
	json["has_explicit_uninstall"] = package.has_explicit_uninstall;
	json["install_status"] = package.install_status;
	json["uninstall_status"] = package.uninstall_status;
	json["last_error"] = package.last_error;
	return json;
}

InstalledPackage package_from_json(const nlohmann::json& json)
{
	InstalledPackage package;
	package.manifest.name = json.value("name", "");
	package.manifest.version = json.value("version", "");
	package.manifest.release = json.value("release", "1");
	package.manifest.build = json.value("build", "1");
	package.manifest.installer_hash = json.value("installer_hash", "");
	package.manifest.repo = json.value("repo", "local");
	package.manifest.desc = json.value("desc", "");
	package.manifest.arch = json.value("arch", "");
	package.manifest.filename = json.value("filename", "");
	package.manifest.source_type = json.value("source_type", "payload") == std::string("wrapped-installer")
		? PackageSourceType::WrappedInstaller : PackageSourceType::Payload;
	if(json.contains("depends")) {
		package.manifest.depends = deps_from_json(json["depends"]);
	}
	if(json.contains("provides")) {
		package.manifest.provides = deps_from_json(json["provides"]);
	}
	if(json.contains("files")) {
		package.manifest.files = files_from_json(json["files"]);
	}
	if(json.contains("registry")) {
		package.manifest.registry = registry_from_json(json["registry"]);
	}
	if(json.contains("shortcuts")) {
		package.manifest.shortcuts = shortcuts_from_json(json["shortcuts"]);
	}
	if(json.contains("trace_scope")) {
		TraceScope scope;
		scope.roots = json["trace_scope"].value("roots", std::vector<std::string>{});
		scope.registry_keys = json["trace_scope"].value("registry_keys", std::vector<std::string>{});
		package.manifest.trace_scope = scope;
	}
	package.reason = json.value("reason", "explicit") == std::string("dependency")
		? InstallReason::Dependency : InstallReason::Explicit;
	if(json.contains("trace")) {
		for(const auto& item : json["trace"].value("paths", nlohmann::json::array())) {
			package.trace.paths.push_back({
				item.value("path", ""),
				item.value("directory", false)
			});
		}
		for(const auto& item : json["trace"].value("registry_values", nlohmann::json::array())) {
			package.trace.registry_values.push_back({
				item.value("hive", ""),
				item.value("key", ""),
				item.value("name", ""),
				item.value("type", ""),
				item.value("value", "")
			});
		}
		package.trace.shortcuts = json["trace"].value("shortcuts", std::vector<std::string>{});
		package.trace.child_processes = json["trace"].value("child_processes", std::vector<std::string>{});
		package.trace.trace_roots = json["trace"].value("trace_roots", std::vector<std::string>{});
		package.trace.trace_registry_keys = json["trace"].value("trace_registry_keys", std::vector<std::string>{});
		if(json["trace"].contains("path_changes")) {
			package.trace.path_changes = path_changes_from_json(json["trace"]["path_changes"]);
		}
		if(json["trace"].contains("registry_changes")) {
			package.trace.registry_changes = registry_changes_from_json(json["trace"]["registry_changes"]);
		}
		if(json["trace"].contains("command_results")) {
			package.trace.command_results = command_results_from_json(json["trace"]["command_results"]);
		}
	}
	package.uninstall.command = json["uninstall"].value("command", "");
	package.uninstall.working_directory = json["uninstall"].value("working_directory", "");
	package.uninstall.arguments = json["uninstall"].value("arguments", std::vector<std::string>{});
	package.has_explicit_uninstall = json.value("has_explicit_uninstall", false);
	package.install_status = json.value("install_status", "installed");
	package.uninstall_status = json.value("uninstall_status", "not-run");
	package.last_error = json.value("last_error", "");
	return package;
}

} /* namespace */

LocalDatabase::LocalDatabase(std::filesystem::path root)
	: root_(std::move(root))
{
}

std::filesystem::path LocalDatabase::db_root() const
{
	return root_ / "var" / "lib" / "pacman";
}

std::filesystem::path LocalDatabase::state_root() const
{
	return db_root() / "windows-local";
}

bool LocalDatabase::load(std::vector<std::string>& errors)
{
	packages_.clear();
	auto root = state_root();
	if(!std::filesystem::exists(root)) {
		return true;
	}
	for(const auto& entry : std::filesystem::directory_iterator(root)) {
		if(!entry.is_regular_file() || entry.path().extension() != ".json") {
			continue;
		}
		std::ifstream file(entry.path());
		if(!file) {
			errors.push_back("failed to open local state file: " + entry.path().string());
			return false;
		}
		nlohmann::json json;
		file >> json;
		auto pkg = package_from_json(json);
		packages_[pkg.manifest.name] = std::move(pkg);
	}
	return true;
}

bool LocalDatabase::save_package(const InstalledPackage& package, std::vector<std::string>& errors) const
{
	std::error_code ec;
	std::filesystem::create_directories(state_root(), ec);
	if(ec) {
		errors.push_back("failed to create local state directory: " + state_root().string());
		return false;
	}
	auto path = state_root() / (package.manifest.name + ".json");
	std::ofstream file(path);
	if(!file) {
		errors.push_back("failed to write local state file: " + path.string());
		return false;
	}
	file << package_to_json(package).dump(2);
	return true;
}

bool LocalDatabase::remove_package(const std::string& name, std::vector<std::string>& errors)
{
	auto it = packages_.find(name);
	if(it == packages_.end()) {
		errors.push_back("package not found in local state: " + name);
		return false;
	}
	std::error_code ec;
	std::filesystem::remove(state_root() / (name + ".json"), ec);
	packages_.erase(it);
	return !ec;
}

const InstalledPackage* LocalDatabase::find_installed(const std::string& name) const
{
	auto it = packages_.find(name);
	return it == packages_.end() ? nullptr : &it->second;
}

std::vector<const InstalledPackage*> LocalDatabase::orphans() const
{
	std::vector<const InstalledPackage*> result;
	for(const auto& [name, pkg] : packages_) {
		if(pkg.reason != InstallReason::Dependency) {
			continue;
		}
		if(!is_required_by_other(name, {})) {
			result.push_back(&pkg);
		}
	}
	return result;
}

bool LocalDatabase::is_required_by_other(const std::string& name, const std::set<std::string>& removing) const
{
	for(const auto& [pkg_name, pkg] : packages_) {
		if(pkg_name == name || removing.count(pkg_name)) {
			continue;
		}
		for(const auto& dep : pkg.manifest.depends) {
			if(dep.name == name || dep.original == name) {
				return true;
			}
			for(const auto& provide : packages_.at(name).manifest.provides) {
				if(dep.name == provide.name || dep.original == provide.original
						|| dep.name == provide.original || dep.original == provide.name) {
					return true;
				}
			}
		}
	}
	return false;
}

} /* namespace windows */
} /* namespace pacman */
