#ifndef RTC_HPP
#define RTC_HPP

#include <stdint.h>

namespace MesaOS::Drivers {

struct Time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

class RTC {
public:
    static Time get_time();

private:
    static uint8_t read_register(int reg);
    static int get_update_in_progress_flag();
};

} // namespace MesaOS::Drivers

#endif
