#CROSS_COMPILE = arm-none-linux-gnueabi-
PRO_PATH = $(shell pwd)
SDK_ROOT = /home/imx/Rockchip/rv1126_rv1109_linux_sdk_v1.8.0_20210224/buildroot/output/rockchip_rv1126_rv1109
SDK_INCLUDE = $(SDK_ROOT)/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include
$(info PRO_PATH=$(PRO_PATH))
$(info SDK_ROOT=$(SDK_ROOT))
CROSS_COMPILE = $(SDK_ROOT)/host/usr/bin/arm-linux-gnueabihf-


AS			= $(CROSS_COMPILE)as
LD			= $(CROSS_COMPILE)ld
CC			= $(CROSS_COMPILE)gcc
CPP			= $(CROSS_COMPILE)g++
AR			= $(CROSS_COMPILE)ar
NM			= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

CFLAGS := -std=c++11 -O3 -Wall -fpermissive -g -DDEBUG -DAUDIO_ALGORITHM_ENABLE -DHAVE_LIBV4L2 -DLIBDRM -DLIVE555_SERVER_H264 -DLIVE555_SERVER_H265 -DMPP_SUPPORT_HW_OSD -DRGA_OSD_ENABLE -DRKAIQ -DRK_MOVE_DETECTION -DUSE_ROCKFACE -DUSE_ROCKX -Deasymedia_EXPORTS

CFLAGS += -DDRAW_RST
CFLAGS += -DPREDICT_ENABLE
#CFLAGS += -DDEBUG_ABLE

CFLAGS += -I$(PRO_PATH)/opencv3/include
CFLAGS += -I$(SDK_ROOT)/build/rockx/sdk/rockx-rv1109-Linux/include
CFLAGS += -I$(SDK_ROOT)/build/rockx/sdk/rockx-rv1109-Linux/include/modules
CFLAGS += -I$(SDK_ROOT)/build/rockx/sdk/rockx-rv1109-Linux/include/utils
CFLAGS += -I$(PRO_PATH)/include
CFLAGS += -I$(PRO_PATH)/rkmedia
CFLAGS += -I$(SDK_INCLUDE)/rkaiq/xcore
CFLAGS += -I$(SDK_INCLUDE)/rkaiq/uAPI	
CFLAGS += -I$(SDK_INCLUDE)/rkaiq/common	
CFLAGS += -I$(SDK_INCLUDE)/rkaiq/algos	
CFLAGS += -I$(SDK_INCLUDE)/rkaiq/iq_parser
CFLAGS += -I$(SDK_INCLUDE)/drm
CFLAGS += -I$(SDK_INCLUDE)/rga



CFLAGS += -I$(PRO_PATH)/libjpeg
CFLAGS += -I$(PRO_PATH)/freetype/include/freetype2
CFLAGS += -I$(PRO_PATH)/rkcaralgo
CFLAGS += -I$(PRO_PATH)/iconv/include


LDFLAGS := -lm -lssl -lcrypto -lz -lpthread -lrknn_api -lxhlpr_rknn -lrga -lLicClient -L$(PRO_PATH)/rkcaralgo -lrockx -L$(SDK_ROOT)/build/rockx/sdk/rockx-rv1109-Linux/lib -leasymedia -L$(SDK_ROOT)/build/rkmedia/src -lrkaiq -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_videoio -L$(PRO_PATH)/opencv3/lib -ljpeg -L$(PRO_PATH)/libjpeg -lfreetype -L$(PRO_PATH)/freetype/lib -lrtsp -L$(PRO_PATH)/librtsp -L$(PRO_PATH)/iconv/libs -liconv -lcharset


TARGET = carplate_main


SRC = carplate_main.cpp
SRC += car_md5.c
SRC += car_gethttps.c
SRC += car_bbbasicmsg.c
SRC += car_bbloctask.c
SRC += car_bbnet.c
SRC += car_bbpara.c
SRC += car_bbparse.cpp
SRC += car_sericom.c
SRC += car_master_server.cpp
SRC += parking_upload_httpservice.cpp

SRC += ./common/sample_common_isp.cpp
SRC += ./common/sample_fake_isp.cpp

SRC += ./util/cjson.c
SRC += ./util/chtext.cpp
SRC += ./util/md5.cpp
SRC += ./util/base64.cpp

#SRC += ./common/sample_common_isp.c
#SRC += ./common/sample_fake_isp.c

all: 
	$(CPP) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	$(CC) carplate_start.c -o carplate_start
	cp $(TARGET) release/
	cp $(TARGET) /home/imx/nfsroot/CarPortDeploy/
	cp carplate_start release/

clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)
