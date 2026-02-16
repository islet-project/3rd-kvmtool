#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: F8E19D059A7E88302D2E8E38D8AEEE2930AFABD9C4B4F3D575C8C7E3D8C7009B9D7BB5A85469A79933AD485833391462860A4D46CAA5902AEA2EAC7C5FBC60D0
