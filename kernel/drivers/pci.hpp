#ifndef PCI_HPP
#define PCI_HPP

#include <stdint.h>

namespace MesaOS::Drivers {

struct PCIDevice {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class_id;
    uint16_t subclass_id;
    uint8_t interrupt_line;
};

class PCIDriver {
public:
    static void initialize();
    static void scan();
    static uint16_t config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    static uint32_t config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    static void config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
    static void config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
    static void enable_bus_mastering(PCIDevice* dev);
    static PCIDevice* get_devices();
    static int get_device_count();

private:
    static PCIDevice devices[32];
    static int device_count;
};

} // namespace MesaOS::Drivers

#endif
