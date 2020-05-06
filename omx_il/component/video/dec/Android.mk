#
# Copyright 2019-2020 FoilPlanet Tech., Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

FOILPLANET_OMX_TOP ?= $(LOCAL_PATH)/../../..
FOILPLANET_OMX_INC ?= $(FOILPLANET_OMX_TOP)/include

#
# libomxvpu_dec
#
include $(CLEAR_VARS)

ifeq ($(PLATFORM_VERSION),7.1.2)
# BOARD_VERSION_BASE := true
endif

LOCAL_CFLAGS += -DUSE_ANB

ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 8.0)))
# LOCAL_CFLAGS += -DUSE_ION
LOCAL_CFLAGS += -DAVS80
endif

LOCAL_SRC_FILES :=  \
    vdec_control.cc \
    vdec.cc         \
    library_register.c

LOCAL_MODULE := libomxvpu_dec
#LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MULTILIB := 64
LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES := \
    $(FOILPLANET_OMX_INC)/khronos           \
    $(FOILPLANET_OMX_INC)/foilplanet        \
    $(FOILPLANET_OMX_TOP)/core              \
    $(FOILPLANET_OMX_TOP)/component/common

LOCAL_STATIC_LIBRARIES := \
    mpp-wrapper           \
    libfpomx_common 
    
LOCAL_SHARED_LIBRARIES := \
    libc        \
    libdl       \
    libcutils   \
    libutils    \
    liblog      \
    libui       \
    libfpomx_rm \
    libhardware \
    libvpu      \
    libmpp

ifeq ($(OMX_USE_DRM), true)
LOCAL_SHARED_LIBRARIES += librga
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_HEVC)), )
LOCAL_CFLAGS += -DSUPPORT_HEVC=1
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_VP6)), )
LOCAL_CFLAGS += -DSUPPORT_VP6=1
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_VP9)), )
LOCAL_CFLAGS += -DSUPPORT_VP9=1
endif

include $(BUILD_SHARED_LIBRARY)
