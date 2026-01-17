#include "pci.hpp"
#include "arch/i386/io_port.hpp"
#include "drivers/vga.hpp"

namespace MesaOS::Drivers {

PCIDevice PCIDriver::devices[32];
int PCIDriver::device_count = 0;

uint16_t PCIDriver::config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
 
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
 
    MesaOS::Arch::x86::outl(0xCF8, address);
    return (uint16_t)((MesaOS::Arch::x86::inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
}

uint32_t PCIDriver::config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | (((uint32_t)slot) << 11) |
              (((uint32_t)func) << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    MesaOS::Arch::x86::outl(0xCF8, address);
    return MesaOS::Arch::x86::inl(0xCFC);
}

void PCIDriver::config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | (((uint32_t)slot) << 11) |
              (((uint32_t)func) << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    MesaOS::Arch::x86::outl(0xCF8, address);
    MesaOS::Arch::x86::outl(0xCFC, (MesaOS::Arch::x86::inl(0xCFC) & ~(0xffff << ((offset & 2) * 8))) | ((uint32_t)value << ((offset & 2) * 8)));
}

void PCIDriver::config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | (((uint32_t)slot) << 11) |
              (((uint32_t)func) << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    MesaOS::Arch::x86::outl(0xCF8, address);
    MesaOS::Arch::x86::outl(0xCFC, value);
}

void PCIDriver::enable_bus_mastering(PCIDevice* dev) {
    uint16_t command = config_read_word(dev->bus, dev->device, dev->function, 0x04);
    if (!(command & 0x0004)) {
        command |= 0x0004; // Set Bus Master bit
        config_write_word(dev->bus, dev->device, dev->function, 0x04, command);
        
        // Debug
        MesaOS::Drivers::VGADriver vga;
        vga.write_string("[PCI] Enabled Bus Mastering for device.\n");
    }
}

// Fixed config_read_word - inb/outb isn't enough for 32-bit PCI address, but we have outb.
// Actually outl/inl would be better for 32 bit. Let's add them to io_port.hpp if needed.
// Wait, my io_port only has outb/inb/outw/inw. I'll add outl/inl.

void PCIDriver::scan() {
    device_count = 0;
    // Limit to bus 0-3 for performance, most VMs only use bus 0
    for (int bus = 0; bus < 4; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t vendor = config_read_word(bus, slot, func, 0);
                if (vendor == 0xFFFF) {
                    if (func == 0) break; // Slot empty, skip other functions
                    continue;
                }
                
                uint16_t device = config_read_word(bus, slot, func, 2);
                uint16_t class_info = config_read_word(bus, slot, func, 10);
                
                if (device_count < 32) {
                    devices[device_count].bus = bus;
                    devices[device_count].device = slot;
                    devices[device_count].function = func;
                    devices[device_count].vendor_id = vendor;
                    devices[device_count].device_id = device;
                    devices[device_count].class_id = class_info >> 8;
                    devices[device_count].subclass_id = class_info & 0xFF;
                    
                    uint16_t int_info = config_read_word(bus, slot, func, 0x3C);
                    devices[device_count].interrupt_line = int_info & 0xFF;

                    device_count++;
                }

                // If not multi-function, don't check other functions
                if (func == 0) {
                    uint16_t header_type = config_read_word(bus, slot, func, 14) & 0xFF;
                    if (!(header_type & 0x80)) break;
                }
            }
        }
    }
}

PCIDevice* PCIDriver::get_devices() { return devices; }
int PCIDriver::get_device_count() { return device_count; }

} // namespace MesaOS::Drivers
