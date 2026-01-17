#include "drivers/vga.hpp"
#include "arch/i386/gdt.hpp"
#include "arch/i386/idt.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/pit.hpp"
#include "shell.hpp"
#include "multiboot.h"
#include "memory/pmm.hpp"
#include "memory/kheap.hpp"
#include <string.h>
#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"
#include "fs/mbr.hpp"
#include "fs/mesafs.hpp"
#include "memory/paging.hpp"
#include "drivers/pci.hpp"
#include "drivers/rtl8139.hpp"
#include "drivers/pcnet.hpp"
#include "scheduler.hpp"

extern uint32_t kernel_end;

void shell_entry() {
    MesaOS::System::Shell::initialize();
    for(;;);
}

extern "C" void kernel_main(uint32_t magic, multiboot_info* mbt) {
    (void)magic;
    (void)mbt;
    MesaOS::Drivers::VGADriver vga;
    vga.initialize();

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_CYAN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Welcome to MesaOS v0.3 'Persist'\n\n");
    
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing GDT... ");
    MesaOS::Arch::x86::GDT::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing IDT... ");
    MesaOS::Arch::x86::IDT::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing PMM... ");
    // Reserve roughly 128MB or total memory
    uint32_t mem_size = (mbt->mem_lower + mbt->mem_upper) * 1024;
    MesaOS::Memory::PMM::initialize((uint32_t)&kernel_end, mem_size);
    MesaOS::Memory::PMM::load_memory_map(mbt);
    
    // Lock the kernel memory blocks
    for (uint32_t i = 0x100000; i < (uint32_t)&kernel_end; i += 4096) {
        MesaOS::Memory::PMM::set_block(i / 4096);
    }
    // Lock the paging structures area if needed (but paging hasn't started yet)
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing KHeap... ");
    // Allocate 4MB for the kernel heap after the kernel end and PMM bitmap
    // PMM bitmap size is mem_size / 32768 bytes. For 128MB it's ~4KB.
    uint32_t heap_start = (uint32_t)&kernel_end + (mem_size / 32 / 8) + 4096;
    MesaOS::Memory::KHeap::initialize(heap_start, 4 * 1024 * 1024);
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Paging... ");
    MesaOS::Memory::Paging::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Scheduler... ");
    MesaOS::System::Scheduler::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Keyboard... ");
    MesaOS::Drivers::Keyboard::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Timer... ");
    MesaOS::Drivers::PIT::initialize(100); // 100Hz
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Enabling Interrupts... ");
    asm volatile("sti");
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing RAMFS... ");
    MesaOS::FS::fs_root = MesaOS::FS::RAMFS::initialize();
    MesaOS::FS::RAMFS::create_file("readme.txt", "Welcome to MesaOS Filesystem!");
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Disk Drive... ");
    MesaOS::FS::PartitionManager::initialize();
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Mounting MesaFS on /disk... ");
    // Mounting on partition 1 (LBA 2048)
    MesaOS::FS::fs_node* disk_node = MesaOS::FS::MesaFS::initialize(2048);
    MesaOS::FS::RAMFS::mount(disk_node);
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Initializing Networking... ");
    MesaOS::Drivers::PCIDriver::scan();
    int net_found = 0;
    char b[16];
    for(int i = 0; i < MesaOS::Drivers::PCIDriver::get_device_count(); i++) {
        MesaOS::Drivers::PCIDevice& dev = MesaOS::Drivers::PCIDriver::get_devices()[i];
        if (dev.vendor_id == 0x10EC && dev.device_id == 0x8139) {
            vga.write_string("Found RTL8139 at ");
            vga.write_string(itoa(dev.bus, b, 16)); vga.write_string(":");
            vga.write_string(itoa(dev.device, b, 16)); vga.write_string(" ");
            MesaOS::Drivers::RTL8139::initialize(&dev);
            net_found = 1;
            break;
        } else if (dev.vendor_id == 0x1022 && dev.device_id == 0x2000) {
            vga.write_string("Found PCNet at ");
            vga.write_string(itoa(dev.bus, b, 16)); vga.write_string(":");
            vga.write_string(itoa(dev.device, b, 16)); vga.write_string(" ");
            MesaOS::Drivers::PCNet::initialize(&dev);
            net_found = 1;
            break;
        }
    }
    if (!net_found) {
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_BROWN, MesaOS::Drivers::VGAColor::BLACK));
        vga.write_string("No NIC found ");
    }
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("OK\n\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREY, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("System Status: ");
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREEN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("READY\n");
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("------------------------------------------\n");
    vga.write_string("  MesaOS v0.3 - Hybrid RAM/Disk Kernel\n");
    vga.write_string("------------------------------------------\n");

    MesaOS::System::Scheduler::add_process("shell", shell_entry);

    for(;;) {
        asm volatile("hlt");
    }
}

// Minimal CRT for global constructors (not strictly needed yet but good practice)
extern "C" void _init() {}
