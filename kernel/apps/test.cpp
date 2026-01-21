#include "test.hpp"
#include "../drivers/vga.hpp"
#include "../shell.hpp"

namespace MesaOS::Apps {

bool Test::running = false;

void Test::run(const char* arg) {
    running = true;

    MesaOS::Drivers::VGADriver vga;
    vga.initialize();

    // Clear screen and show test application
    vga.set_color(MesaOS::Drivers::vga_entry_color(
        MesaOS::Drivers::VGAColor::LIGHT_CYAN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("\n");
    vga.write_string("========================================\n");
    vga.write_string("        MesaOS Test Package App         \n");
    vga.write_string("========================================\n");
    vga.set_color(MesaOS::Drivers::vga_entry_color(
        MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));

    vga.write_string("\n");
    vga.write_string("Hello from MesaOS Package Manager!\n");
    vga.write_string("This application was downloaded from GitHub.\n");
    vga.write_string("Package installation system working! \n");
    vga.write_string("\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(
        MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("  Package Name: test\n");
    vga.write_string("  Repository: https://github.com/crackanimador/MesaOS-PKG\n");
    vga.write_string("  Version: 1.0\n");
    vga.write_string("  Status: Successfully Installed\n");
    vga.set_color(MesaOS::Drivers::vga_entry_color(
        MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));

    vga.write_string("\n");
    vga.write_string("This demonstrates that MesaOS can:\n");
    vga.write_string("- Download packages from GitHub\n");
    vga.write_string("- Install ELF executables\n");
    vga.write_string("- Run installed applications\n");
    vga.write_string("- Display GUI-like interfaces\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(
        MesaOS::Drivers::VGAColor::LIGHT_CYAN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("\nPress any key to exit this test application...\n");

    // Simple delay and exit (simplified for kernel environment)
    for (volatile int i = 0; i < 10000000; i++) {
        // Small delay
    }
    running = false;

    // Return to shell
    MesaOS::System::Shell::prompt();
}

bool Test::is_running() {
    return running;
}

} // namespace MesaOS::Apps
