# OX05B1S driver

## Introduction
This driver provides basic support for the OX05B1S sensor module on the RPi 5.

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
