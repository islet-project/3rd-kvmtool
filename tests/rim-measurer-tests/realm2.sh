#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha256" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--kaslr-seed 12345 \
	--rng \
	--9p /shared,FMR \
	-m 128M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 2879F4E9F6ACC71442F16698022B0CD7B53E4B53F56231606BE812E2C7C53069

