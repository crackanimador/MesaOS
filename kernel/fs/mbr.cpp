#include "mbr.hpp"
#include "drivers/ide.hpp"
#include "drivers/vga.hpp"
#include <string.h>

namespace MesaOS::FS {

void PartitionManager::initialize() {
    MesaOS::Drivers::IDEDriver::initialize();
}

void PartitionManager::list_partitions() {
    MesaOS::Drivers::VGADriver vga;
    uint8_t sector[512];
    MesaOS::Drivers::IDEDriver::read_sector(0, sector);

    MBR* mbr = (MBR*)sector;
    if (mbr->signature != 0xAA55) {
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_RED, MesaOS::Drivers::VGAColor::BLACK));
        vga.write_string("Error: Disk not partitioned or invalid MBR signature.\n");
        return;
    }

    // Disk total estimation (simplified: based on partition sum or fixed 64MB for this demo)
    // In a real OS we'd use ATA IDENTIFY 
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_CYAN, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("\n--- MesaOS Disk Manager ---\n");
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Disk /dev/hda: 64 MiB (67108864 bytes)\n\n");

    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::LIGHT_GREY, MesaOS::Drivers::VGAColor::BLACK));
    vga.write_string("Num  Boot  Type         Format      Size (MB)   Sectors\n");
    vga.write_string("-----------------------------------------------------------\n");

    for (int i = 0; i < 4; i++) {
        if (mbr->partitions[i].type == 0) continue;
        
        char buf[32];
        
        // Number
        itoa(i + 1, buf, 10);
        vga.write_string(buf);
        vga.write_string("    ");

        // Boot
        vga.write_string(mbr->partitions[i].attributes & 0x80 ? "*" : " ");
        vga.write_string("     ");
        
        // Type (Hex)
        itoa(mbr->partitions[i].type, buf, 16);
        vga.write_string("0x");
        vga.write_string(buf);
        vga.write_string("       ");

        // Format Name
        if (mbr->partitions[i].type == 0x83) vga.write_string("Linux      ");
        else if (mbr->partitions[i].type == 0x07) vga.write_string("NTFS/HPFS  ");
        else if (mbr->partitions[i].type == 0x0C) vga.write_string("FAT32 LBA  ");
        else if (mbr->partitions[i].type == 0x0B) vga.write_string("FAT32      ");
        else vga.write_string("Unknown    ");

        // Size in MB (sectors * 512 / 1024 / 1024)
        uint32_t size_mb = (mbr->partitions[i].sector_count * 512) / (1024 * 1024);
        itoa(size_mb, buf, 10);
        vga.write_string(buf);
        vga.write_string(" MiB");
        for(size_t k = strlen(buf); k < 7; k++) vga.write_string(" ");

        // Sectors
        itoa(mbr->partitions[i].sector_count, buf, 10);
        vga.write_string(buf);
        vga.write_string("\n");
    }
    vga.write_string("\n");
}

} // namespace MesaOS::FS
