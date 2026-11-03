# OX05B1S driver

## Introduction
This driver provides basic support for the OX05B1S sensor module on the RPi 5.

## libcamera integration changes
The following changes are required for the Raspberry Pi PiSP libcamera
pipeline:

* `V4L2_CID_HBLANK` is exposed as a read-only control. The line length is
  derived from the selected link frequency; the 4-lane 450 MHz mode uses
  3008 pixels per line (HTS `0x178` in units of 8 pixels), giving 416 pixels
  of horizontal blanking for a 2592-pixel-wide image.
* `V4L2_CID_PIXEL_RATE` reports 384 MHz. The previous 240 MHz value made the
  IPA compute incorrect exposure and frame duration limits.
* `V4L2_CID_CAMERA_ORIENTATION` and `V4L2_CID_CAMERA_SENSOR_ROTATION` are
  exposed from the `orientation` and `rotation` device tree properties.

## Requirements
This driver has been developed to be installed using DKMS on the Raspberry Pi.
Install the build tools and kernel headers first:
```
sudo apt-get install dkms build-essential device-tree-compiler
sudo apt-get install raspberrypi-kernel-headers || sudo apt-get install linux-headers-$(uname -r)
```

## Installation
To build the driver, first clone the repository:
```
git clone https://github.com/wavesvision/kmod-ox05b1s-basic.git
```
Copy the source to `/usr/src` with the version name expected by DKMS:
```
sudo rm -rf /usr/src/ox05b1s-0.2.0
sudo cp -a kmod-ox05b1s-basic /usr/src/ox05b1s-0.2.0
```
Add, build and install the driver:
```
sudo dkms add -m ox05b1s -v 0.2.0
sudo dkms build -m ox05b1s -v 0.2.0
sudo dkms install -m ox05b1s -v 0.2.0
```
If you wish to uninstall the driver:
```
sudo dkms remove -m ox05b1s -v 0.2.0 --all
```
Check that the module and overlay were installed:
```
ls /boot/overlays/ox05b1s.dtbo /lib/modules/$(uname -r)/updates/dkms/ox05b1s.ko.xz
```
Finally edit `/boot/firmware/config.txt` and add under the `[pi5]` section:
```
[pi5]
dtoverlay=ox05b1s,cam0,4lane,vcm
camera_auto_detect=0
```
Use `cam1` instead of `cam0` if the camera is on the other connector.

The `vcm` parameter enables the DW9714 autofocus lens. Install the DW9714
driver first by following:
https://github.com/wavesvision/kmod-dw9714

Reboot after changing the overlay.

# Testing
To check that the camera has been detected, run:
```
sudo dmesg |grep ox05
```
You should see several lines of output, specifically:
```
ox05b1s 10-0036: __ox05b1s_detect: detected chip ID 0x580542
```
Check that the sensor exposes the libcamera timing controls:
```
v4l2-ctl -d /dev/v4l-subdevN --list-ctrls
```
Use the `ox05b1s` subdevice from `media-ctl -p -d /dev/media0`. The expected
values include `horizontal_blanking = 416`, `pixel_rate = 384000000` and
`link_frequency = 450000000`.
