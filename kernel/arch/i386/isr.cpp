#include "isr.hpp"
#include "drivers/vga.hpp"
#include "io_port.hpp"
#include <string.h>
#include "scheduler.hpp"
#include "signals.hpp"
#include "memory/vmm.hpp"
#include "memory/pmm.hpp"
#include "logging.hpp"
#include "panic.hpp"

namespace MesaOS::Arch::x86 {

ISRHandler interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, ISRHandler handler) {
    interrupt_handlers[n] = handler;
}

void register_irq_handler(uint8_t n, ISRHandler handler) {
    interrupt_handlers[n] = handler;
}

} // namespace MesaOS::Arch::x86

extern "C" uint32_t isr_handler(uint32_t esp) {
    MesaOS::Arch::x86::Registers* regs = (MesaOS::Arch::x86::Registers*)esp;

    // Handle syscalls (INT 0x80 = 128)
    if (regs->int_no == 128) {
        if (MesaOS::Arch::x86::interrupt_handlers[128] != 0) {
            MesaOS::Arch::x86::ISRHandler handler = MesaOS::Arch::x86::interrupt_handlers[128];
            handler(regs);
        }
        return esp;
    }

    // Handle page faults (interrupt 14)
    if (regs->int_no == 14) {
        // Page fault handler
        uint32_t faulting_address;
        asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

        // Check error code bits
        bool present = regs->err_code & 0x1;        // Page present bit
        bool rw = regs->err_code & 0x2;            // Read/write bit
        bool user = regs->err_code & 0x4;          // User/supervisor bit
        bool reserved = regs->err_code & 0x8;      // Reserved bit
        bool instruction = regs->err_code & 0x10;  // Instruction fetch bit

        // Handle Copy-on-Write (COW) page faults
        if (present && rw) {
            // Write to a present page - likely COW
            uint32_t dir_idx = faulting_address >> 22;
            uint32_t table_idx = (faulting_address >> 12) & 0x03FF;

            // Get current page directory
            uint32_t* current_dir;
            asm volatile("mov %%cr3, %0" : "=r"(current_dir));

            if (current_dir && (dir_idx < 1024) && (current_dir[dir_idx] & 0x1)) {
                uint32_t* table = (uint32_t*)(current_dir[dir_idx] & ~0xFFF);
                if (table[table_idx] & 0x1) {
                    uint32_t flags = table[table_idx] & 0xFFF;
                    // Check if this is a COW page (bit 9 set)
                    if (flags & (1 << 9)) {
                        // COW page fault - allocate new page and copy data
                        void* new_page = MesaOS::Memory::PMM::allocate_block();
                        if (new_page) {
                            // Copy data from original page
                            uint32_t original_phys = table[table_idx] & ~0xFFF;
                            memcpy(new_page, (void*)original_phys, 4096);

                            // Update page table entry: clear COW flag, set writable
                            table[table_idx] = ((uint32_t)new_page) | (flags & ~(1 << 9)) | 0x2;

                            // Invalidate TLB
                            asm volatile("invlpg (%0)" : : "r"(faulting_address) : "memory");

                            MesaOS::System::Logging::info("COW page fault handled");
                            return esp; // Resume execution
                        }
                    }
                }
            }
        }

        // If not handled as COW, panic
        char panic_msg[128] = "Page fault at address 0x";
        char addr_buf[16];
        itoa(faulting_address, addr_buf, 16);
        strcat(panic_msg, addr_buf);
        strcat(panic_msg, " - ");

        if (!present) strcat(panic_msg, "Page not present ");
        if (rw) strcat(panic_msg, "Write access ");
        else strcat(panic_msg, "Read access ");
        if (user) strcat(panic_msg, "User mode ");
        else strcat(panic_msg, "Kernel mode ");
        if (reserved) strcat(panic_msg, "Reserved bit set ");
        if (instruction) strcat(panic_msg, "Instruction fetch");

        MesaOS::System::KernelPanic::panic(panic_msg, regs);
        return esp; // This won't be reached, but keeps compiler happy
    }

    // Basic handler for other exceptions - panic with details
    char panic_msg[64] = "CPU Exception - Interrupt ";
    char buf[16];
    itoa(regs->int_no, buf, 10);
    strcat(panic_msg, buf);
    strcat(panic_msg, " (Error code: ");
    itoa(regs->err_code, buf, 10);
    strcat(panic_msg, buf);
    strcat(panic_msg, ")");

    MesaOS::System::KernelPanic::panic(panic_msg, regs);
    return esp; // This won't be reached, but keeps compiler happy
}

extern "C" uint32_t irq_handler(uint32_t esp) {
    MesaOS::Arch::x86::Registers* regs = (MesaOS::Arch::x86::Registers*)esp;
    // Send EOI (End of Interrupt) to the PICs
    if (regs->int_no >= 40) {
        MesaOS::Arch::x86::outb(0xA0, 0x20); // Signal slave PIC
    }
    MesaOS::Arch::x86::outb(0x20, 0x20);     // Signal master PIC

    if (MesaOS::Arch::x86::interrupt_handlers[regs->int_no] != 0) {
        MesaOS::Arch::x86::ISRHandler handler = MesaOS::Arch::x86::interrupt_handlers[regs->int_no];
        handler(regs);
    }
    
    // For IRQ0 (Timer), we might want to return a DIFFERENT stack pointer
    if (regs->int_no == 32) {
        return MesaOS::System::Scheduler::schedule(esp);
    }

    return esp;
}
