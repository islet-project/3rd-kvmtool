#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console virtio \
	--irqchip=gicv3 \
	--network virtio \
	--virtio-transport=mmio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: CD4EAA35DBA2533300C1121A0BAA9FB35666F3837496E5FF6C4EF617BECF4E09F147A84CD78EC374DE140A710FBAFB5A5DB1945A14BD1A1DB12E0E0DA3DCFBB8

