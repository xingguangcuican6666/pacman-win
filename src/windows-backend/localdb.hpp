#ifndef PACMAN_WINDOWS_LOCALDB_HPP
#define PACMAN_WINDOWS_LOCALDB_HPP

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "pwpkg.hpp"

namespace pacman {
namespace windows {

enum class InstallReason {
	Explicit,
	Dependency
};

struct OwnedPath {
	std::string path;
	bool directory = false;
};

struct OwnedRegistryValue {
	std::string hive;
	std::string key;
	std::string name;
	std::string type;
	std::string value;
};

struct InstallerTrace {
	std::vector<OwnedPath> paths;
	std::vector<OwnedRegistryValue> registry_values;
	std::vector<std::string> shortcuts;
	std::vector<std::string> child_processes;
};

struct InstalledPackage {
	PackageManifest manifest;
	InstallReason reason = InstallReason::Explicit;
	bool has_explicit_uninstall = false;
	UninstallRecord uninstall;
	InstallerTrace trace;
};

class LocalDatabase {
public:
	explicit LocalDatabase(std::filesystem::path root);

	const std::filesystem::path& root() const { return root_; }
	std::filesystem::path db_root() const;
	std::filesystem::path state_root() const;

	bool load(std::vector<std::string>& errors);
	bool save_package(const InstalledPackage& package, std::vector<std::string>& errors) const;
	bool remove_package(const std::string& name, std::vector<std::string>& errors);
	const InstalledPackage* find_installed(const std::string& name) const;
	const std::map<std::string, InstalledPackage>& packages() const { return packages_; }
	std::vector<const InstalledPackage*> orphans() const;
	bool is_required_by_other(const std::string& name, const std::set<std::string>& removing) const;

private:
	std::filesystem::path root_;
	std::map<std::string, InstalledPackage> packages_;
};

} /* namespace windows */
} /* namespace pacman */

#endif
