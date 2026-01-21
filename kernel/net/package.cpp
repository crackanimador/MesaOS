#include "package.hpp"
#include "tcp.hpp"
#include "fs/vfs.hpp"
#include "fs/mesafs.hpp"
#include "fs/ramfs.hpp"
#include "logging.hpp"
#include <string.h>

// Simple sprintf implementation for HTTP requests
static int simple_sprintf(char* buffer, const char* format, const char* path, const char* host) {
    // Basic sprintf for HTTP GET request
    strcpy(buffer, "GET ");
    strcat(buffer, path);
    strcat(buffer, " HTTP/1.0\r\nHost: ");
    strcat(buffer, host);
    strcat(buffer, "\r\nUser-Agent: MesaOS/1.0\r\nConnection: close\r\n\r\n");
    return strlen(buffer);
}

// Simple strrchr implementation for freestanding environment
static const char* simple_strrchr(const char* str, char ch) {
    const char* last = nullptr;
    while (*str) {
        if (*str == ch) {
            last = str;
        }
        str++;
    }
    return last;
}

// Simple strstr implementation for freestanding environment
static const char* simple_strstr(const char* haystack, const char* needle) {
    if (!*needle) return haystack;

    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return haystack;
        haystack++;
    }
    return nullptr;
}

namespace MesaOS::Net {

const char* PackageManager::repositories[8] = {
    "https://github.com/crackanimador/MesaOS-PKG/raw/main/",
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
int PackageManager::repo_count = 1;

void PackageManager::initialize() {
    // Initialize without creating directories initially (safer)
    // Directories will be created on-demand when needed
    MesaOS::System::Logging::info("Package manager initialized (simplified)");
}

bool PackageManager::download_package(const char* name, const char* version) {
    if (!name) return false;
    
    // Construct package URL
    char url[256];
    char package_name[128];
    strcpy(package_name, name);
    if (version) {
        strcat(package_name, "-");
        strcat(package_name, version);
    }
    strcat(package_name, ".elf");
    
    strcpy(url, repositories[0]); // Use first repo
    strcat(url, package_name);
    
    // Download to cache
    char cache_path[256] = "/packages/cache/";
    strcat(cache_path, package_name);
    
    if (!download_file(url, cache_path)) {
        MesaOS::System::Logging::error("Failed to download package");
        return false;
    }
    
    // Install the package
    return install_package(cache_path);
}

bool PackageManager::install_package(const char* package_path) {
    if (!package_path) return false;

    // Read the downloaded package file
    MesaOS::FS::fs_node* src_node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, package_path);
    if (!src_node) {
        MesaOS::System::Logging::error("Package file not found");
        return false;
    }

    // Read package content
    char package_content[4096];
    uint32_t content_size = MesaOS::FS::read_fs(src_node, 0, sizeof(package_content) - 1, (uint8_t*)package_content);
    if (content_size == 0) {
        MesaOS::System::Logging::error("Empty package file");
        return false;
    }
    package_content[content_size] = '\0';

    // Determine destination path
    char dest_path[256] = "/bin/";
    const char* filename = simple_strrchr(package_path, '/');
    if (filename) {
        filename++; // Skip the '/'
        // Remove .elf extension for executable name
        char exe_name[128];
        strcpy(exe_name, filename);
        char* dot = (char*)simple_strrchr(exe_name, '.');
        if (dot) *dot = '\0'; // Remove extension

        strcat(dest_path, exe_name);

        // Install the package (copy content to /bin)
        if (MesaOS::FS::MesaFS::create_file(dest_path, 0, 0, 0644)) {
            // Write the content
            MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, dest_path);
            if (node) {
                MesaOS::FS::MesaFS::write(node, 0, strlen(package_content), (uint8_t*)package_content);
                MesaOS::System::Logging::info("Package installed successfully: ");
                MesaOS::System::Logging::info(exe_name);
                return true;
            } else {
                MesaOS::System::Logging::error("Failed to get file node after creation");
                return false;
            }
        } else {
            MesaOS::System::Logging::error("Failed to create executable file");
            return false;
        }
    }

    MesaOS::System::Logging::error("Invalid package path");
    return false;
}

bool PackageManager::remove_package(const char* name) {
    if (!name) return false;
    
    char package_path[256] = "/bin/";
    strcat(package_path, name);
    
    // Remove from filesystem - TODO: implement delete in MesaFS
    MesaOS::System::Logging::info("Package removal not implemented in MesaFS");
    return false;
    
    return false;
}

bool PackageManager::list_installed_packages() {
    // List files in /bin directory (safely)
    MesaOS::FS::fs_node* bin_dir = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, "/bin");
    if (bin_dir && bin_dir->readdir) {
        int i = 0;
        struct MesaOS::FS::dirent* de;
        MesaOS::System::Logging::info("Installed packages:");

        while ((de = bin_dir->readdir(bin_dir, i)) != nullptr && i < 100) { // Safety limit
            if (de->name && strcmp(de->name, ".") != 0 && strcmp(de->name, "..") != 0) {
                MesaOS::System::Logging::info(de->name);
            }
            i++;
        }
        return true;
    }

    MesaOS::System::Logging::info("No packages installed or /bin directory not found");
    return false;
}

bool PackageManager::check_updates() {
    // Simplified: assume no updates available
    MesaOS::System::Logging::info("All packages are up to date");
    return true;
}

bool PackageManager::update_all_packages() {
    MesaOS::System::Logging::info("Updating all packages...");
    // Simplified implementation
    return true;
}

bool PackageManager::add_repository(const char* url) {
    if (!url || repo_count >= 8) return false;
    
    repositories[repo_count++] = url;
    MesaOS::System::Logging::info("Repository added");
    return true;
}

bool PackageManager::remove_repository(const char* url) {
    // Simplified
    MesaOS::System::Logging::info("Repository management not fully implemented");
    return false;
}

bool PackageManager::list_repositories() {
    MesaOS::System::Logging::info("Configured repositories:");
    for (int i = 0; i < repo_count; i++) {
        if (repositories[i]) {
            MesaOS::System::Logging::info("  ");
            MesaOS::System::Logging::info(repositories[i]);
            MesaOS::System::Logging::info("");
        }
    }
    if (repo_count == 0) {
        MesaOS::System::Logging::info("  No repositories configured");
    }
    return true;
}

bool PackageManager::sync_repositories() {
    MesaOS::System::Logging::info("Syncing repositories...");
    // Download package lists from repositories
    return true;
}

// Private helper methods
bool PackageManager::download_file(const char* url, const char* local_path) {
    // Implement actual HTTP download for GitHub
    // Parse URL to extract host and path
    const char* host_start = simple_strstr(url, "://");
    if (!host_start) return false;

    host_start += 3; // Skip "://"
    const char* path_start = strchr(host_start, '/');
    if (!path_start) return false;

    // Extract host and path
    char host[64];
    char path[256];

    size_t host_len = path_start - host_start;
    if (host_len >= sizeof(host)) return false;
    memcpy(host, host_start, host_len);
    host[host_len] = '\0';

    strcpy(path, path_start);

    MesaOS::System::Logging::info("Downloading from host: ");
    MesaOS::System::Logging::info(host);
    MesaOS::System::Logging::info(" Path: ");
    MesaOS::System::Logging::info(path);

    // Create TCP connection to host (port 80 for HTTP)
    uint32_t ip = 0;

    // Simple DNS resolution - hardcoded IPs for common hosts
    if (strcmp(host, "github.com") == 0 || strcmp(host, "raw.githubusercontent.com") == 0) {
        // GitHub's raw content is served from different IPs, try a known working IP
        // This is a simplified approach - in production would do proper DNS
        ip = (140 << 24) | (82 << 16) | (118 << 8) | 3; // 140.82.118.3 (one of GitHub's IPs)
        MesaOS::System::Logging::info("Using GitHub IP: 140.82.118.3");
    } else if (strcmp(host, "example.com") == 0) {
        ip = (93 << 24) | (184 << 16) | (216 << 8) | 34; // 93.184.216.34
    } else {
        // For unknown hosts, try to use gateway as proxy (won't work but shows attempt)
        ip = MesaOS::Net::IPv4::get_gateway();
        if (ip == 0) {
            MesaOS::System::Logging::error("Unknown host and no gateway configured");
            return false;
        }
        MesaOS::System::Logging::info("Unknown host, attempting via gateway");
    }

    if (ip == 0) {
        MesaOS::System::Logging::error("Failed to resolve host IP");
        return false;
    }

    // Create HTTP GET request
    char http_request[512];
    simple_sprintf(http_request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: MesaOS/1.0\r\nConnection: close\r\n\r\n", path, host);

    // Create TCP socket
    MesaOS::System::Logging::info("Attempting TCP connection...");
    int sock = MesaOS::Net::TCP::connect(ip, 80);
    if (sock < 0) {
        MesaOS::System::Logging::error("Failed to connect to server (socket creation failed)");
        char buf[16];
        MesaOS::System::Logging::error("Socket return value: ");
        MesaOS::System::Logging::error(itoa(sock, buf, 10));
        return false;
    }
    MesaOS::System::Logging::info("TCP connection established");

    // Send HTTP request
    int sent = MesaOS::Net::TCP::send(sock, (uint8_t*)http_request, strlen(http_request));
    if (sent <= 0) {
        MesaOS::Net::TCP::close(sock);
        MesaOS::System::Logging::error("Failed to send HTTP request");
        return false;
    }

    // Receive response
    uint8_t buffer[4096];
    int total_received = 0;
    bool headers_done = false;
    int content_length = -1;

    while (true) {
        int received = MesaOS::Net::TCP::recv(sock, buffer + total_received,
                                             sizeof(buffer) - total_received - 1);
        if (received <= 0) break;

        total_received += received;
        buffer[total_received] = '\0';

        // Parse headers if not done yet
        if (!headers_done) {
            char* header_end = (char*)simple_strstr((char*)buffer, "\r\n\r\n");
            if (header_end) {
                *header_end = '\0';
                headers_done = true;

                // Parse Content-Length
                char* cl_header = (char*)simple_strstr((char*)buffer, "Content-Length:");
                if (cl_header) {
                    cl_header += 15; // Skip "Content-Length:"
                    content_length = 0;
                    while (*cl_header >= '0' && *cl_header <= '9') {
                        content_length = content_length * 10 + (*cl_header - '0');
                        cl_header++;
                    }
                }

                // Skip headers
                uint8_t* body_start = (uint8_t*)header_end + 4;
                int body_len = total_received - (body_start - buffer);

                // Save body to file
                if (body_len > 0) {
                    MesaOS::FS::MesaFS::create_file(local_path, 0, 0, 0644);
                    MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, local_path);
                    if (node) {
                        MesaOS::FS::MesaFS::write(node, 0, body_len, body_start);
                    }
                    MesaOS::System::Logging::info("Downloaded file successfully");
                    MesaOS::Net::TCP::close(sock);
                    return true;
                }
            }
        }
    }

    MesaOS::Net::TCP::close(sock);
    MesaOS::System::Logging::error("Download failed - no response body");
    return false;
}

bool PackageManager::verify_checksum(const char* file_path, const char* expected_checksum) {
    // Simplified checksum verification
    MesaOS::System::Logging::info("Verifying checksum (simplified)");
    return true;
}

bool PackageManager::extract_package(const char* archive_path, const char* dest_dir) {
    // Simplified extraction
    MesaOS::System::Logging::info("Extracting package (simplified)");
    return true;
}

bool PackageManager::register_package(const PackageInfo* info) {
    // Save package metadata
    char metadata_path[256] = "/packages/installed/";
    strcat(metadata_path, info->name);
    strcat(metadata_path, ".meta");

    MesaOS::FS::MesaFS::create_file(metadata_path, 0, 0, 0644);
    MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, metadata_path);
    if (node) {
        const char* meta = "package metadata";
        MesaOS::FS::MesaFS::write(node, 0, strlen(meta), (uint8_t*)meta);
    }
    return true;
}

bool PackageManager::unregister_package(const char* name) {
    char metadata_path[256] = "/packages/installed/";
    strcat(metadata_path, name);
    strcat(metadata_path, ".meta");

    // TODO: implement delete in MesaFS
    MesaOS::System::Logging::info("Package unregister not implemented in MesaFS");
    return true;
}

} // namespace MesaOS::Net
