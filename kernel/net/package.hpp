#ifndef PACKAGE_HPP
#define PACKAGE_HPP

#include <stdint.h>
#include <stddef.h>

namespace MesaOS::Net {

// Package metadata structure
struct PackageInfo {
    char name[64];
    char version[16];
    char description[256];
    uint32_t size;
    char dependencies[512]; // Comma-separated list
    char checksum[64];      // SHA-256
};

class PackageManager {
public:
    static void initialize();
    static bool download_package(const char* name, const char* version = nullptr);
    static bool install_package(const char* package_path);
    static bool remove_package(const char* name);
    static bool list_installed_packages();
    static bool check_updates();
    static bool update_all_packages();
    
    // Repository management
    static bool add_repository(const char* url);
    static bool remove_repository(const char* url);
    static bool list_repositories();
    static bool sync_repositories();

private:
    static bool download_file(const char* url, const char* local_path);
    static bool verify_checksum(const char* file_path, const char* expected_checksum);
    static bool extract_package(const char* archive_path, const char* dest_dir);
    static bool register_package(const PackageInfo* info);
    static bool unregister_package(const char* name);
    
    // Repository URLs
    static const char* repositories[8];
    static int repo_count;
};

} // namespace MesaOS::Net

#endif
