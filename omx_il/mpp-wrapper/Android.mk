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

# For building in AOSP
ifeq ($(TARGET_ARCH_ABI),)
TARGET_ARCH_ABI := $(TARGET_ARCH_VARIANT)
endif

#
# libosal
#
# include $(CLEAR_VARS)
# LOCAL_MODULE := libosal
# LOCAL_MODULE_CLASS := STATIC_LIBRARIES
# LOCAL_MODULE_SUFFIX := .a
# LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libosal.a
# LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc/osal
# include $(BUILD_PREBUILT)

#
# libmpp
#
include $(CLEAR_VARS)
LOCAL_MODULE := libmpp
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libmpp.so
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmpp
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libmpp.so
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
include $(BUILD_PREBUILT)

#
# libvpu (mpp/legacy)
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvpu
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libvpu.so
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvpu
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libvpu.so
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
include $(BUILD_PREBUILT)

#
# libmpp_static
#
include $(CLEAR_VARS)
LOCAL_MODULE := libmpp_static
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libmpp_static.a
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
include $(PREBUILT_STATIC_LIBRARY)

#
# mpp-wrapper
#
include $(CLEAR_VARS)

LOCAL_MODULE := mpp-wrapper

LOCAL_CFLAGS += -DANBOX=1

LOCAL_C_INCLUDES +=         \
    $(LOCAL_PATH)/inc       \
    $(LOCAL_PATH)/inc/osal
    
LOCAL_SRC_FILES +=          \
    src/MppEncoder.cc       \
    src/MppWrapper.cc

LOCAL_STATIC_LIBRARIES += minicap-common
# LOCAL_STATIC_LIBRARIES += libmpp_static
# LOCAL_SHARED_LIBRARIES += libmpp

# ifneq ($(USE_LIBJPEG_TURBO),)
# LOCAL_STATIC_LIBRARIES += jpeg-turbo
# endif

# LOCAL_EXPORT_C_INCLUDES
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/inc       \
    $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
