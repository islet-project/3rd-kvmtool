if [[ -z "$ANDROID_PRODUCT_OUT" ]];
then
	echo "The ANDROID_PRODUCT_OUT environmental variable is not set. Please setup the AOSP's build environment"
	exit 1
fi

../lkvm-rim-measurer run \
	--name ODCCService \
	--vsock 2048 \
	--balloon \
	--params crashkernel=17M \
	--mem 2048 \
	--cpus 1 \
	--term-file n=0,out=/proc/self/fd/32,in=/dev/null \
	--term-file n=1,out=/proc/self/fd/14 \
	--term-file n=4,out=/proc/self/fd/32,in=/proc/self/fd/33 \
	--initrd $ANDROID_PRODUCT_OUT/apex/com.android.virt/etc/microdroid_initrd_debuggable.img \
	--disk disk1.img \
	--disk disk2.img,vm-instance \
	--disk disk3.img,encryptedstore \
	--disk disk4.img \
	--kernel $ANDROID_PRODUCT_OUT/apex/com.android.virt/etc/fs/microdroid_kernel \
	--ipc-dir ipc-dir \
	--debug \
	--realm \
	--params androidboot.selinux=permissive \
	--realm-pv-hex c85f3e4db5ebb451a6e384c77da00a4eff1dfc849bafb551ce40a4c548f4807ced8e7312e5129c143a731c2cb6ef2c57db9e5c98407bc061693c40ecd44f038b
