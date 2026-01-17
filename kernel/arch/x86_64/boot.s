/* 
MesaOS 64-bit Boot Trampoline 
This code starts in 32-bit Protected Mode (Multiboot) and jumps to 64-bit Long Mode.
*/

.set MAGIC,    0x1BADB002
.set FLAGS,    (1 << 0) | (1 << 1)
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096
stack_bottom:
.skip 16384
stack_top:

.section .rodata
.align 8
gdt64:
    .quad 0x0000000000000000          # Null descriptor
    .quad 0x00af9a000000ffff          # 64-bit Code descriptor (exec/read)
    .quad 0x00cf92000000ffff          # 64-bit Data descriptor (read/write)
gdt64_ptr:
    .word . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start64
_start64:
    mov $stack_top, %esp

    # 1. Identity map the first 2MB for the kernel trampoline
    # PML4[0] -> PDPT
    mov $pdpt, %eax
    or $0x3, %eax                     # Present + Writable
    mov %eax, (pml4)

    # PDPT[0] -> PD
    mov $pd, %eax
    or $0x3, %eax
    mov %eax, (pdpt)

    # PD[0] -> 0 (2MB Huge Page)
    mov $0x00000083, %eax             # Present + Writable + Huge
    mov %eax, (pd)

    # 2. Enable PAE
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    # 3. Load PML4
    mov $pml4, %eax
    mov %eax, %cr3

    # 4. Enable Long Mode in EFER MSR
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    # 5. Enable Paging
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    # 6. Load 64-bit GDT
    lgdt gdt64_ptr

    # 7. Long jump to 64-bit code
    ljmp $0x08, $_start64_longmode

.code64
_start64_longmode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Call 64-bit entry point (to be implemented)
    # call kernel_main64
    
    cli
1:  hlt
    jmp 1b
