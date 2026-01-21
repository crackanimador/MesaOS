#include "firewall.hpp"
#include "logging.hpp"
#include <string.h>

namespace MesaOS::Net {

FirewallRule Firewall::rules[MAX_FIREWALL_RULES];
uint32_t Firewall::rule_count = 0;

void Firewall::initialize() {
    memset(rules, 0, sizeof(rules));
    rule_count = 0;

    // Default deny rule
    add_rule(0, 0, 0, 0, 0, DENY);

    MesaOS::System::Logging::info("Firewall initialized with default deny policy");
}

bool Firewall::add_rule(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port,
                       uint16_t dst_port, uint8_t protocol, FirewallAction action) {
    if (rule_count >= MAX_FIREWALL_RULES) {
        return false;
    }

    FirewallRule* rule = &rules[rule_count++];
    rule->src_ip = src_ip;
    rule->dst_ip = dst_ip;
    rule->src_port = src_port;
    rule->dst_port = dst_port;
    rule->protocol = protocol;
    rule->action = action;
    rule->enabled = true;

    MesaOS::System::Logging::info("Firewall rule added");
    return true;
}

bool Firewall::check_packet(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port,
                          uint16_t dst_port, uint8_t protocol) {
    // Check rules in order (first match wins)
    for (uint32_t i = 0; i < rule_count; i++) {
        FirewallRule* rule = &rules[i];
        if (!rule->enabled) continue;

        // Check if rule matches
        bool src_ip_match = (rule->src_ip == 0 || rule->src_ip == src_ip);
        bool dst_ip_match = (rule->dst_ip == 0 || rule->dst_ip == dst_ip);
        bool src_port_match = (rule->src_port == 0 || rule->src_port == src_port);
        bool dst_port_match = (rule->dst_port == 0 || rule->dst_port == dst_port);
        bool protocol_match = (rule->protocol == 0 || rule->protocol == protocol);

        if (src_ip_match && dst_ip_match && src_port_match && dst_port_match && protocol_match) {
            // Rule matches
            if (rule->action == ALLOW) {
                MesaOS::System::Logging::debug("Firewall: packet allowed");
                return true;
            } else {
                MesaOS::System::Logging::warn("Firewall: packet blocked");
                return false;
            }
        }
    }

    // Default deny
    MesaOS::System::Logging::warn("Firewall: packet blocked by default policy");
    return false;
}

void Firewall::enable_rule(uint32_t index) {
    if (index < rule_count) {
        rules[index].enabled = true;
    }
}

void Firewall::disable_rule(uint32_t index) {
    if (index < rule_count) {
        rules[index].enabled = false;
    }
}

} // namespace MesaOS::Net
