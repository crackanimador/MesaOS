#ifndef NANO_HPP
#define NANO_HPP

#include <stddef.h>
#include <stdint.h>

namespace MesaOS::Apps {

class Nano {
public:
    static void run(const char* filename);
    static void handle_input(char c);
    static bool is_running();

private:
    static char buffer[4096];
    static char clipboard[512];
    static char current_file[128];
    static size_t cursor_pos;
    static bool running;
    static bool show_help;
    static void refresh_screen();
    static void save();
    static void cut_line();
    static void paste_line();
    static void search();
    static void go_to_line();
    static void show_cursor_pos();
};

} // namespace MesaOS::Apps

#endif
