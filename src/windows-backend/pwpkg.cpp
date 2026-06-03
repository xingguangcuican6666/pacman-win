#include "pwpkg.hpp"

#include <fstream>

#include <archive.h>
#include <archive_entry.h>
#include "json.hpp"

namespace pacman {
namespace windows {

static Dependency parse_dep(const std::string& value)
{
	std::string name = value;
	const std::string operators[] = {">=", "<=", "=", ">", "<"};
	for(const auto& op : operators) {
		std::size_t pos = value.find(op);
		if(pos != std::string::npos) {
			name = value.substr(0, pos);
			break;
		}
	}
	return Dependency{name, value};
}

bool windows_pkg_is_pwpkg(const char *target)
{
	if(target == nullptr) {
		return false;
	}
	std::string name(target);
	return name.size() > 6 && name.substr(name.size() - 6) == ".pwpkg";
}

bool windows_pkg_read_manifest(const std::filesystem::path& path, PackageManifest& manifest, std::vector<std::string>& errors)
{
	std::ifstream file(path);
	if(!file) {
		errors.push_back("failed to open manifest: " + path.string());
		return false;
	}
	nlohmann::json json;
	try {
		file >> json;
	} catch(const std::exception& ex) {
		errors.push_back("failed to parse manifest: " + std::string(ex.what()));
		return false;
	}

	manifest.name = json.value("name", "");
	manifest.version = json.value("version", "");
	manifest.release = json.value("release", "1");
	manifest.build = json.value("build", "1");
	manifest.installer_hash = json.value("installer_hash", "");
	manifest.repo = json.value("repo", "local");
	manifest.desc = json.value("desc", "");
	manifest.arch = json.value("arch", "any");
	manifest.filename = json.value("filename", manifest.name + "-" + manifest.version + ".pwpkg");
	manifest.source_type = json.value("source_type", "payload") == "wrapped-installer"
		? PackageSourceType::WrappedInstaller
		: PackageSourceType::Payload;

	for(const auto& dep : json.value("depends", nlohmann::json::array())) {
		manifest.depends.push_back(parse_dep(dep.get<std::string>()));
	}
	for(const auto& provide : json.value("provides", nlohmann::json::array())) {
		manifest.provides.push_back(parse_dep(provide.get<std::string>()));
	}
	for(const auto& item : json.value("files", nlohmann::json::array())) {
		manifest.files.push_back(FileEntry{
			item.value("source", ""),
			item.value("destination", ""),
			item.value("directory", false)
		});
	}
	for(const auto& item : json.value("registry", nlohmann::json::array())) {
		manifest.registry.push_back(RegistryEntry{
			item.value("hive", ""),
			item.value("key", ""),
			item.value("name", ""),
			item.value("type", ""),
			item.value("value", "")
		});
	}
	for(const auto& item : json.value("shortcuts", nlohmann::json::array())) {
		manifest.shortcuts.push_back(ShortcutSpec{
			item.value("path", ""),
			item.value("target", "")
		});
	}
	if(json.contains("trace_scope")) {
		TraceScope scope;
		const auto& trace_scope = json["trace_scope"];
		scope.roots = trace_scope.value("roots", std::vector<std::string>{});
		scope.registry_keys = trace_scope.value("registry_keys", std::vector<std::string>{});
		manifest.trace_scope = scope;
	}
	if(json.contains("installer")) {
		InstallerSpec spec;
		const auto& installer = json["installer"];
		spec.executable = installer.value("executable", "");
		spec.arguments = installer.value("arguments", std::vector<std::string>{});
		spec.working_directory = installer.value("working_directory", "");
		if(installer.contains("uninstall")) {
			UninstallRecord record;
			record.command = installer["uninstall"].value("command", "");
			record.working_directory = installer["uninstall"].value("working_directory", "");
			record.arguments = installer["uninstall"].value("arguments", std::vector<std::string>{});
			spec.uninstall = record;
		}
		manifest.installer = spec;
	}

	return !manifest.name.empty() && !manifest.version.empty();
}

bool windows_pkg_extract(
	const std::filesystem::path& package_path,
	const std::filesystem::path& destination_root,
	ExtractedPackage& extracted,
	std::vector<std::string>& errors)
{
	extracted = {};
	extracted.extracted_root = destination_root;
	std::error_code ec;
	std::filesystem::remove_all(destination_root, ec);
	std::filesystem::create_directories(destination_root, ec);
	if(ec) {
		errors.push_back("failed to prepare extraction directory: " + destination_root.string());
		return false;
	}

	archive *ar = archive_read_new();
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if(archive_read_open_filename(ar, package_path.string().c_str(), 10240) != ARCHIVE_OK) {
		errors.push_back("failed to open package archive: " + package_path.string());
		archive_read_free(ar);
		return false;
	}

	archive_entry *entry = nullptr;
	while(archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		const char *path_raw = archive_entry_pathname(entry);
		if(!path_raw) {
			archive_read_data_skip(ar);
			continue;
		}
		auto out_path = destination_root / std::filesystem::path(path_raw);
		if(archive_entry_filetype(entry) == AE_IFDIR) {
			std::filesystem::create_directories(out_path, ec);
			archive_read_data_skip(ar);
			continue;
		}
		std::filesystem::create_directories(out_path.parent_path(), ec);
		std::ofstream out(out_path, std::ios::binary);
		if(!out) {
			errors.push_back("failed to extract file: " + out_path.string());
			archive_read_free(ar);
			return false;
		}
		char buffer[8192];
		for(;;) {
			auto read = archive_read_data(ar, buffer, sizeof(buffer));
			if(read == 0) {
				break;
			}
			if(read < 0) {
				errors.push_back("failed while extracting package archive: " + package_path.string());
				archive_read_free(ar);
				return false;
			}
			out.write(buffer, read);
		}
		out.close();
		std::filesystem::permissions(
			out_path,
			static_cast<std::filesystem::perms>(archive_entry_perm(entry)),
			std::filesystem::perm_options::replace,
			ec);
	}
	archive_read_close(ar);
	archive_read_free(ar);

	if(!windows_pkg_read_manifest(destination_root / "manifest.json", extracted.manifest, errors)) {
		return false;
	}
	extracted.payload_root = destination_root / "payload";
	return true;
}

} /* namespace windows */
} /* namespace pacman */
