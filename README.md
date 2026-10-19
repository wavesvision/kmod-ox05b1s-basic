# OX05B1S driver

## Introduction
This driver provides basic support for the OX05B1S sensor module on the RPi 5.

## libcamera integration changes
The following changes are required for the Raspberry Pi PiSP libcamera
pipeline:

* `V4L2_CID_HBLANK` is exposed as a read-only control. The line length is
  derived from the selected link frequency; the 4-lane 450 MHz mode uses
  2944 pixels per line (HTS `0x170` in units of 8 pixels), giving 352 pixels
  of horizontal blanking for a 2592-pixel-wide image.
* `V4L2_CID_PIXEL_RATE` reports 384 MHz. The previous 240 MHz value made the
  IPA compute incorrect exposure and frame duration limits.
* `V4L2_CID_CAMERA_ORIENTATION` and `V4L2_CID_CAMERA_SENSOR_ROTATION` are
  exposed from the `orientation` and `rotation` device tree properties.

## Requirements
This driver has been developed to be installed using DKMS on the Raspberry Pi.
So first install dkms:
```
sudo apt-get install dkms
```

## Installation
To build the driver, first clone the repository:
```
git clone https://github.com/wavesvision/kmod-ox05b1s-basic.git
```
Next copy the downloaded source to '/usr/src':
```
sudo cp -r kmod-ox05b1s-basic /usr/src/
```
Then create a symlink to represent the version you want to build:
```
sudo ln -s /usr/src/kmod-ox05b1s-basic /usr/src/ox05b1s-{module_version}
```
Now the driver needs to be added to DKMS:
```
sudo dkms add -m ox05b1s -v {module_version}
```
The driver can then be built with:
```
sudo dkms build -m ox05b1s -v {module_version}
```
Then the driver is installed with:
```
sudo dkms install -m ox05b1s -v {module_version}
```
If you wish to uninstall the driver, this can be done with:
```
sudo dkms remove -m ox05b1s -v {module_version}
```
If you want to check that it has placed the module & overlay in the correct place, you can run:
```
ls /boot/overlays/ox05b1s.dtbo /lib/modules/$(uname -r)/updates/dkms/ox05b1s.ko.xz
```
Finally the RPi's hardware configuration file needs updating, so you need to edit the file '/boot/firmware/config.txt' as root:
```
sudo nano /boot/firmware/config.txt
```
And add to the end of the file (adjust 'cam0' to 'cam1' is the camera is in port 1):
```
dtoverlay=ox05b1s,cam0,4lane
```
Now reboot!

# Testing
To check that the camera has been detected, run:
```
sudo dmesg |grep ox05
```
You should see several lines of output, specifically:
```
ox05b1s 10-0036: __ox05b1s_detect: detected chip ID 0x580542
```
