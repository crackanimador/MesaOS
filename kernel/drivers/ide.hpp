#ifndef IDE_HPP
#define IDE_HPP

#include <stdint.h>

namespace MesaOS::Drivers {

class IDEDriver {
public:
    static void initialize();
    static void read_sector(uint32_t lba, uint8_t* buffer);
    static void write_sector(uint32_t lba, uint8_t* buffer);

private:
    static void wait_bsy();
    static void wait_drq();
};

} // namespace MesaOS::Drivers

#endif
