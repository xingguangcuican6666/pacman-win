#ifndef PACMAN_WINDOWS_PWPKG_HPP
#define PACMAN_WINDOWS_PWPKG_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pacman {
namespace windows {

enum class PackageSourceType {
	Payload,
	WrappedInstaller
};

struct Dependency {
	std::string name;
	std::string original;
};

struct FileEntry {
	std::string source;
	std::string destination;
	bool directory = false;
};

struct RegistryEntry {
	std::string hive;
	std::string key;
	std::string name;
	std::string type;
	std::string value;
};

struct ShortcutSpec {
	std::string path;
	std::string target;
};

struct UninstallRecord {
	std::string command;
	std::string working_directory;
	std::vector<std::string> arguments;
};

struct InstallerSpec {
	std::string executable;
	std::vector<std::string> arguments;
	std::string working_directory;
	std::optional<UninstallRecord> uninstall;
};

struct PackageManifest {
	std::string name;
	std::string version;
	std::string release;
	std::string build;
	std::string installer_hash;
	std::string repo;
	std::string desc;
	std::string arch;
	std::string filename;
	PackageSourceType source_type = PackageSourceType::Payload;
	std::vector<Dependency> depends;
	std::vector<Dependency> provides;
	std::vector<FileEntry> files;
	std::vector<RegistryEntry> registry;
	std::vector<ShortcutSpec> shortcuts;
	std::optional<InstallerSpec> installer;
};

struct ExtractedPackage {
	PackageManifest manifest;
	std::filesystem::path extracted_root;
	std::filesystem::path payload_root;
};

bool windows_pkg_is_pwpkg(const char *target);
bool windows_pkg_read_manifest(const std::filesystem::path& path, PackageManifest& manifest, std::vector<std::string>& errors);
bool windows_pkg_extract(
	const std::filesystem::path& package_path,
	const std::filesystem::path& destination_root,
	ExtractedPackage& extracted,
	std::vector<std::string>& errors);

} /* namespace windows */
} /* namespace pacman */

#endif
