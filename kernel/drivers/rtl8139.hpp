#ifndef RTL8139_HPP
#define RTL8139_HPP

#include <stdint.h>
#include "drivers/pci.hpp"
#include "arch/i386/isr.hpp"

namespace MesaOS::Drivers {

class RTL8139 {
public:
    static void initialize(PCIDevice* dev);
    static void send_packet(uint8_t* data, uint32_t size);
    static void receive_packet();
    static uint8_t* get_mac_address() { return mac_address; }
    
    static void callback(MesaOS::Arch::x86::Registers* regs);

private:
    static uint32_t io_base;
    static uint8_t* rx_buffer;
    static uint8_t mac_address[6];
};

} // namespace MesaOS::Drivers

#endif
