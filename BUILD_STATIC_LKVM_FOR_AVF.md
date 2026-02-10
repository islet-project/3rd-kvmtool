Firstly, you need to have aarch64 cross compiler (aarch64-linux-gnu-) installed on your Linux machine.


1. Clone the DTC project git://git.kernel.org/pub/scm/utils/dtc/dtc.git
2. Build the Libfdt library:

```
	git clone git://git.kernel.org/pub/scm/utils/dtc/dtc.git
	cd dtc
	CC=aarch64-linux-gnu-gcc make
	cd -
```

3. Clone the kvmtool source code adapted to run Realms in AVF architecture


```
	git clone -b aosp/cca/v4 git@github.com:islet-project/3rd-kvmtool.git
```

4. Build the kvmtool

```
	cd 3rd-kvmtool
	CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 LIBFDT_DIR=../dtc/libfdt make lkvm-static
```

