#include "scheduler.hpp"
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::System {

Process* Scheduler::process_list = 0;
Process* Scheduler::current_process = 0;
uint32_t Scheduler::next_pid = 0;

void Scheduler::initialize() {
    process_list = 0;
    current_process = 0;
    next_pid = 0;
    
    // The kernel itself is PID 0
    add_process("kernel", 0); 
    current_process = process_list;
    current_process->state = RUNNING;
}

void Scheduler::add_process(const char* name, void (*entry_point)()) {
    Process* proc = (Process*)kmalloc(sizeof(Process));
    memset(proc, 0, sizeof(Process));
    
    proc->pid = next_pid++;
    strcpy(proc->name, name);
    proc->state = READY;
    
    // Stack allocation
    uint32_t stack_size = 8192;
    proc->stack_ptr = (uint32_t)kmalloc(stack_size) + stack_size;
    
    // Push initial registers to the process's stack
    proc->stack_ptr -= sizeof(MesaOS::Arch::x86::Registers);
    MesaOS::Arch::x86::Registers* regs = (MesaOS::Arch::x86::Registers*)proc->stack_ptr;
    memset(regs, 0, sizeof(MesaOS::Arch::x86::Registers));
    regs->eip = (uint32_t)entry_point;
    regs->cs = 0x08; // Kernel code segment
    regs->ds = 0x10; // Kernel data segment
    regs->eflags = 0x202; // IF (Interrupt Flag) enabled

    // Add to list
    proc->next = process_list;
    process_list = proc;
}

uint32_t Scheduler::schedule(uint32_t current_stack) {
    if (!current_process) return current_stack;

    // Save current stack
    current_process->stack_ptr = current_stack;
    current_process->state = READY;

    // Simple Round Robin
    current_process = current_process->next;
    if (!current_process) current_process = process_list;
    
    current_process->state = RUNNING;
    
    // Return the stack pointer of the new process
    return current_process->stack_ptr;
}

Process* Scheduler::get_current() {
    return current_process;
}

Process* Scheduler::get_process_list() {
    return process_list;
}

} // namespace MesaOS::System
