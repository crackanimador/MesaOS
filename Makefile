CC = i686-elf-gcc
CXX = i686-elf-g++
AS = i686-elf-as

CFLAGS = -ffreestanding -O2 -Wall -Wextra
CXXFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -ffreestanding -O2 -nostdlib -lgcc

KERNEL_OBJS = \
	kernel/arch/i386/boot.o \
	kernel/arch/i386/gdt.o \
	kernel/arch/i386/gdt_flush.o \
	kernel/arch/i386/idt.o \
	kernel/arch/i386/interrupts.o \
	kernel/arch/i386/isr.o \
	kernel/libc/string.o \
	kernel/drivers/keyboard.o \
	kernel/drivers/ide.o \
	kernel/drivers/pci.o \
	kernel/drivers/rtl8139.o \
	kernel/drivers/pcnet.o \
	kernel/drivers/pit.o \
	kernel/drivers/rtc.o \
	kernel/memory/pmm.o \
	kernel/memory/kheap.o \
	kernel/memory/paging.o \
	kernel/fs/vfs.o \
	kernel/fs/ramfs.o \
	kernel/fs/mbr.o \
	kernel/fs/mesafs.o \
	kernel/apps/nano.o \
	kernel/scheduler.o \
	kernel/shell.o \
	kernel/kernel.o \
	kernel/drivers/vga.o \
	kernel/net/ethernet.o \
	kernel/net/arp.o \
	kernel/net/ipv4.o \
	kernel/net/udp.o \
	kernel/net/dhcp.o \
	kernel/net/icmp.o

MESAOS_BIN = MesaOS.bin
MESAOS_ISO = MesaOS.iso

all: $(MESAOS_BIN)

%.o: %.s
	$(AS) $< -o $@

%.o: %.cpp
	$(CXX) -c $< -o $@ -Ikernel/include -Ikernel $(CXXFLAGS)

$(MESAOS_BIN): $(KERNEL_OBJS)
	$(CXX) -T kernel/linker.ld -o $@ $(LDFLAGS) $(KERNEL_OBJS)

iso: $(MESAOS_BIN)
	mkdir -p isodir/boot/grub
	cp $(MESAOS_BIN) isodir/boot/$(MESAOS_BIN)
	echo 'menuentry "MesaOS" {' > isodir/boot/grub/grub.cfg
	echo '  multiboot /boot/MesaOS.bin' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(MESAOS_ISO) isodir

clean:
	rm -rf $(KERNEL_OBJS) $(MESAOS_BIN) $(MESAOS_ISO) isodir

qemu: iso
	qemu-system-i386 -boot d -cdrom $(MESAOS_ISO) -hda disk.img -net nic,model=rtl8139 -net user

.PHONY: all clean iso qemu
