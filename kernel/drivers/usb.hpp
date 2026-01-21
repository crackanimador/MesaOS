#ifndef USB_HPP
#define USB_HPP

#include <stdint.h>

namespace MesaOS::Drivers {

struct USBDevice {
    uint8_t address;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t max_packet_size;
    uint8_t num_configurations;
};

class USBDriver {
public:
    static void initialize();
    static bool is_present();
    static void enumerate_devices();
    static USBDevice* get_devices();
    static int get_device_count();
    static bool send_control_packet(uint8_t device_addr, uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index, uint8_t* data, uint16_t length);
    static bool send_bulk_packet(uint8_t device_addr, uint8_t endpoint, uint8_t* data, uint16_t length);
    static uint16_t receive_bulk_packet(uint8_t device_addr, uint8_t endpoint, uint8_t* buffer, uint16_t max_length);

private:
    static USBDevice devices[16];
    static int device_count;
    static bool initialized;
};

} // namespace MesaOS::Drivers

#endif
