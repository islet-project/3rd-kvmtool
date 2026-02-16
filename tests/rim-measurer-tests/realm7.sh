#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha256" \
	--disable-sve \
	--console virtio \
	--irqchip=gicv3 \
	--network virtio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 4D7E2201764CD355C6336835E18686D932C2BA6F8801F403EF15691E437FC4D6
