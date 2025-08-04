# Radxa kernel build with OX05B1S sensor support

Built with:
* Build OS: Ubuntu 24.04 (Noble)
* Radxa kernel version: 6.1.115 (commit f4082319ad2a4ca5d190aa2f28bee6d773193559)

## Preparation
This is a slightly complicated build process because Radxa have split their module overlays from the rest of the kernel.

You need to GIT clone:
	https://github.com/radxa/kernel.git
	https://github.com/radxa-pkg/radxa-overlays.git

You will also have to manually download:
	https://raw.githubusercontent.com/radxa-repo/bsp/refs/heads/main/linux/.stable-6.1/0100-vendor/0001-VENDOR-Add-Radxa-overlays.patch

This is a patch file to integrate the overlays into the main kernel build process.

So having cloned both repositories. You need to symlink the overlays into the kernel source, so from the root of the kernel repository.
```
	cd arch/arm64/boot/dts/rockchip/
	ln -s <path_2_overlays_repo>/arch/arm64/boot/dts/rockchip/overlays/
	cd -
	cd arch/arm64/boot/dts/amlogic/
	ln -s <path_2_overlays_repo>/arch/arm64/boot/dts/amlogic/overlays/
	cd -
```
Then you need to apply the patch file that you downloaded, so in the root of the kernel source repository.
```
	git apply <path_2_patchfile>/0100-vendor/0001-VENDOR-Add-Radxa-overlays.patch
```

Once that is done, the overlays are part of the kernel.

Now the ox05b1s sensor driver needs integrating, from the kernel source root.
```
	cd drivers/media/i2c/
	ln -s <path_2_sensor_src_directory> ox05b1s
	cd -
	cd arch/arm64/boot/dts/rockchip/overlays/
	ln -s <path_2_sensor_src_directory>/overlay/rock-5b-plus-cam0-wavesvision-ox05b1s-2l.dts
	ln -s <path_2_sensor_src_directory>/overlay/rock-5b-plus-cam0-wavesvision-ox05b1s-4l.dts
	ln -s <path_2_sensor_src_directory>/overlay/rock-5b-plus-cam1-wavesvision-ox05b1s-2l.dts
	ln -s <path_2_sensor_src_directory>/overlay/rock-5b-plus-cam1-wavesvision-ox05b1s-4l.dts
	cd -
```


## Patch files
In addition to manually linking the source trees, a number of files need updating and so patch files are provided.
Because git is applying the patch, and the repositories are split, this has to be done on the 2 different repositories.
So, in the root of the overlays repository:
```
	git apply <path_2_patches>/0001_radxa_overlays.patch
```
In the root of the kernel repository:
```
	git apply <path_2_remaining_patches>
```

Remaining patch files:
	0001_radxa_overlays.patch		- Patches overlays Makefile
	0002_radxa_kernel_config.patch		- Patches rockchip_linux_defconfig to include drivers for the sensor
	0003_radxa_kernel_driver.patch		- Patches i2c media driver's Kconfig/Makefile 
	0005_radxa_kernel_vcm.patch		- DW9714 VCM driver from RPi repo (Optional) 


## Required packages
To install the necessary packages, run:
```
	sudo apt-get install build-essential crossbuild-essential-arm64
```


## Build
This driver has been built using Ubuntu 24.04, while the standard BSP should woork, this has not been tested.
To build:
```
	make clean
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j4 rockchip_linux_defconfig
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j4 bindeb-pkg
```

N.B.: Alter -j to reflect the number of CPU threads to uses

That will build the kernel/modules/DTBs, and create a bunch of Debian packages from them, placed in the directory above the kernel source directory.
E.g.:
```
linux-headers-6.1.43-467218-g3e26818dcaaa-dirty_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.deb
linux-image-6.1.43-467218-g3e26818dcaaa-dirty_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.deb
linux-image-6.1.43-467218-g3e26818dcaaa-dirty-dbg_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.deb
linux-libc-dev_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.deb
linux-upstream_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.buildinfo
linux-upstream_6.1.43-467218-g3e26818dcaaa-dirty-80_arm64.changes
```


## Installation
Copy the linux-image-<version>_arm64.deb to the system, and you can install as a Debian package using:
```
	sudo apt install ./linux-image-<version>_arm64.deb
```

Depending on your version of U-Boot installed, you may find the kernel will not boot.
This is because it has not been built with gzip support.
If this is the case, after installing the kernel package you need to run the following commands:
```
cd /boot
mv vmlinuz-<new_kernel_version> vmlinuz-<new_kernel_version>.gz
gunzip vmlinuz-<new_kernel_version>.gz
```


## Testing
Once you have booted the kernel you can confirm that the driver has loaded correctly by running:
```
sudo dmesg |grep ox05b1s
```

You should see output similar to:
```
[   17.048243] platform csi2-dphy1: Fixed dependency cycle(s) with /i2c@feab0000/camera0-ox05b1s@36
[   17.074838] ox05b1s 3-0036: driver version: 00.01.00
[   17.074874] ox05b1s 3-0036: Using 4 data lanes
[   17.074943] ox05b1s 3-0036: OX05B1S power on
[   17.584341] ox05b1s 3-0036: OX05B1S power off
[   17.584890] ox05b1s 3-0036: OmniVision OX05B1S camera probed
[   17.897260] ox05b1s 3-0036: Consider updating driver ox05b1s to match on endpoints
[   17.897277] rockchip-csi2-dphy csi2-dphy1: dphy1 matches m00_b_ox05b1s 3-0036:bus type 5
```
