#include "pcnet.hpp"
#include "arch/i386/io_port.hpp"
#include "drivers/vga.hpp"
#include "memory/kheap.hpp"
#include "net/ethernet.hpp"
#include <string.h>

namespace MesaOS::Drivers {

static uint32_t io_base = 0;
static uint8_t mac_address[6];

struct PCNetDescriptor {
    uint32_t address;
    uint32_t status;
    uint32_t status2;
    uint32_t res;
} __attribute__((packed));

struct PCNetInitBlock {
    uint16_t mode;
    uint8_t rlen;
    uint8_t tlen;
    uint8_t mac[6];
    uint16_t reserved;
    uint32_t ladrf[2];
    uint32_t rx_ring;
    uint32_t tx_ring;
} __attribute__((packed));

static PCNetDescriptor* rx_ring;
static PCNetDescriptor* tx_ring;
static uint8_t* rx_buffers;
static uint8_t* tx_buffers;
static int rx_index = 0;
static int tx_index = 0;

void PCNet::write_csr(uint32_t index, uint32_t data) {
    MesaOS::Arch::x86::outl(io_base + 0x14, index);
    MesaOS::Arch::x86::outl(io_base + 0x10, data);
}

uint32_t PCNet::read_csr(uint32_t index) {
    MesaOS::Arch::x86::outl(io_base + 0x14, index);
    return MesaOS::Arch::x86::inl(io_base + 0x10);
}

void PCNet::write_bcr(uint32_t index, uint32_t data) {
    MesaOS::Arch::x86::outl(io_base + 0x14, index);
    MesaOS::Arch::x86::outl(io_base + 0x1C, data);
}

uint32_t PCNet::read_bcr(uint32_t index) {
    MesaOS::Arch::x86::outl(io_base + 0x14, index);
    return MesaOS::Arch::x86::inl(io_base + 0x1C);
}

void PCNet::initialize(PCIDevice* dev) {
    MesaOS::Drivers::VGADriver vga;
    char buf[16];

    // Find I/O base (usually BAR0)
    io_base = 0;
    for (int i = 0; i < 6; i++) {
        uint32_t bar = PCIDriver::config_read_dword(dev->bus, dev->device, dev->function, 0x10 + (i * 4));
        if (bar & 1) {
            io_base = bar & ~0x3;
            break;
        }
    }

    if (io_base == 0) return;

    vga.write_string("PCNet: Initializing at 0x");
    vga.write_string(itoa(io_base, buf, 16));
    vga.write_string("\n");

    // 1. Reset device (read from 0x14 then 0x10 is reset in 32-bit mode)
    // Actually, just reading from the reset ports
    MesaOS::Arch::x86::inl(io_base + 0x18); // Reset

    // 2. Set to 32-bit mode (DWIO) and SWSTYLE
    MesaOS::Arch::x86::outl(io_base + 0x10, 0); // Force 32-bit access trigger
    
    // Important: Set BCR 20 to 2 (32-bit software style)
    write_bcr(20, 2);
    
    // 3. Read MAC from PROM (first 6 bytes)
    vga.write_string("MAC: ");
    for (int i = 0; i < 6; i++) {
        mac_address[i] = MesaOS::Arch::x86::inb(io_base + i);
        vga.write_string(itoa(mac_address[i], buf, 16));
        if (i < 5) vga.write_string(":");
    }
    vga.write_string("\n");

    // 4. Register IRQ
    MesaOS::Arch::x86::register_irq_handler(32 + dev->interrupt_line, PCNet::callback);
    
    // 4b. Enable Bus Mastering (Critical for DMA)
    PCIDriver::enable_bus_mastering(dev);

    // 5. Setup Rings (Must be 16-byte aligned)
    void* rx_ring_raw = kmalloc(sizeof(PCNetDescriptor) * 32 + 16);
    rx_ring = (PCNetDescriptor*)(((uint32_t)rx_ring_raw + 15) & ~0xF);

    void* tx_ring_raw = kmalloc(sizeof(PCNetDescriptor) * 8 + 16);
    tx_ring = (PCNetDescriptor*)(((uint32_t)tx_ring_raw + 15) & ~0xF);
    
    rx_buffers = (uint8_t*)kmalloc(2048 * 32);
    tx_buffers = (uint8_t*)kmalloc(2048 * 8);

    for (int i = 0; i < 32; i++) {
        rx_ring[i].address = (uint32_t)rx_buffers + (i * 2048);
        rx_ring[i].status = 0x80000000 | (uint16_t)(-1536); // OWN bit + size (-1536)
        rx_ring[i].status2 = 0;
        rx_ring[i].res = 0;
    }

    for (int i = 0; i < 8; i++) {
        tx_ring[i].address = (uint32_t)tx_buffers + (i * 2048);
        tx_ring[i].status = 0;
        tx_ring[i].status2 = 0;
        tx_ring[i].res = 0;
    }

    // 6. Setup Init Block (Must be 16-byte aligned preferably, definitely 2-byte)
    void* init_block_raw = kmalloc(sizeof(PCNetInitBlock) + 16);
    PCNetInitBlock* init_block = (PCNetInitBlock*)(((uint32_t)init_block_raw + 15) & ~0xF);
    memset(init_block, 0, sizeof(PCNetInitBlock));
    init_block->mode = 0x8000; // Promiscuous Mode (Accept ALL packets, simplify debugging)
    init_block->rlen = (5 << 4); // 32 descriptors
    init_block->tlen = (3 << 4); // 8 descriptors
    memcpy(init_block->mac, mac_address, 6);
    init_block->rx_ring = (uint32_t)rx_ring;
    init_block->tx_ring = (uint32_t)tx_ring;

    // 7. Write Init Block to CSR
    write_csr(1, (uint32_t)init_block & 0xFFFF);
    write_csr(2, (uint32_t)init_block >> 16);

    // 8. Initialize and Start
    write_csr(0, 0x01); // INIT bit
    while (!(read_csr(0) & 0x0100)); // Wait for IDON (Init Done)
    
    write_csr(0, 0x02 | 0x40); // STRT (Start) | IENA (Interrupt Enable)

    vga.write_string("PCNet: Network UP\n");
}

void PCNet::send_packet(uint8_t* data, uint32_t size) {
    if (size > 2000) return;
    
    memcpy(tx_buffers + (tx_index * 2048), data, size);
    tx_ring[tx_index].status2 = 0;
    tx_ring[tx_index].status = 0x83000000 | (uint16_t)(-size); // OWN + STP + ENP
    
    write_csr(0, 0x48); // Set TDMD (Poll Transmit)
    tx_index = (tx_index + 1) % 8;
}

void PCNet::callback(MesaOS::Arch::x86::Registers* regs) {
    (void)regs;
    uint32_t csr0 = read_csr(0);
    
    if (csr0 & 0x0400) { // RINT (Receive Interrupt)
        receive_packet();
    }
    
    write_csr(0, csr0 & ~0x0040); // Ack interrupts
}

void PCNet::receive_packet() {
    // Loop until we find a buffer owned by the card (0x80000000 set means card owns it)
    // So we process while !(status & 0x80000000)
    int count = 0;
    while (!(rx_ring[rx_index].status & 0x80000000)) {
        uint32_t length = rx_ring[rx_index].status2 & 0x0FFF; // Message Byte Count
        
        // Debug every packet
        // MesaOS::Drivers::VGADriver vga;
        // char b[16]; vga.write_string("[RX "); vga.write_string(itoa(length, b, 10)); vga.write_string("] ");

        MesaOS::Net::Ethernet::handle_packet(rx_buffers + (rx_index * 2048), length);
        
        // Hand back to card
        rx_ring[rx_index].status2 = 0;
        rx_ring[rx_index].status = 0x80000000 | (uint16_t)(-1536);
        rx_index = (rx_index + 1) % 32;
        
        count++;
        if (count > 32) break; // Safety break
    }
}

uint8_t* PCNet::get_mac_address() {
    return mac_address;
}

void PCNet::poll() {
    receive_packet();
}

} // namespace MesaOS::Drivers
