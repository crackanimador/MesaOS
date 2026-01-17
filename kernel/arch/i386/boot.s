/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* 
Multiboot header as per spec. 
*/
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/*
The stack for the kernel.
*/
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

/*
Entry point.
*/
.section .text
.global _start
.type _start, @function
_start:
	# Set up the stack.
	mov $stack_top, %esp

	# Call the global constructors.
	call _init

	# Push the multiboot parameters
	push %ebx
	push %eax

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main returns.
	cli
1:	hlt
	jmp 1b

.size _start, . - _start
