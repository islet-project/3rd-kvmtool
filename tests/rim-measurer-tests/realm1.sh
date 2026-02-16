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
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: 378F70BABA959120F98658ED97BACB8DC488F46A15B413E4D890AA4A402106C6
