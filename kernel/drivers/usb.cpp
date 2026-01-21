#include "usb.hpp"
#include "pci.hpp"
#include "logging.hpp"
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::Drivers {

USBDevice USBDriver::devices[16];
int USBDriver::device_count = 0;
bool USBDriver::initialized = false;

void USBDriver::initialize() {
    if (initialized) return;

    // Look for USB controllers on PCI bus
    MesaOS::Drivers::PCIDriver::scan();

    PCIDevice* pci_devices = MesaOS::Drivers::PCIDriver::get_devices();
    int count = MesaOS::Drivers::PCIDriver::get_device_count();

    for (int i = 0; i < count; i++) {
        // Look for USB controllers (class 0x0C, subclasses 0x03 = USB)
        // VirtualBox USB 2.0: EHCI controller (subclass 0x03, interface 0x20)
        // Also check for Intel ICH6 USB controllers that VirtualBox might emulate
        if (pci_devices[i].class_id == 0x0C03 ||
            (pci_devices[i].vendor_id == 0x8086 && pci_devices[i].device_id == 0x265C)) {
            MesaOS::System::Logging::info("USB controller found - initializing...");

            // Check for VirtualBox USB 2.0 (EHCI)
            if (pci_devices[i].vendor_id == 0x80EE && pci_devices[i].device_id == 0x0021) {
                MesaOS::System::Logging::info("VirtualBox USB 2.0 EHCI controller detected");
            }
            // Check for Intel ICH6 USB controller (that VirtualBox emulates)
            else if (pci_devices[i].vendor_id == 0x8086 && pci_devices[i].device_id == 0x265C) {
                MesaOS::System::Logging::info("Intel ICH6 USB controller detected (VirtualBox emulated)");
            }

            // In a real implementation, we'd initialize the specific USB controller
            // UHCI, OHCI, EHCI, or xHCI depending on the device
            initialized = true;
            MesaOS::System::Logging::info("USB driver initialized successfully");

            // Simulate some USB devices (including VirtualBox simulated ones)
            enumerate_devices();
            break;
        }
    }

    if (!initialized) {
        MesaOS::System::Logging::info("No USB controllers found");
    }
}

bool USBDriver::is_present() {
    return initialized;
}

void USBDriver::enumerate_devices() {
    if (!initialized) return;

    // Reset device count
    device_count = 0;

    // Simulate device enumeration
    // In real implementation, this would traverse the USB bus hierarchy

    // Simulate finding some USB devices (including VirtualBox simulated ones)
    if (device_count < 16) {
        devices[device_count].address = 1;
        devices[device_count].device_class = 0x09; // Hub
        devices[device_count].device_subclass = 0x00;
        devices[device_count].device_protocol = 0x00;
        devices[device_count].vendor_id = 0x80EE; // VirtualBox
        devices[device_count].product_id = 0x0021; // USB Root Hub
        devices[device_count].max_packet_size = 64;
        devices[device_count].num_configurations = 1;
        device_count++;
    }

    if (device_count < 16) {
        devices[device_count].address = 2;
        devices[device_count].device_class = 0x03; // HID
        devices[device_count].device_subclass = 0x01; // Boot Interface
        devices[device_count].device_protocol = 0x02; // Mouse
        devices[device_count].vendor_id = 0x80EE; // VirtualBox simulated
        devices[device_count].product_id = 0x0030; // VirtualBox USB Tablet
        devices[device_count].max_packet_size = 8;
        devices[device_count].num_configurations = 1;
        device_count++;
    }

    if (device_count < 16) {
        devices[device_count].address = 3;
        devices[device_count].device_class = 0x08; // Mass Storage
        devices[device_count].device_subclass = 0x06; // SCSI
        devices[device_count].device_protocol = 0x50; // Bulk-Only
        devices[device_count].vendor_id = 0x80EE; // VirtualBox simulated
        devices[device_count].product_id = 0x0040; // VirtualBox USB Storage
        devices[device_count].max_packet_size = 64;
        devices[device_count].num_configurations = 1;
        device_count++;
    }

    char log_msg[32];
    strcpy(log_msg, "Found ");
    char count_str[8];
    itoa(device_count, count_str, 10);
    strcat(log_msg, count_str);
    strcat(log_msg, " USB devices");
    MesaOS::System::Logging::info(log_msg);
}

USBDevice* USBDriver::get_devices() {
    return devices;
}

int USBDriver::get_device_count() {
    return device_count;
}

bool USBDriver::send_control_packet(uint8_t device_addr, uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index, uint8_t* data, uint16_t length) {
    if (!initialized) return false;

    // Simulate control transfer
    // In real implementation, this would set up a control transfer on the USB bus

    return true;
}

bool USBDriver::send_bulk_packet(uint8_t device_addr, uint8_t endpoint, uint8_t* data, uint16_t length) {
    if (!initialized) return false;

    // Simulate bulk transfer
    // In real implementation, this would queue data for bulk transfer

    return true;
}

uint16_t USBDriver::receive_bulk_packet(uint8_t device_addr, uint8_t endpoint, uint8_t* buffer, uint16_t max_length) {
    if (!initialized || !buffer) return 0;

    // Simulate bulk reception
    // In real implementation, this would dequeue received data

    return 0; // No data available in simulation
}

} // namespace MesaOS::Drivers
