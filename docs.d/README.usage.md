# Using the OX05B1S driver

This document contains some basic information about the driver.


## Devices
The driver creates a number of devices in /dev, these include:
* /dev/video0		:= Video stream from ISP (default: packed)
* /dev/video1		:= Video stream from ISP (default: packed)
* /dev/video2		:= Video stream from ISP (default: packed)
* /dev/video3		:= Video stream from ISP (default: packed)
* /dev/v4l-subdev2	:= OX05B1S sensor
* /dev/v4l-subdev3	:= Lens VCM (DW9714) interface
* /dev/media0		:= Configuration of sensor, CIF/ISP interface

Notes:
* /dev/video{x} output the same stream, opening one will block streaming of video from the others
* /dev/video{x} streams are packed by default


## Packing
The ISP supports outputing both packed (4 pixels in 5 bytes), and unpacked (1 pixel in 2 bytes) streams.
While packing saves memory, when having to process the frames in the CPU rather than the ISP this introduces a high overhead having to unpack individual pixels.
It therefore can advantageous to disable the packing.
The ISP provides a SysFS interface to acheive this:
```
sys/devices/platform/rkcif-mipi-lvds2/compact_test
```
This interface supports both R/W operations.
To disable packing on all the interfaces, as root:
```
echo "0 0 0 0" > /sys/devices/platform/rkcif-mipi-lvds2/compact_test
```


## Video4Linux2
Get available frame formats:
```
v4l2-ctl --list-formats-ext -d /dev/video0
```

Get available sensor controls:
```
v4l2-ctl -l -d /dev/v4l-subdev2
```

Enable AEC:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='auto_exposure=0'
```

Disable AEC:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='auto_exposure=1'
```

Set exposure (manually):
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='exposure=68'
```

Enable AGC:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='gain_automatic=1'
```

Disable AGC:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='gain_automatic=0'
```

Set analogue gain (manually):
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='analogue_gain=15'
```

Enable test pattern generator:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='test_pattern=1'
```

Disbale test pattern generator:
```
v4l2-ctl -d /dev/v4l-subdev2 --set-ctrl='test_pattern=0'
```

Get register value (device must be streaming):
```
sudo v4l2-dbg -d /dev/v4l-subdev2 -g 0x3815
````

Set register value (device must be streaming):
```
sudo v4l2-dbg -d /dev/v4l-subdev2 -s 0x3815 0x33
````


## Media Pad
List media pad configuration:
```
media-ctl -p -d 0
```
