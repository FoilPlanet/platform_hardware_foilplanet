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

ifeq ($(strip $(BOARD_USE_DRM)), true)
    OMX_USE_DRM := true
else
    OMX_USE_DRM := false
endif

include $(CLEAR_VARS)

# $(info $(shell ($(LOCAL_PATH)/compile_setup.sh $(LOCAL_PATH))))

FOILPLANET_OMX_TOP := $(LOCAL_PATH)

FOILPLANET_OMX_INC := $(FOILPLANET_OMX_TOP)/include/
FOILPLANET_OMX_COMPONENT := $(FOILPLANET_OMX_TOP)/component

include $(FOILPLANET_OMX_TOP)/mpp-wrapper/Android.mk
include $(FOILPLANET_OMX_TOP)/core/Android.mk

include $(FOILPLANET_OMX_COMPONENT)/common/Android.mk
include $(FOILPLANET_OMX_COMPONENT)/video/dec/Android.mk

# TODO
# include $(FOILPLANET_OMX_COMPONENT)/video/enc/Android.mk


