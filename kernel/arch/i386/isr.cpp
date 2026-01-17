#include "isr.hpp"
#include "drivers/vga.hpp"
#include "io_port.hpp"
#include <string.h>
#include "scheduler.hpp"

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
    // Basic handler for now. Later we can look up interrupt_handlers[regs->int_no]
    MesaOS::Drivers::VGADriver vga;
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::RED));
    vga.write_string("\n\n!! CPU EXCEPTION !!\n");
    vga.write_string("Interrupt: ");
    
    char buf[16];
    vga.write_string(itoa(regs->int_no, buf, 10));
    vga.write_string("\nError Code: ");
    vga.write_string(itoa(regs->err_code, buf, 10));
    vga.write_string("\nEIP: 0x");
    vga.write_string(itoa(regs->eip, buf, 16));
    
    vga.write_string("\n\nSystem Halted.");
    
    for(;;);
    return esp;
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
