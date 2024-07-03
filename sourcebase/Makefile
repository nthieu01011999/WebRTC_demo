###############################################################################
# author: ThanNT
# date: 6/1/2017
###############################################################################

-include config.mk

RED=\033[0;31m
GREEN=\033[0;32m
NC=\033[0m

RPI_SYS_DIR	=
TOOLS_DIR	=

# path to compile
# PATH=/home/huy/Workspace/FPT/Github/Camera/Realtek/rts39xx_sdk_v5.0.1/toolchain/asdk-10.3.1-a7-EL-5.4-u1.0-a32nh-220218/bin:$PATH

CROSS_COMPILE = 

ifdef BUILD_LINUX64
CROSS_COMPILE=
OBJ_DIR		= _build
NAME_MODULE	= fcam-vvtk-linux64
endif
ifdef BUILD_ARM_VVTK
CROSS_COMPILE=cross-compile/rts39xx_sdk_v5.0.1/toolchain/asdk-10.3.1-a7-EL-5.4-u1.0-a32nh-220218/bin/arm-linux-
endif

ifdef RELEASE
	ifeq ($(RELEASE), 1)
		OBJ_DIR		= _build_vvtk_$(FW_PRODUCT)-release-watchdog
		NAME_MODULE	= fcam-$(FW_PRODUCT)-release-watchdog
	else
		OBJ_DIR		= _build_vvtk_$(FW_PRODUCT)-release-nowatchdog
		NAME_MODULE	= fcam-$(FW_PRODUCT)-release-nowatchdog
	endif
else
	OBJ_DIR		= _build_vvtk_$(FW_PRODUCT)-debug
	NAME_MODULE	= fcam-$(FW_PRODUCT)-debug
endif

CXX	  =$(CROSS_COMPILE)g++
CC	  =$(CROSS_COMPILE)gcc
AR	  =$(CROSS_COMPILE)ar
STRIP =$(CROSS_COMPILE)strip


OPTIMIZE_OPTION	=	-g -s -O3
WARNNING_OPTION	+=	#-Werror -W -Wno-missing-field-initializers




CXXFLAGS += $(HAVE_FLAGS)

-include sources/ak/Makefile.mk
-include sources/sys/Makefile.mk
-include sources/app/Makefile.mk
-include sources/common/Makefile.mk
-include sources/driver/Makefile.mk
-include sources/libraries/Makefile.mk

ifdef BUILD_LINUX64
GENERAL_FLAGS +=\
	-DBUILD_LINUX64
else
GENERAL_FLAGS +=\
	-DBUILD_ARM_VVTK
endif

ifdef RELEASE
	ifeq ($(RELEASE), 1)
		GENERAL_FLAGS += -DRELEASE=1
	else
		GENERAL_FLAGS += -DRELEASE=0
	endif
endif

# CXX compiler option
CXXFLAGS	+=\
		$(OPTIMIZE_OPTION)	\
		$(WARNNING_OPTION)	\
		-Wall				\
-pipe				\
		-ggdb				\
		$(GENERAL_FLAGS) 

# Library paths
LDFLAGS	+= -Wl,-Map=$(OBJ_DIR)/$(NAME_MODULE).map


ifdef BUILD_LINUX64
DEPENDENCIES=sources/libraries/linux64
endif
ifdef BUILD_ARM_VVTK
#TODO fix vvtk path
DEPENDENCIES=sources/libraries/arm_rts
# LDLIBS +=  $(DEPENDENCIES)/lib/libfreetype.a
endif


#DEBUG-preprocessor
ifdef TEST_USE_WEB_SOCKET
CXXFLAGS += -DTEST_USE_WEB_SOCKET
endif

ifdef FEATURE_RTMP
	GENERAL_FLAGS += -DFEATURE_RTMP
endif

ifdef FEATURE_AI
GENERAL_FLAGS += -DFEATURE_AI
endif

ifdef VOICE_GUIDE
GENERAL_FLAGS += -DVOICE_GUIDE
endif

ifdef FEATURE_RECORD
GENERAL_FLAGS += -D FEATURE_RECORD
endif

ifdef BUILD_LINUX64
LDLIBS +=  $(DEPENDENCIES)/lib/libdatachannel.a
LDLIBS +=  $(DEPENDENCIES)/lib/libjuice.a
LDLIBS +=  $(DEPENDENCIES)/lib/libsrtp2.a
LDLIBS +=  $(DEPENDENCIES)/lib/libusrsctp.a

LDLIBS	+=\
		-lpthread			\
		-lrt				\
		-lm					\
		-lcurl				\
		-lcrypto			\
		-lssl				\
		-lmosquittopp				\
		-lmosquitto				
endif

ifdef BUILD_ARM_VVTK
LDLIBS +=  $(DEPENDENCIES)/lib/libmosquittopp.a
LDLIBS +=  $(DEPENDENCIES)/lib/libmosquitto.a

# lib webrtc
LDLIBS +=  $(DEPENDENCIES)/libdatachannel/lib/libdatachannel.a
LDLIBS +=  $(DEPENDENCIES)/libdatachannel/lib/libjuice.a
LDLIBS +=  $(DEPENDENCIES)/libdatachannel/lib/libsrtp2.a
LDLIBS +=  $(DEPENDENCIES)/libdatachannel/lib/libusrsctp.a

ifdef FEATURE_RTMP
LDLIBS +=  $(DEPENDENCIES)/lib/libRTMP.a
LDLIBS +=  $(DEPENDENCIES)/lib/librtmp.a
LDLIBS +=  $(DEPENDENCIES)/lib/libFPTHmac.a
endif

# #libmp4v2
# LDLIBS +=  $(DEPENDENCIES)/lib/libmp4v2.a

#libs3
LDLIBS +=  $(DEPENDENCIES)/lib/libs3.a

LDLIBS += -L$(DEPENDENCIES)/lib
LDLIBS	+=\
		-lpthread			\
		-lrt				\
		-lm					\
		-lcurl				\
		-lcrypto			\
		-lssl				\

#stb
LDLIBS	+=\
		-DSTB_IMAGE_IMPLEMENTATION			\
		-DSTB_IMAGE_WRITE_IMPLEMENTATION				\
		-lm					\
		-I$(PWD)/sources/libraries/arm_rts/include/stb	\
	
# lib SDK
LDLIBS	+=\
		-lvvtkhal

endif
		
all: create $(OBJ_DIR)/$(NAME_MODULE) copy

create:
	@echo mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)

	@echo "DEPENDENCIES" $(DEPENDENCIES)

	rm -rf $(OBJ_DIR)/$(NAME_MODULE)

$(OBJ_DIR)/%.o: %.cpp
	@echo CXX $<
	@$(CXX) -c -o $@ $< $(CXXFLAGS) -std=c++17 $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@echo CC $<
	@$(CC) -c -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(OBJ_DIR)/$(NAME_MODULE): $(OBJ)
	@echo ---------- START LINK PROJECT ----------
	@echo $(CXX) -o $@ $^ $(CXXFLAGS)
	@echo "\n\n"
	@echo LIB $(LDLIBS) "\n\n"audio_stream
	@$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

ifdef RELEASE
	@echo "\033[1;32m----------------- Project is build Release -----------------\033[0m"
else
	@echo "\033[1;31m----------------- Project is build Debug -----------------\033[0m"
endif


.PHONY: cp
copy: $(OBJ_DIR)/$(NAME_MODULE)
 
	sudo cp $(OBJ_DIR)/$(NAME_MODULE) /mnt/nfs_share/agent/
	
 

.PHONY: scp
scp:
	scp $(OBJ_DIR)/$(NAME_MODULE) huynt@192.168.1.141:/home/huynt/Workspace/FPT/Github/Camera/vvtk_nfs

.PHONY: flash
flash:
	@sudo LD_LIBRARY_PATH=/usr/local/lib/ $(OBJ_DIR)/$(NAME_MODULE)

.PHONY: debug
debug:
	sudo gdb $(OBJ_DIR)/$(NAME_MODULE)

.PHONY: install
install:
	cp $(OBJ_DIR)/$(NAME_MODULE) /mnt/usr/bin

.PHONY: clean
clean: 
	@echo rm -rf $(OBJ_DIR)
	@rm -rf $(OBJ_DIR)

.PHONY: strip
strip:
	$(STRIP) $(OBJ_DIR)/$(NAME_MODULE)

show_variables:
	@echo "DEPENDENCIES: $(DEPENDENCIES)"
	@echo "LDLIBS: $(LDLIBS)"

show-build:
ifdef RELEASE
	@echo "\033[1;32m----------------- Project is build Release -----------------\033[0m"
else
	@echo "\033[1;31m----------------- Project is build Debug -----------------\033[0m"
endif

agent-cp:
	cp -r $(OBJ_DIR)/$(NAME_MODULE) ../agent/bin/fcam

check:
ifneq ($(BUILD_ERROR), '')
	$(error error is $(BUILD_ERROR))
endif

build-agent: check agent-cp #generate_script

	@echo "\033[1;32mBuild model [$(FW_MODEL)]\033[0m"

	@mkdir -p $(BUILD_DIR)

	@../auto_gen.sh ../$(PROJECT_AGENT)/version  $(VENDOR) $(FW_MODEL) $(FW_NAME) $(VERSION) $(DATE_CURR) $(BUILD_TIME)
	mksquashfs ../$(PROJECT_AGENT) $(BUILD_DIR)/fpt_agent_$(FW_NAME).bin -comp xz 

	@echo "\033[1;32mBuild model [$(FW_MODEL)]\033[0m"

	cp $(BUILD_DIR)/* /mnt/nfs_share
#scp $(BUILD_DIR)/* huynt@192.168.1.141:/home/huynt/Workspace/FPT/Github/Camera/vvtk_nfs
# cp $(BUILD_DIR)/* /mnt/hgfs/shareFd/
#sudo cp $(BUILD_DIR)/* /mnt/nfs-host/vivotek/firmware/
# scp $(BUILD_DIR)/* huynt@192.168.1.141:/home/huynt/Workspace/FPT/Github/Camera/vvtk_nfs
# cp $(BUILD_DIR)/* /mnt/hgfs/shareFd/
#cp $(BUILD_DIR)/* /home/huy/Workspace/FPT/Github/Camera/nfs_vvtk

ifdef RELEASE
ifeq ($(RELEASE), 1)
	@echo "\033[1;32m----------------- Project is build Release -----------------\033[0m"
else
	@echo "\033[1;33m----------------- Project is build Release disable watchdog -----------------\033[0m"
endif
else
	@echo "\033[1;31m----------------- Project is build Debug -----------------\033[0m"
endif

ifeq ($(FACTORY), 1)
	@echo "\033[1;32m----------------- Project is build Factory -----------------\033[0m"
else
	@echo "\033[1;31m----------------- Project is build Dev -----------------\033[0m"
endif

hal-ver:
	@echo $(HAL_VERSION)

clean-agent:
	rm -rf $(BUILD_DIR)

build-release: check
	@echo $(FW_RELEASE)
	mkdir -p $(RELEASE_DIR)/$(RELEASE_MODEL)
	@file_count=$$(find $(BUILD_DIR) -maxdepth 1 -type f -name "fpt_agent_$(FW_RELEASE)" | wc -l); \
	if [ $${file_count} -eq 1 ]; then \
		file_name=$$(find $(BUILD_DIR) -maxdepth 1 -type f -name "fpt_agent_$(FW_RELEASE)" -exec basename {} \; | sed 's/^fpt_agent_//'); \
		echo "${GREEN}start build release agent file: $$file_name${NC}"; \
        echo "cp -r $(BUILD_DIR)/fpt_agent_$$file_name $(RELEASE_DIR)/$(RELEASE_MODEL)/agent.sqsh"; \
		cp -r $(BUILD_DIR)/fpt_agent_$$file_name $(RELEASE_DIR)/$(RELEASE_MODEL)/agent.sqsh;\
		\
		echo "../vvtk_fwpack -o $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin -r $(HAL_OTA_DIR)/linux_ota.bin -f $(RELEASE_DIR)/$(RELEASE_MODEL)/agent.sqsh"; \
		../vvtk_fwpack -o $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin -r $(HAL_OTA_DIR)/linux_ota.bin -f $(RELEASE_DIR)/$(RELEASE_MODEL)/agent.sqsh; \
		\
		echo "cp $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin $(RELEASE_DIR)/$(RELEASE_MODEL)/$$file_name"; \
		cp $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin $(RELEASE_DIR)/$(RELEASE_MODEL)/$$file_name; \
		\
		echo "md5sum $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin > $(RELEASE_DIR)/$(RELEASE_MODEL)/md5.txt"; \
		md5sum $(RELEASE_DIR)/$(RELEASE_MODEL)/linux_vvtk.bin > $(RELEASE_DIR)/$(RELEASE_MODEL)/md5.txt; \
		cat $(RELEASE_DIR)/$(RELEASE_MODEL)/md5.txt; \
		\
		cp -r $(RELEASE_DIR)/$(RELEASE_MODEL)/$$file_name /mnt/hgfs/shareFd/; \
	else \
	    echo "${RED}file agent not found${NC}"; \
	fi

clean-release:
	rm -rf $(RELEASE_DIR)/$(RELEASE_MODEL)

.PHONY: generate_script
generate_script:
	rm -rf fpt_services_management.sh
	@echo '#!/bin/sh' > fpt_services_management.sh
	@echo 'source /etc/profile\n' >> fpt_services_management.sh
	@echo 'cp -r /app/default/TZ /var/conf/\n' >> fpt_services_management.sh
	@echo 'udhcpc -s /app/bin/udhcpc.sh -i eth0 -p /var/run/eth0.pid &' >> fpt_services_management.sh
	@echo 'start_ntpdate > /dev/null 2>&1 &\n' >> fpt_services_management.sh
	@echo 'sleep 5\n' >> fpt_services_management.sh
	@echo "amixer -Dhw:0 cset name='Rear Amic Capture Volume' 60" >> fpt_services_management.sh
	@echo "amixer -Dhw:0 cset name='Master Playback Volume' 120\n" >> fpt_services_management.sh
	@echo 'APP_FCAM="/app/bin/fcam"\n' >> fpt_services_management.sh
ifdef RELEASE
	@echo 'echo Firmware is Release ------------------' >> fpt_services_management.sh
	@echo 'if [ -e $$APP_FCAM ]; then' >> fpt_services_management.sh
	@echo '    LD_LIBRARY_PATH=/app/lib/ $$APP_FCAM > /dev/null 2>&1' >> fpt_services_management.sh
	@echo 'else' >> fpt_services_management.sh
	@echo '    echo "file $$APP_FCAM not exist"' >> fpt_services_management.sh
	@echo 'fi' >> fpt_services_management.sh
else
	@echo 'echo Firmware is Debug ------------------' > fpt_services_management.sh
	@echo 'if [ -e "$$APP_FCAM" ]; then' >> fpt_services_management.sh
	@echo '    LD_LIBRARY_PATH=/app/lib/ "$$APP_FCAM" & echo $$! > /var/run/fcam.pid' >> fpt_services_management.sh
	@echo 'else' >> fpt_services_management.sh
	@echo '    echo "file $$APP_FCAM not exist"' >> fpt_services_management.sh
	@echo 'fi' >> fpt_services_management.sh
endif
	@chmod +x fpt_services_management.sh
	@cp fpt_services_management.sh ../agent/init
	@rm -rf fpt_services_management.sh

ifdef FACTORY
	@rm -rf MQTTService
	@echo '{' > MQTTService
	@echo '  "Password":"",' >> MQTTService
	@echo '  "Username":"",' >> MQTTService
	@echo '  "ClientID":"",' >> MQTTService
ifeq ($(FACTORY), 1)
	@echo '  "Host":"broker-mqtt.fcam.vn",' >> MQTTService
else
	@echo '  "Host":"beta-broker-mqtt.fcam.vn",' >> MQTTService
endif
	@echo '  "Port":8883,' >> MQTTService
	@echo '  "KeepAlive":30,' >> MQTTService
	@echo '  "QOS":1,' >> MQTTService
	@echo '  "Retain":true,' >> MQTTService
	@echo '  "TLSEnable":true,' >> MQTTService
	@echo '  "TLSVersion":"tlsv1.2",' >> MQTTService
	@echo '  "Protocol":"mqtt\/tls",' >> MQTTService
	@echo '  "Enable":true' >> MQTTService
	@echo '}' >> MQTTService
	@cp MQTTService ../agent/default
	@rm -rf MQTTService

	@rm -rf RtcServersConfig
	@echo '{' > RtcServersConfig
	@echo '    "Connections": [' >> RtcServersConfig
	@echo '        {' >> RtcServersConfig
	@echo '            "Type": "stun",' >> RtcServersConfig
	@echo '            "StunUrl": []' >> RtcServersConfig
	@echo '        },' >> RtcServersConfig
	@echo '        {' >> RtcServersConfig
	@echo '            "Type": "turn",' >> RtcServersConfig
	@echo '            "TurnUrl": ["turn:turnuser:camfptvnturn133099@stun-connect.fcam.vn:3478", "turn:turnuser:camfptvnturn133099@stunp-connect.fcam.vn:3478"]' >> RtcServersConfig
	@echo '        }' >> RtcServersConfig
	@echo '    ]' >> RtcServersConfig
	@echo '}' >> RtcServersConfig
	@cp RtcServersConfig ../agent/default
	@rm -rf RtcServersConfig
endif
