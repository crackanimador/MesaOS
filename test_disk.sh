#!/bin/bash
# Create a 64MB disk image with a partition table
dd if=/dev/zero of=disk.img bs=1M count=64
echo -e "n\np\n1\n\n\nt\n83\nw" | fdisk disk.img
