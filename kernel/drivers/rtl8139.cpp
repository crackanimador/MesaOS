#include "rtl8139.hpp"
#include "arch/i386/io_port.hpp"
#include "memory/kheap.hpp"
#include "drivers/vga.hpp"
#include "net/ethernet.hpp"
#include <string.h>

namespace MesaOS::Drivers {

uint32_t RTL8139::io_base = 0;
uint8_t* RTL8139::rx_buffer = 0;
uint8_t RTL8139::mac_address[6];

void RTL8139::initialize(PCIDevice* dev) {
    MesaOS::Drivers::VGADriver vga;
    
    // Find the I/O base address by scanning BARs
    io_base = 0;
    for (int i = 0; i < 6; i++) {
        uint32_t bar = MesaOS::Drivers::PCIDriver::config_read_dword(dev->bus, dev->device, dev->function, 0x10 + (i * 4));
        if (bar & 0x1) { // Check if it's an I/O BAR
            io_base = bar & ~0x3;
            break;
        }
    }

    if (io_base == 0) {
        vga.write_string("RTL8139: Error - No I/O BAR found!\n");
        return;
    }

    vga.write_string("RTL8139: IO Base at 0x");
    char buf[16];
    vga.write_string(itoa(io_base, buf, 16));
    vga.write_string("\n");

    // Enable PCI Bus Mastering
    uint16_t command = MesaOS::Drivers::PCIDriver::config_read_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2); // Bus Master
    MesaOS::Drivers::PCIDriver::config_write_word(dev->bus, dev->device, dev->function, 0x04, command);
    
    // Power on (Config 1 register)
    MesaOS::Arch::x86::outb(io_base + 0x52, 0x00);

    // Software Reset
    MesaOS::Arch::x86::outb(io_base + 0x37, 0x10);
    int timeout = 10000;
    while((MesaOS::Arch::x86::inb(io_base + 0x37) & 0x10) != 0 && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        vga.write_string("RTL8139: Reset timeout!\n");
        return;
    }

    // Initialize RX buffer
    rx_buffer = (uint8_t*)kmalloc(8192 + 16 + 1500); // 8K + header + max packet
    memset(rx_buffer, 0, 8192 + 16 + 1500);
    MesaOS::Arch::x86::outl(io_base + 0x30, (uint32_t)rx_buffer);

    // Set IMR/ISR (Interrupts)
    MesaOS::Arch::x86::outw(io_base + 0x3C, 0x0005); // ROK and TOK (Receive/Transmit OK)

    // Configure RX (Accept Broadcast, Multicast, My Physical, All)
    MesaOS::Arch::x86::outl(io_base + 0x44, 0x0000000F | (1 << 7)); // WRAP bit

    // Enable RX and TX
    MesaOS::Arch::x86::outb(io_base + 0x37, 0x0C);

    // Read MAC address
    vga.write_string("MAC: ");
    for (int i = 0; i < 6; i++) {
        mac_address[i] = MesaOS::Arch::x86::inb(io_base + i);
        vga.write_string(itoa(mac_address[i], buf, 16));
        if (i < 5) vga.write_string(":");
    }
    vga.write_string("\n");

    // Register IRQ handler
    MesaOS::Arch::x86::register_irq_handler(32 + dev->interrupt_line, RTL8139::callback);
}

void RTL8139::callback(MesaOS::Arch::x86::Registers* regs) {
    (void)regs;
    uint16_t status = MesaOS::Arch::x86::inw(io_base + 0x3E);
    MesaOS::Arch::x86::outw(io_base + 0x3E, status); 

    if (status & 0x01) { // Receive OK
        receive_packet();
    }
}

static uint32_t rx_offset = 0;

void RTL8139::receive_packet() {
    while ((MesaOS::Arch::x86::inb(io_base + 0x37) & 0x01) == 0) {
        uint16_t* header = (uint16_t*)(rx_buffer + rx_offset);
        uint16_t status = header[0];
        uint16_t length = header[1];
        
        // Basic RTL8139 packet validation
        if (!(status & 0x01) || length > 1792 || length < 4) {
            // Error! Reset everything.
            MesaOS::Arch::x86::outb(io_base + 0x37, 0x04);
            MesaOS::Arch::x86::outb(io_base + 0x37, 0x0C);
            rx_offset = 0;
            return;
        }
        
        uint8_t* packet = rx_buffer + rx_offset + 4;
        MesaOS::Net::Ethernet::handle_packet(packet, length - 4);

        rx_offset = (rx_offset + length + 4 + 3) & ~3;
        if (rx_offset >= 8192) rx_offset -= 8192;
        MesaOS::Arch::x86::outw(io_base + 0x38, (uint16_t)rx_offset - 16);
    }
}

void RTL8139::send_packet(uint8_t* data, uint32_t size) {
    if (io_base == 0) return; // Safety check
    static uint8_t tx_desc = 0;
    MesaOS::Arch::x86::outl(io_base + 0x20 + (tx_desc * 4), (uint32_t)data);
    MesaOS::Arch::x86::outl(io_base + 0x10 + (tx_desc * 4), size);
    tx_desc = (tx_desc + 1) % 4;
}

} // namespace MesaOS::Drivers
