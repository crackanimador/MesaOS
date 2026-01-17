#ifndef PAGING_HPP
#define PAGING_HPP

#include <stdint.h>

namespace MesaOS::Memory {

class Paging {
public:
    static void initialize();
    static void switch_page_directory(uint32_t* directory);
    static void map_page(uint32_t virtual_addr, uint32_t physical_addr, bool user, bool rw);

private:
    static uint32_t* page_directory;
};

} // namespace MesaOS::Memory

#endif
