#!/bin/sh

../../lkvm-rim-measurer run \
	--debug \
	--realm \
	--measurement-algo="sha512" \
	--disable-sve \
	--console serial \
	--irqchip=gicv3 \
	--network virtio \
	--rng \
	--9p /shared,FMR \
	-m 256M \
	-c 1 \
	-k linux.realm \
	-i rootfs-realm.cpio.gz \
	-p "earlycon=ttyS0 printk.devkmsg=on"

# RIM: D40DF81B4403DF695A86DBA547B46663CDE31BBE5D5B7DAFE2114392F1259D0846D77F5D9F2344BBFCEC057013C53BADF8B2824BC0E8F0118B97E9FDFFAC6BB9
