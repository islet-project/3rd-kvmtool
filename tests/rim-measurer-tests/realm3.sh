#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--kaslr-seed 12345 \
	--rng \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: A475412BA0AE2BBE407AC7E28C557ECED9C088112FE1DAE0B572AF46B25A6FB5C1ABAF7A15F1C865A28C9094C98BD1BCE2F292BDF2C7D962C6D3929B12A763B9

