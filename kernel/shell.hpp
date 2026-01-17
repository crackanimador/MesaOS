#ifndef SHELL_HPP
#define SHELL_HPP

#include <stddef.h>

namespace MesaOS::System {

class Shell {
public:
    static void initialize();
    static void handle_input(char c);
    static void execute_command(const char* command);
    static bool is_app_running();
    static void prompt();
    
    // Output Abstraction
    static void kprint(const char* text);
    static void kprint_char(char c);
    static void handle_signal(int sig);
    static bool abort_requested;

private:
    static bool app_running;
    static char command_buffer[256];
    static size_t buffer_index;
    
    // Redirection State
    static bool redir_out;
    static char redir_file[128];

    // Pipe State
    static bool pipe_active; // True if output goes to pipe buffer
    static char pipe_buffer[4096];
    static int pipe_cursor;
    static bool pipe_input_available; // True if next cmd should read from pipe
    
    static char current_user[32];
    static bool is_admin;
    static char current_path[128];
};

} // namespace MesaOS::System

#endif
