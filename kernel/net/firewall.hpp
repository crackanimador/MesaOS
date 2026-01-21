#ifndef FIREWALL_HPP
#define FIREWALL_HPP

#include <stdint.h>

namespace MesaOS::Net {

#define MAX_FIREWALL_RULES 32

enum FirewallAction {
    ALLOW,
    DENY
};

struct FirewallRule {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol; // 6=TCP, 17=UDP, 1=ICMP
    FirewallAction action;
    bool enabled;
};

class Firewall {
public:
    static void initialize();
    static bool add_rule(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port,
                        uint16_t dst_port, uint8_t protocol, FirewallAction action);
    static bool check_packet(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port,
                           uint16_t dst_port, uint8_t protocol);
    static void enable_rule(uint32_t index);
    static void disable_rule(uint32_t index);

private:
    static FirewallRule rules[MAX_FIREWALL_RULES];
    static uint32_t rule_count;
};

} // namespace MesaOS::Net

#endif
