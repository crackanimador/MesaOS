#include "ide.hpp"
#include "arch/i386/io_port.hpp"

namespace MesaOS::Drivers {

#define STATUS_BSY 0x80
#define STATUS_DRQ 0x08

void IDEDriver::initialize() {
    // Basic ATA initialization: identify drive 0
    MesaOS::Arch::x86::outb(0x1F6, 0xA0);
}

void IDEDriver::wait_bsy() {
    while (MesaOS::Arch::x86::inb(0x1F7) & STATUS_BSY);
}

void IDEDriver::wait_drq() {
    while (!(MesaOS::Arch::x86::inb(0x1F7) & STATUS_DRQ));
}

void IDEDriver::read_sector(uint32_t lba, uint8_t* buffer) {
    wait_bsy();
    MesaOS::Arch::x86::outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    MesaOS::Arch::x86::outb(0x1F2, 1);
    MesaOS::Arch::x86::outb(0x1F3, (uint8_t)lba);
    MesaOS::Arch::x86::outb(0x1F4, (uint8_t)(lba >> 8));
    MesaOS::Arch::x86::outb(0x1F5, (uint8_t)(lba >> 16));
    MesaOS::Arch::x86::outb(0x1F7, 0x20); // READ SECTORS

    wait_bsy();
    wait_drq();

    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = MesaOS::Arch::x86::inw(0x1F0);
    }
}

void IDEDriver::write_sector(uint32_t lba, uint8_t* buffer) {
    wait_bsy();
    MesaOS::Arch::x86::outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    MesaOS::Arch::x86::outb(0x1F2, 1);
    MesaOS::Arch::x86::outb(0x1F3, (uint8_t)lba);
    MesaOS::Arch::x86::outb(0x1F4, (uint8_t)(lba >> 8));
    MesaOS::Arch::x86::outb(0x1F5, (uint8_t)(lba >> 16));
    MesaOS::Arch::x86::outb(0x1F7, 0x30); // WRITE SECTORS

    wait_bsy();
    wait_drq();

    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        MesaOS::Arch::x86::outw(0x1F0, ptr[i]);
    }
}

} // namespace MesaOS::Drivers
