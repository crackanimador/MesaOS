#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <stdint.h>
#include "arch/i386/isr.hpp"

namespace MesaOS::System {

enum ProcessState {
    READY,
    RUNNING,
    SUSPENDED,
    TERMINATED
};

struct Process {
    uint32_t pid;
    char name[32];
    ProcessState state;
    MesaOS::Arch::x86::Registers regs;
    uint32_t stack_ptr;
    struct Process* next;
};

class Scheduler {
public:
    static void initialize();
    static void add_process(const char* name, void (*entry_point)());
    static uint32_t schedule(uint32_t current_stack);
    static Process* get_current();
    static Process* get_process_list();

private:
    static Process* process_list;
    static Process* current_process;
    static uint32_t next_pid;
};

} // namespace MesaOS::System

#endif
