#include "rtc.hpp"
#include "arch/i386/io_port.hpp"

namespace MesaOS::Drivers {

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

int RTC::get_update_in_progress_flag() {
    MesaOS::Arch::x86::outb(CMOS_ADDRESS, 0x0A);
    return (MesaOS::Arch::x86::inb(CMOS_DATA) & 0x80);
}

uint8_t RTC::read_register(int reg) {
    MesaOS::Arch::x86::outb(CMOS_ADDRESS, reg);
    return MesaOS::Arch::x86::inb(CMOS_DATA);
}

Time RTC::get_time() {
    while (get_update_in_progress_flag());

    Time time;
    time.second = read_register(0x00);
    time.minute = read_register(0x02);
    time.hour = read_register(0x04);
    time.day = read_register(0x07);
    time.month = read_register(0x08);
    time.year = read_register(0x09);

    uint8_t registerB = read_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        time.second = (time.second & 0x0F) + ((time.second / 16) * 10);
        time.minute = (time.minute & 0x0F) + ((time.minute / 16) * 10);
        time.hour = ((time.hour & 0x0F) + (((time.hour & 0x70) / 16) * 10)) | (time.hour & 0x80);
        time.day = (time.day & 0x0F) + ((time.day / 16) * 10);
        time.month = (time.month & 0x0F) + ((time.month / 16) * 10);
        time.year = (time.year & 0x0F) + ((time.year / 16) * 10);
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    if (!(registerB & 0x02) && (time.hour & 0x80)) {
        time.hour = ((time.hour & 0x7F) + 12) % 24;
    }

    // Assuming we're in the 21st century
    time.year += 2000;

    return time;
}

} // namespace MesaOS::Drivers
