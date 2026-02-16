#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3-its \
	--network virtio \
	--virtio-transport=mmio \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 882AA0746488117396E8196894B37931B0CAC2882AF59573BAB923DCEE67A569AE0F0BCAABF787A8E40343D8F33972E6CA59FE22FA3C3013EE0290D77A2F81F4
