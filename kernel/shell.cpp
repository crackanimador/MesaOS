#include "shell.hpp"
#include "drivers/vga.hpp"
#include "arch/i386/io_port.hpp"
#include "drivers/rtc.hpp"
#include "apps/nano.hpp"
#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"
#include "fs/mesafs.hpp"
#include "scheduler.hpp"
#include "drivers/keyboard.hpp"
#include "fs/mbr.hpp"
#include "drivers/rtl8139.hpp"
#include "net/ipv4.hpp"
#include "net/dhcp.hpp"
#include "net/arp.hpp"
#include "net/ethernet.hpp"
#include "net/icmp.hpp"
#include "drivers/pcnet.hpp"
#include <string.h>

namespace MesaOS::System {

char Shell::command_buffer[256];
size_t Shell::buffer_index = 0;
bool Shell::app_running = false;
char Shell::current_user[32] = "mesa";
bool Shell::is_admin = false;
char Shell::current_path[128] = "/";

// History
static char history[10][256];
static int history_count = 0;
static int history_cursor = 0;

// Redirection
bool Shell::redir_out = false;
char Shell::redir_file[128] = {0};

// Pipe
bool Shell::pipe_active = false;
char Shell::pipe_buffer[4096] = {0};
int Shell::pipe_cursor = 0;
bool Shell::pipe_input_available = false;

// Signals
bool Shell::abort_requested = false;

void Shell::handle_signal(int sig) {
    if (sig == 2) { // SIGINT
        abort_requested = true;
        kprint("^C\n");
    }
}

void Shell::kprint_char(char c) {
    char buf[2] = {c, 0};
    kprint(buf);
}

void Shell::kprint(const char* text) {
    if (pipe_active) {
        // Output to pipe buffer
        int len = strlen(text);
        if (pipe_cursor + len < 4095) {
            strcpy(pipe_buffer + pipe_cursor, text);
            pipe_cursor += len;
            pipe_buffer[pipe_cursor] = 0;
        }
        return; 
    }

    if (redir_out) {
        // Redirection Mode
        char full_path[256];
        if (redir_file[0] == '/') strcpy(full_path, redir_file);
        else {
            strcpy(full_path, current_path);
            if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
            strcat(full_path, redir_file);
        }

        MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
        if (!node) {
            // Create if not exists
            if (strncmp(full_path, "/disk/", 6) == 0) {
                 MesaOS::FS::MesaFS::create_file(full_path + 6);
            } else {
                 MesaOS::FS::RAMFS::create_file(redir_file, ""); 
            }
            node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
        }

        if (node) {
            MesaOS::FS::write_fs(node, node->length, strlen(text), (uint8_t*)text);
        }
    } else {
        // Normal VGA Output
        MesaOS::Drivers::VGADriver vga;
        vga.write_string(text);
    }
}

void Shell::initialize() {
    buffer_index = 0;
    MesaOS::Drivers::VGADriver vga;
    vga.initialize();
    vga.write_string("\nMesaOS Shell\n");
    prompt();
}

uint32_t string_to_ip(char* str) {
    uint32_t ip = 0;
    int octet = 0;
    int shift = 0;
    for (int i = 0; ; i++) {
        if (str[i] == '.' || str[i] == 0) {
            ip |= (octet << shift);
            shift += 8;
            octet = 0;
            if (str[i] == 0) break;
        } else if (str[i] >= '0' && str[i] <= '9') {
            octet = octet * 10 + (str[i] - '0');
        }
    }
    return ip;
}

// Helper for grep
char* k_strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
             const char* h = haystack;
             const char* n = needle;
             while (*h && *n && *h == *n) {
                 h++; n++;
             }
             if (!*n) return (char*)haystack;
        }
    }
    return 0;
}

bool Shell::is_app_running() {
    return app_running;
}

void Shell::prompt() {
    abort_requested = false; // Reset signal state
    MesaOS::Drivers::VGADriver vga;
    
    // Format: user@hostname:path$
    vga.set_color(MesaOS::Drivers::vga_entry_color(is_admin ? MesaOS::Drivers::VGAColor::LIGHT_RED : MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string(current_user);
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("@");
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("mesa-os");
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string(":");
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_BLUE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string(current_path);
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string(is_admin ? "# " : "$ ");
}

void Shell::handle_input(char c) {
    if (app_running) {
        MesaOS::Apps::Nano::handle_input(c);
        if (!MesaOS::Apps::Nano::is_running()) {
            app_running = false;
            prompt();
        }
        return;
    }

    if (MesaOS::Drivers::Keyboard::is_ctrl_pressed()) {
        if (c == 'l' || c == 'L') {
            MesaOS::Drivers::VGADriver vga_c;
            vga_c.initialize();
            prompt();
            return;
        }
    }

    MesaOS::Drivers::VGADriver vga;

    if (c == 0x11) { // Up Arrow
        if (history_count > 0) {
             // Clear current line
             while(buffer_index > 0) {
                 vga.write_string("\b \b");
                 buffer_index--;
             }
             
             // Move cursor back
             if (history_cursor > 0) history_cursor--;
             else history_cursor = (history_count < 10) ? history_count - 1 : 9; // Wrap or stay? usually stay at top
             // actually standard history (bash): up goes to older.
             // implementation: history_cursor tracks current pos.
             
             // Simplification: just show last command for now if repeated up logic is complex 
             // without proper state tracking.
             // Let's implement simple stack-like: 
             // To do it right: we need to track if we are BROWSING history or typing.
             // For now: Always get the *last* command (index history_count-1) on first up.
             
             // Let's assume history_cursor points to the command to show.
             
             strcpy(command_buffer, history[history_cursor]);
             buffer_index = strlen(command_buffer);
             vga.write_string(command_buffer);
             
             if (history_cursor > 0) history_cursor--;
        }
        return;
    }
    
    // Reset history cursor on any typing
    if (c != 0x11 && c != 0x12) {
         history_cursor = (history_count > 0) ? history_count - 1 : 0;
    }

    if (c == '\t') { // Autocomplete
        // 1. Find last space to isolate the word being typed
        int start = buffer_index;
        while(start > 0 && command_buffer[start-1] != ' ') start--;
        
        char partial[64];
        int plen = buffer_index - start;
        if (plen <= 0 || plen >= 64) return;
        
        strncpy(partial, command_buffer + start, plen);
        partial[plen] = 0;
        
        // 2. Search in current directory (or bin for commands? let's stick to current dir for files)
        MesaOS::FS::fs_node* dir = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, current_path);
        if (dir) {
            int i = 0;
            struct MesaOS::FS::dirent *de = 0;
            char match[64];
            int match_count = 0;
            
            while ((de = MesaOS::FS::readdir_fs(dir, i)) != 0) {
                if (strncmp(de->name, partial, plen) == 0) {
                     strcpy(match, de->name);
                     match_count++;
                }
                i++;
            }
            
            if (match_count == 1) {
                // Complete it!
                char* rest = match + plen;
                while (*rest) {
                    if (buffer_index < 255) {
                        command_buffer[buffer_index++] = *rest;
                        vga.put_char(*rest);
                    }
                    rest++;
                }
                // Determine if it is a directory to add '/'? Hard without resolving. 
                // Let's resolve.
                char full[256];
                if (current_path[1] == 0) { full[0]='/'; strcpy(full+1, match); } // /match
                else { strcpy(full, current_path); strcat(full, "/"); strcat(full, match); }
                
                MesaOS::FS::fs_node* child = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full);
                if (child && (child->flags & 0x07) == MesaOS::FS::FS_DIRECTORY) {
                     if (buffer_index < 255) {
                        command_buffer[buffer_index++] = '/';
                        vga.put_char('/');
                     }
                }
                
            } else if (match_count > 1) {
                // Beep or show list? For now do nothing or partial.
            }
        }
        return;
    }

    if (c == '\n') {
        command_buffer[buffer_index] = '\0';
        vga.put_char('\n');
        
        // Add to history if not empty
        if (buffer_index > 0) {
            if (history_count < 10) {
                strcpy(history[history_count], command_buffer);
                history_count++;
            } else {
                // Shift
                for(int i=0; i<9; i++) strcpy(history[i], history[i+1]);
                strcpy(history[9], command_buffer);
            }
            history_cursor = history_count - 1;
        }

        execute_command(command_buffer);
        buffer_index = 0;
        prompt();
    } else if (c == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            vga.write_string("\b \b");
        }
    } else {
        if (buffer_index < 255) {
            command_buffer[buffer_index++] = c;
            vga.put_char(c);
        }
    }
}

void Shell::execute_command(const char* command_in) {
    abort_requested = false;
    char command_processed[256];
    strcpy(command_processed, command_in);
    
    // Check for Pipe
    char* pipe_char = strchr(command_processed, '|');
    if (pipe_char) {
        *pipe_char = 0;
        char* left_cmd = command_processed;
        char* right_cmd = pipe_char + 1;
        while(*right_cmd == ' ') right_cmd++; // Skip spaces

        // Execute Left (Capture Output)
        pipe_active = true;
        pipe_cursor = 0;
        memset(pipe_buffer, 0, 4096);
        execute_command(left_cmd);
        pipe_active = false;

        // Execute Right (Process Input)
        pipe_input_available = true;
        execute_command(right_cmd);
        pipe_input_available = false;
        return;
    }

    // Check for redirection (>> before >)
    char* redir = k_strstr(command_processed, ">>");
    bool append_mode = false;
    if (redir) {
        append_mode = true;
        *redir = 0;
        redir += 2;
    } else {
        redir = strchr(command_processed, '>');
        if (redir) {
            *redir = 0;
            redir++;
        }
    }
    if (redir) {
        while(*redir == ' ') redir++;
        if (*redir != 0) {
            redir_out = true;
            strcpy(redir_file, redir);
            
            if (!append_mode) {
                // Truncate: Delete if exists
                char full_path[256];
                if (redir_file[0] == '/') strcpy(full_path, redir_file);
                else {
                    strcpy(full_path, current_path);
                    if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
                    strcat(full_path, redir_file);
                }
                MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
                if (node) {
                    MesaOS::FS::RAMFS::delete_file(redir_file);
                }
            }
        }
    }

    const char* command = command_processed;
    // MesaOS::Drivers::VGADriver vga; // Don't use direct VGA anymore
    
    // Simple argument parsing
    char cmd[128];
    char arg[128];
    memset(cmd, 0, 128);
    memset(arg, 0, 128);

    char* space = strchr(command, ' ');
    if (space) {
        strncpy(cmd, command, space - command);
        strcpy(arg, space + 1);
    } else {
        strcpy(cmd, command);
    }

    if (strcmp(cmd, "help") == 0) {
        kprint("\n      .---.      MesaOS v1.0-alpha");
        kprint("\n     /     \\     Digital. Free. Imperfect.");
        kprint("\n    | () () |    Root: /dev/internal_sd");
        kprint("\n     \\  ^  /     ");
        kprint("\n      |||||      \n");

        kprint("=== MesaOS Help System ===\n\n");
        kprint("System Core:\n");
        kprint("  sdstat   - Check internal SD health\n");
        kprint("  sync     - Flush buffers\n");
        
        kprint("\nProcess Management:\n");
        kprint("  ps       - List living processes\n");
        kprint("  kill     - Terminate a process\n");

        kprint("\nMemory & Truth:\n");
        kprint("  meminfo  - Display raw memory usage\n");

        kprint("\nUtilities:\n");
        kprint("  echo     - Send a message to the void\n");
        kprint("  clear    - Wipe the screen, keep the soul\n");
        kprint("  help     - Show this manual\n");
        kprint("  exit     - Return to the dark void\n");
        kprint("  lspci    - List PCI devices\n");
        kprint("  dhcp     - Request IP via DHCP\n");
        kprint("  ping     - Send ICMP Echo\n");
        kprint("  grep     - Filter output (use with | )\n");

        kprint("\n[!] Remember: Reality is imperfect. Glitches are features.\n");
    } else if (strcmp(cmd, "echo") == 0) {
        kprint(arg); kprint("\n");
    } else if (strcmp(cmd, "whoami") == 0) {
        kprint(current_user); kprint("\n");
    } else if (strcmp(cmd, "uname") == 0) {
        if (strcmp(arg, "-a") == 0) kprint("MesaOS mesa-os 0.3-persist i686 GNU/Mesa\n");
        else kprint("MesaOS\n");
    } else if (strcmp(cmd, "lspci") == 0) {
        MesaOS::Drivers::PCIDriver::scan();
        int count = MesaOS::Drivers::PCIDriver::get_device_count();
        MesaOS::Drivers::PCIDevice* devices = MesaOS::Drivers::PCIDriver::get_devices();
        kprint("PCI Devices Found:\n");
        for (int i = 0; i < count; i++) {
             MesaOS::Drivers::PCIDevice& dev = devices[i];
             char buf[16];
             kprint("Bus "); kprint(itoa(dev.bus, buf, 10));
             kprint(" Dev "); kprint(itoa(dev.device, buf, 10));
             kprint(": ");
             kprint("Vendor 0x"); kprint(itoa(dev.vendor_id, buf, 16));
             kprint(" Device 0x"); kprint(itoa(dev.device_id, buf, 16));
             kprint("\n");
        }
    } else if (strcmp(cmd, "top") == 0) {
        MesaOS::Drivers::VGADriver vga; // Local instance for special screen control
        vga.initialize();
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::BLACK, MesaOS::Drivers::VGAColor::LIGHT_GREY));
        vga.write_string(" MesaOS Task Manager | CPU: 100% | Press any key to exit                    \n");
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
        execute_command("ps");
        vga.write_string("\n[ TASK SWITCHING: ENABLED ]");
    } else if (strcmp(cmd, "cd") == 0) {
        if (strlen(arg) > 0) {
            if (strcmp(arg, "/") == 0) {
                strcpy(current_path, "/");
            } else if (strcmp(arg, "..") == 0) {
                // To keep it simple, /disk is a top-level dir. 
                // We just reset to root for now if in a dir.
                strcpy(current_path, "/");
            } else {
                // Check if directory exists
                char full_path[256];
                if (arg[0] == '/') strcpy(full_path, arg);
                else {
                    strcpy(full_path, current_path);
                    if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
                    strcat(full_path, arg);
                }
                
                MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
                if (node && (node->flags & 0x07) == MesaOS::FS::FS_DIRECTORY) {
                    strcpy(current_path, full_path);
                } else {
                    kprint("cd: "); kprint(arg); kprint(": No such directory\n");
                }
            }
        } else {
            strcpy(current_path, "/");
        }
    } else if (strcmp(cmd, "su") == 0) {
        if (strlen(arg) > 0) {
            strcpy(current_user, arg);
            is_admin = (strcmp(arg, "root") == 0);
            kprint("Switched to user: ");
            kprint(arg);
            kprint("\n");
        } else {
            kprint("Usage: su <username>\n");
        }
    } else if (strcmp(cmd, "fdisk") == 0) {
        MesaOS::FS::PartitionManager::list_partitions();
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (strlen(arg) > 0) {
            // ... (rest of mkdir logic same)
            char full_path[256];
            if (arg[0] == '/') strcpy(full_path, arg);
            else {
                strcpy(full_path, current_path);
                if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
                strcat(full_path, arg);
            }
            
            if (strncmp(full_path, "/disk/", 6) == 0) {
                MesaOS::FS::MesaFS::create_dir(full_path + 6);
            } else {
                MesaOS::FS::RAMFS::create_dir(arg); // Simple RAMFS mkdir
            }
            kprint("Directory created.\n");
        } else {
            kprint("Usage: mkdir <dir_name>\n");
        }
    } else if (strcmp(cmd, "run") == 0) {
        if (strcmp(arg, "nano") == 0) {
             app_running = true;
             MesaOS::Apps::Nano::run(0);
        } else {
            kprint("Executable '");
            kprint(arg);
            kprint("' not found in /bin\n");
        }
    } else if (strcmp(cmd, "ls") == 0) {
        char full_path[256];
        if (strlen(arg) == 0) strcpy(full_path, current_path);
        else if (arg[0] == '/') strcpy(full_path, arg);
        else {
            strcpy(full_path, current_path);
            if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
            strcat(full_path, arg);
        }
        MesaOS::FS::fs_node* dir = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
        if (dir) {
            int i = 0;
            struct MesaOS::FS::dirent *de = 0;
            while ((de = MesaOS::FS::readdir_fs(dir, i)) != 0) {
                // Resolve type for coloring
                // We need to construct full path temporarily to checking child node type
                MesaOS::FS::fs_node* child = MesaOS::FS::finddir_fs(dir, de->name);
                if (child && (child->flags & 0x07) == MesaOS::FS::FS_DIRECTORY) {
                     // Colors unsupported in kprint yet without ANSI parsing? 
                     // For redirection, we shouldn't output colors?
                     // kprint separates abstractly.
                     // For now just print name.
                     kprint(de->name);
                     kprint("/");
                } else {
                     kprint(de->name);
                }
                kprint("  ");
                i++;
            }
            kprint("\n");
        } else {
            kprint("Directory not found.\n");
        }
    } else if (strcmp(cmd, "touch") == 0) {
        if (strlen(arg) > 0) {
            char full_path[256];
            if (arg[0] == '/') strcpy(full_path, arg);
            else {
                strcpy(full_path, current_path);
                if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
                strcat(full_path, arg);
            }
            
            if (strncmp(full_path, "/disk/", 6) == 0) {
                MesaOS::FS::MesaFS::create_file(full_path + 6);
            } else {
                MesaOS::FS::RAMFS::create_file(arg, ""); // Still use relative name for RAMFS internals for now
            }
            kprint("File created.\n");
        } else {
            kprint("Usage: touch <filename>\n");
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (strlen(arg) > 0) {
            char full_path[256];
            if (arg[0] == '/') strcpy(full_path, arg);
            else {
                strcpy(full_path, current_path);
                if (current_path[strlen(current_path)-1] != '/') strcat(full_path, "/");
                strcat(full_path, arg);
            }
            MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, full_path);
            if (node) {
                char buf[2048];
                uint32_t len = MesaOS::FS::read_fs(node, 0, 2047, (uint8_t*)buf);
                buf[len] = '\0';
                kprint(buf);
                kprint("\n");
            } else {
                kprint("File not found.\n");
            }
        } else {
            kprint("Usage: cat <filename>\n");
        }
    } else if (strcmp(cmd, "grep") == 0) {
        if (strlen(arg) > 0) {
            if (pipe_input_available) {
                // Process pipe_buffer line by line
                char* line_start = pipe_buffer;
                char* p = pipe_buffer;
                while (*p) {
                    if (*p == '\n') {
                        *p = 0; // Temp null terminate
                        if (k_strstr(line_start, arg)) {
                            kprint(line_start); kprint("\n");
                        }
                        *p = '\n'; // Restore
                        line_start = p + 1;
                    }
                    p++;
                }
                // Last line
                if (k_strstr(line_start, arg)) {
                    kprint(line_start); kprint("\n");
                }
            } else {
                 kprint("grep: Usage: <command> | grep <pattern> (File grep not impl)\n");
            }
        } else {
             kprint("Usage: grep <pattern>\n");
        }
    } else if (strcmp(cmd, "rm") == 0) {
        if (strlen(arg) > 0) {
            if (MesaOS::FS::RAMFS::delete_file(arg)) {
                kprint("File deleted.\n");
            } else {
                kprint("File not found.\n");
            }
        } else {
            kprint("Usage: rm <filename>\n");
        }
    } else if (strcmp(cmd, "ifconfig") == 0) {
        kprint("eth0: Link encap:Ethernet  HWaddr ");
        uint8_t* mac = MesaOS::Net::Ethernet::get_mac_address();
        char b[16];
        for (int k = 0; k < 6; k++) {
            kprint(itoa(mac[k], b, 16));
            if (k < 5) kprint(":");
        }
        kprint("\n      inet addr:");
        uint32_t ip = MesaOS::Net::IPv4::get_ip();
        if (ip == 0) {
            kprint("0.0.0.0 (No IP assigned)");
        } else {
            kprint(itoa(ip & 0xFF, b, 10)); kprint(".");
            kprint(itoa((ip >> 8) & 0xFF, b, 10)); kprint(".");
            kprint(itoa((ip >> 16) & 0xFF, b, 10)); kprint(".");
            kprint(itoa((ip >> 24) & 0xFF, b, 10));
        }
        kprint("\n      UP BROADCAST RUNNING MULTICAST\n");
    } else if (strcmp(cmd, "nano") == 0) {
        app_running = true;
        char full_arg[256];
        if (arg[0] == '/') strcpy(full_arg, arg);
        else {
            strcpy(full_arg, current_path);
            if (current_path[strlen(current_path)-1] != '/') strcat(full_arg, "/");
            strcat(full_arg, arg);
        }
        MesaOS::Apps::Nano::run(strlen(arg) > 0 ? full_arg : 0);
    } else if (strcmp(cmd, "ps") == 0) {
        kprint("PID  NAME      STATUS\n");
        MesaOS::System::Process* proc = MesaOS::System::Scheduler::get_process_list();
        while (proc) {
            char buf[10];
            kprint(itoa(proc->pid, buf, 10));
            kprint("    ");
            kprint(proc->name);
            for (size_t k = strlen(proc->name); k < 10; k++) kprint(" ");
            
            if (proc->state == MesaOS::System::READY) kprint("READY\n");
            else if (proc->state == MesaOS::System::RUNNING) kprint("RUNNING\n");
            else kprint("SUSPENDED\n");
            
            proc = proc->next;
        }
    } else if (strcmp(cmd, "tree") == 0) {
        kprint("/\n");
        int i = 0;
        struct MesaOS::FS::dirent *node = 0;
        while ((node = MesaOS::FS::readdir_fs(MesaOS::FS::fs_root, i)) != 0) {
            kprint("|- ");
            kprint(node->name);
            kprint("\n");
            i++;
        }
    } else if (strcmp(cmd, "dhcp") == 0) {
        kprint("Configuring network via DHCP... ");
        MesaOS::Net::DHCP::discover();
        kprint("SENT.\nUse 'ifconfig' to check status.\n");
    } else if (strcmp(cmd, "ping") == 0) {
        if (strlen(arg) > 0) {
            uint32_t target_ip = string_to_ip(arg);
            
            kprint("Pinging "); kprint(arg); kprint(" with 32 bytes of data:\n");
            
             for(int i=0; i<4; i++) {
                 if (abort_requested) break;
                 MesaOS::Net::ICMP::send_echo_request(target_ip, 0x1234, i+1);
                 
                 // Wait for reply (polling)
                 for(volatile int j=0; j<20000000; j++) {
                      MesaOS::Drivers::PCNet::poll();
                      if (abort_requested) break;
                 }
             }
             kprint("Ping statistics: Transmitted packets.\n");
         } else {
              kprint("Usage: ping <ip_address>\n");
         }
    } else if (strcmp(cmd, "lspci") == 0) {
        // Redundant block, removal or merge recommended, 
        // but replacing vga for now to fix compile.
        int count = MesaOS::Drivers::PCIDriver::get_device_count();
        char b[16];
        kprint("PCI Devices Found: "); kprint(itoa(count, b, 10)); kprint("\n");
        for (int k = 0; k < count; k++) {
            MesaOS::Drivers::PCIDevice& dev = MesaOS::Drivers::PCIDriver::get_devices()[k];
            kprint(itoa(dev.bus, b, 16)); kprint(":");
            kprint(itoa(dev.device, b, 16)); kprint(".");
            kprint(itoa(dev.function, b, 16)); kprint(" - ");
            kprint(itoa(dev.vendor_id, b, 16)); kprint(":");
            kprint(itoa(dev.device_id, b, 16)); kprint(" (Class ");
            kprint(itoa(dev.class_id, b, 16)); kprint(")\n");
        }
    } else if (strcmp(cmd, "ping") == 0) {
        // Redundant block 2 found in original code at end?
        if (strlen(arg) > 0) {
             kprint("Pinging "); kprint(arg); kprint("...\n");
             // ...
             kprint("ICMP Echo Request (Broadcast) sent!\n");
        } else {
             kprint("Usage: ping <ip_address>\n");
        }
    } else if (strcmp(cmd, "about") == 0) {
        MesaOS::Drivers::VGADriver vga; // Needs colors
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_CYAN, MesaOS::Drivers::VGAColor::BLACK));
        vga.write_string("\nMesaOS v0.3 - Premium Minimalistic OS\n");
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
        vga.write_string("Designed for speed, built with passion.\n\n");
    } else if (strcmp(cmd, "clock") == 0) {
        MesaOS::Drivers::Time t = MesaOS::Drivers::RTC::get_time();
        char buf[10];
        itoa(t.hour, buf, 10);
        if (t.hour < 10) kprint("0");
        kprint(buf);
        kprint(":");
        itoa(t.minute, buf, 10);
        if (t.minute < 10) kprint("0");
        kprint(buf);
        kprint(":");
        itoa(t.second, buf, 10);
        if (t.second < 10) kprint("0");
        kprint(buf);
        kprint("\n");
    } else if (strcmp(command, "clear") == 0) {
        MesaOS::Drivers::VGADriver vga; vga.initialize(); 
    } else if (strcmp(command, "reboot") == 0) {
        kprint("Rebooting...\n");
        // Pulse the CPU reset line
        uint8_t good = 0x02;
        while (good & 0x02)
            good = MesaOS::Arch::x86::inb(0x64);
        MesaOS::Arch::x86::outb(0x64, 0xFE);
    } else if (strlen(command) == 0) {
        // Do nothing for empty command
    } else {
        kprint("Unknown command: ");
        kprint(command);
        kprint("\n");
    }
    
    redir_out = false;
}

} // namespace MesaOS::System
