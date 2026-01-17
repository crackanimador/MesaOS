#ifndef PCNET_HPP
#define PCNET_HPP

#include "drivers/pci.hpp"
#include "arch/i386/isr.hpp"
#include <stdint.h>

namespace MesaOS::Drivers {

class PCNet {
public:
    static void initialize(PCIDevice* dev);
    static void send_packet(uint8_t* data, uint32_t size);
    static void callback(MesaOS::Arch::x86::Registers* regs);
    static uint8_t* get_mac_address();
    static void poll(); // Manual polling for packets

private:
    static void write_csr(uint32_t index, uint32_t data);
    static uint32_t read_csr(uint32_t index);
    static void write_bcr(uint32_t index, uint32_t data);
    static uint32_t read_bcr(uint32_t index);
    
    static void receive_packet();
};

} // namespace MesaOS::Drivers

#endif
