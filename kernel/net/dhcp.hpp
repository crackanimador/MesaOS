#ifndef DHCP_HPP
#define DHCP_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct DHCPMessage {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie;
    uint8_t options[312];
} __attribute__((packed));

class DHCP {
public:
    static void discover();
    static void handle_packet(uint8_t* data, uint32_t size);

private:
    static uint32_t transaction_id;
};

} // namespace MesaOS::Net

#endif
