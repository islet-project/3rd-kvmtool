#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3-its \
	--network virtio \
	--virtio-legacy \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: F1144851B916E5B562B5AE1A6FC966479DF52AADCA37F7FB4AA781A25FE0DF8D7250923DD1B167EEE38F5EF9AD87832CBB87A27F87CC8368B78DDB0BD4D21882
