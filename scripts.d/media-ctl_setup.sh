SENSOR_NAME="ox05b1s 10-0036"
MEDIA_DEV=
VIDEO_DEV=
SENSOR_DEV=

declare -a SENSOR_FORMAT
SENSOR_FORMAT[0]="SRGGB10_1X10"
SENSOR_FORMAT[1]="SGRBG10_1X10"
SENSOR_FORMAT[2]="SGBRG10_1X10"
SENSOR_FORMAT[3]="SBGGR10_1X10"

SENSOR_HEIGHT="1944"
SENSOR_WIDTH="2592"


# Detect devices
#########################################
# Detect media device
for tmp_dev in `ls /dev/media*`; do
	media-ctl -p -d ${tmp_dev} | grep -q "${SENSOR_NAME}" &>/dev/null
	if [ ${?} -eq 0 ]; then
		MEDIA_DEV="${tmp_dev}"
		break
	fi
done
if [ -z ${MEDIA_DEV} ]; then
	echo "Media device: Not Found!!!"
	exit -1
else
	echo "Media device: ${MEDIA_DEV}"
fi

# Find video device
VIDEO_DEV=`media-ctl -p -d "${MEDIA_DEV}" | grep '/dev/video' | head -n 1 | sed -E 's|\s*device\s*node\s*name\s*(/dev/video[0-9]+)|\1|g'`
if [ -z ${VIDEO_DEV} ]; then
	echo "Video device: Not Found!!!"
	exit -1
else
	echo "Video device: ${VIDEO_DEV}"
fi

# Find the sensor's sub-device
SENSOR_DEV=`media-ctl -p -d "${MEDIA_DEV}" | grep -E -A 3 "\-\s*entity.*:\s*${SENSOR_NAME}" | grep 'device node name' | sed -E 's|^\s*device node name\s*(.*)$|\1|'`
if [ -z ${SENSOR_DEV} ]; then
	echo "Sensor device: Not Found!!!"
	exit -1
else
	echo "Sensor device: ${SENSOR_DEV}"
fi

# Get sensor flip
SENSOR_DEV_HFLIP=`v4l2-ctl -d "${SENSOR_DEV}" --list-ctrls-menu | grep 'horizontal_flip' | sed -E 's|.*\svalue=([0-1]+)\s.*|\1|g'`
SENSOR_DEV_VFLIP=`v4l2-ctl -d "${SENSOR_DEV}" --list-ctrls-menu | grep 'vertical_flip' | sed -E 's|.*\svalue=([0-1]+)\s.*|\1|g'`
SENSOR_FORMAT_IDX=$((${SENSOR_DEV_HFLIP} + (${SENSOR_DEV_VFLIP} * 2)))
echo "Sensor format: ${SENSOR_FORMAT[${SENSOR_FORMAT_IDX}]}"

# Create sensor format string
SENSOR_FORMAT_STR="${SENSOR_FORMAT[${SENSOR_FORMAT_IDX}]}/${SENSOR_WIDTH}x${SENSOR_HEIGHT}"

# Initialise sensor pipeline
#########################################
# Reset media device
media-ctl --verbose -d "${MEDIA_DEV}" -r &>/dev/null
if [ ${?} -ne 0 ]; then
	echo "Failed to reset media device: ${MEDIA_DEV}"
	exit -1
fi

# Route MIPI stream directly rather than through ISP
media-ctl --verbose -d "${MEDIA_DEV}" -l ''\''csi2'\'':4 -> '\''rp1-cfe-csi2_ch0'\'':0 [1]' &>/dev/null
if [ ${?} -ne 0 ]; then
	echo "Failed to route MIPI data to video device: ${MEDIA_DEV}"
	exit -1
fi

# Set format on sensor stream
media-ctl --verbose -d "${MEDIA_DEV}" -V "'ox05b1s 10-0036':0 [fmt:${SENSOR_FORMAT_STR} field:none colorspace:raw]" &>/dev/null
if [ ${?} -ne 0 ]; then
	echo "Failed to set sensor stream format: ${MEDIA_DEV}"
	exit -1
fi

# Set format on CSI sink
media-ctl --verbose -d "${MEDIA_DEV}" -V "'csi2':0 [fmt:${SENSOR_FORMAT_STR} field:none colorspace:raw]" &>/dev/null
if [ ${?} -ne 0 ]; then
	echo "Failed to set CSI sink format: ${MEDIA_DEV}"
	exit -1
fi

# Set format on CSI source
media-ctl --verbose -d "${MEDIA_DEV}" -V "'csi2':4 [fmt:${SENSOR_FORMAT_STR} field:none colorspace:raw]" &>/dev/null
if [ ${?} -ne 0 ]; then
	echo "Failed to set CSI source format: ${MEDIA_DEV}"
	exit -1
fi

# Set video device format
#v4l2-ctl --verbose -d "${VIDEO_DEV}" --set-fmt-video=width=2592,height=1944,pixelformat=PC1B &>/dev/null
#if [ ${?} -ne 0 ]; then
#	echo "Failed to video device format: ${VIDEO_DEV}"
#	exit -1
#fi

echo "Configured sensor: ${SENSOR_FORMAT_STR}"
