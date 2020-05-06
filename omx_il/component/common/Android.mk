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

FOILPLANET_OMX_TOP ?= $(LOCAL_PATH)/../../
FOILPLANET_OMX_INC ?= $(FOILPLANET_OMX_TOP)/include

#
# libfpomx_common
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=                      \
    Foilplanet_OMX_Basecomponent.cc     \
    Foilplanet_OMX_Baseport.cc          \
    osal_android.cc                     \
    osal_event.cc                       \
    osal_rga.cc

LOCAL_MODULE := libfpomx_common
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS :=
LOCAL_CPP_EXTENSION := .cc

LOCAL_STATIC_LIBRARIES := mpp-wrapper
LOCAL_SHARED_LIBRARIES := libcutils libutils liblog

LOCAL_C_INCLUDES :=                     \
    $(FOILPLANET_OMX_INC)/foilplanet    \
    $(FOILPLANET_OMX_INC)/khronos       \
    $(FOILPLANET_OMX_TOP)/component

include $(BUILD_STATIC_LIBRARY)

#
# libfpomx_rm
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    Foilplanet_OMX_Resourcemanager.cc

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libfpomx_rm
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 64
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS :=
LOCAL_CPP_EXTENSION := .cc

LOCAL_STATIC_LIBRARIES := mpp-wrapper
LOCAL_SHARED_LIBRARIES := libmpp libcutils libutils liblog

LOCAL_C_INCLUDES :=                     \
    $(FOILPLANET_OMX_INC)/foilplanet    \
    $(FOILPLANET_OMX_INC)/khronos

include $(BUILD_SHARED_LIBRARY)
