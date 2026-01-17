#ifndef VFS_HPP
#define VFS_HPP

#include <stdint.h>
#include <stddef.h>

namespace MesaOS::FS {

enum FSNodeType {
    FS_FILE = 0x01,
    FS_DIRECTORY = 0x02,
    FS_CHARDEVICE = 0x03,
    FS_BLOCKDEVICE = 0x04,
    FS_PIPE = 0x05,
    FS_SYMLINK = 0x06,
    FS_MOUNTPOINT = 0x08
};

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef struct dirent * (*readdir_type_t)(struct fs_node*, uint32_t);
typedef struct fs_node * (*finddir_type_t)(struct fs_node*, const char *name);

struct fs_node {
    char name[128];
    uint32_t mask;
    uint32_t uid;
    uint32_t gid;
    uint32_t flags;
    uint32_t inode;
    uint32_t length;
    uint32_t impl;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    struct fs_node *ptr; // Used by mountpoints and symlinks
};

struct dirent {
    char name[128];
    uint32_t ino;
};

extern fs_node *fs_root;

uint32_t read_fs(fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void open_fs(fs_node *node, uint8_t read, uint8_t write);
void close_fs(fs_node *node);
struct dirent *readdir_fs(fs_node *node, uint32_t index);
fs_node *finddir_fs(fs_node *node, const char *name);
fs_node *find_path_fs(fs_node *root, const char *path);

} // namespace MesaOS::FS

#endif
