GCOV_PROFILE := y
ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
#HTC_CAMERA+
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/lc898123AXD_htc
obj-$(CONFIG_OIS_CALIBRATION) += lc898123AXD_htc/
#HTC_CAMERA-
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ flash/ eeprom/ ois/
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_init.o msm_sensor_driver.o msm_sensor.o
obj-$(CONFIG_MT9M114) += mt9m114.o
obj-$(CONFIG_OV5645) += ov5645.o
obj-$(CONFIG_OV2685) += ov2685.o
obj-$(CONFIG_OV2685_SCV3B4035) += ov2685_scv3b4035.o
obj-$(CONFIG_OV5645) += ov7695.o
obj-$(CONFIG_GC0310) += gc0310.o
obj-$(CONFIG_IMX214_HTC) += imx214_htc.o lc898123_htc.o
obj-$(CONFIG_OV4688_HTC) += ov4688_htc.o
obj-$(CONFIG_IMX377_HTC) += imx377_htc.o
obj-$(CONFIG_S5K4E6_FRONT_HTC) += s5k4e6_front_htc.o

#HTC_CAMERA+
obj-y += htc_awb_cal.o
#obj-$(CONFIG_OIS_CALIBRATION) += lc898123_htc.o lc898123_md5.o
#HTC_CAMERA-

