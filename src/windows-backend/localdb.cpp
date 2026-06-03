#include "localdb.hpp"

#include <fstream>

#include "json.hpp"

namespace pacman {
namespace windows {
namespace {

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
	json["uninstall"]["command"] = package.uninstall.command;
	json["uninstall"]["working_directory"] = package.uninstall.working_directory;
	json["uninstall"]["arguments"] = package.uninstall.arguments;
	json["has_explicit_uninstall"] = package.has_explicit_uninstall;
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
	package.reason = json.value("reason", "explicit") == std::string("dependency")
		? InstallReason::Dependency : InstallReason::Explicit;
	for(const auto& item : json["trace"]["paths"]) {
		package.trace.paths.push_back({
			item.value("path", ""),
			item.value("directory", false)
		});
	}
	for(const auto& item : json["trace"]["registry_values"]) {
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
	package.uninstall.command = json["uninstall"].value("command", "");
	package.uninstall.working_directory = json["uninstall"].value("working_directory", "");
	package.uninstall.arguments = json["uninstall"].value("arguments", std::vector<std::string>{});
	package.has_explicit_uninstall = json.value("has_explicit_uninstall", false);
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
		}
	}
	return false;
}

} /* namespace windows */
} /* namespace pacman */
