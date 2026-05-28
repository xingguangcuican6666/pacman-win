#pragma once
// #include "include.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "json.hpp"
#define PACMAN
#include "include/define.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef IGNORE
#undef IGNORE
#endif
#endif

struct config_Main
{
    bool HELP = 0;
    bool VERSION = 0;
    bool DATABASE = 0;
    bool FILES = 0;
    bool QUERY = 0;
    bool REMOVE = 0;
    bool SYNC = 0;
    bool DEPTEST = 0;
    bool UPGRADE = 0;
};

inline config_Main config;

struct config_Sync
{
    std::string DBPATH;
    int CLEAN = 0;
    int NODEPS = 0;
    int GROUPS = 0;
    int INFO = 0;
    std::string LIST;
    bool PRINT = 0;
    bool QUIET = 0;
    std::string ROOT;
    std::string SEARCH;
    int SYSUPGRADE = 0;
    bool VERBOSE = 0;
    bool DOWNLOADONLY = 0;
    int REFRESH = 0;
    std::string ARCH;
    bool ASDEPS = 0;
    bool ASEXPLICIT = 0;
    std::vector<std::string> ASSUME_INSTALLED;
    std::string CACHEDIR;
    std::string COLOR;
    std::string CONFIG;
    bool CONFIRM = 0;
    bool DBONLY = 0;
    bool DEBUG = 0;
    bool DISABLE_DOWNLOAD_TIMEOUT = 0;
    bool DISABLE_SANDBOX = 0;
    bool DISABLE_SANDBOX_FILESYSTEM = 0;
    bool DISABLE_SANDBOX_SYSCALLS = 0;
    std::string GPGDIR;
    std::string HOOKDIR;
    std::vector<std::string> IGNORE;
    std::vector<std::string> IGNOREGROUP;
    std::string LOGFILE;
    bool NEEDED = 0;
    bool NOCONFIRM = 0;
    bool NOPROGRESSBAR = 0;
    bool NOSCRIPTLET = 0;
    std::vector<std::string> OVERWRITE;
    std::string PRINT_FORMAT;
    bool SYSROOT = 0;
};

inline config_Sync config_sy;

inline std::map<int, std::string> language_map;

inline int register_language(int lang_id){
    if(lang_id < 0x01) return ERROR_INVALID_LANGUAGE;
    nlohmann::json lang_json;
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) {
        return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
    }
    std::filesystem::path exe_path(buffer);
    std::filesystem::path lang_path = exe_path.parent_path() / "data" / (std::to_string(lang_id) + ".dat");
    std::ifstream lang_file(lang_path, std::ios_base::in | std::ios_base::binary);
#else
    std::ifstream lang_file("/usr/lib/pacman/data/"+std::to_string(lang_id)+".dat", std::ios_base::in | std::ios_base::binary);
#endif
    if(!lang_file.is_open()) return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
    return NORMAL;
};
