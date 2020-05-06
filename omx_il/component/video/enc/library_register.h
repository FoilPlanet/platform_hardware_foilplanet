/*
 * Copyright 2019-2020 FoilPlanet Tech., Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _FOILPLANET_OMX_ENC_REG_H_
#define _FOILPLANET_OMX_ENC_REG_H_

#include "OMX_Def.h"
#include "OMX_Component.h"

// core
#include "Foilplanet_OMX_Component_Register.h"
#include "Foilplanet_OMX_Core.h"

#define OSCL_EXPORT_REF __attribute__((visibility("default")))

#define RK_OMX_COMPONENT_H264_ENC       "OMX.fp.video_encoder.avc"
#define RK_OMX_COMPONENT_H264_ENC_ROLE  "video_encoder.avc"

#define RK_OMX_COMPONENT_VP8_ENC        "OMX.fp.video_encoder.vp8"
#define RK_OMX_COMPONENT_VP8_ENC_ROLE   "video_encoder.vp8"

#define RK_OMX_COMPONENT_HEVC_ENC       "OMX.fp.video_encoder.hevc"
#define RK_OMX_COMPONENT_HEVC_ENC_ROLE  "video_encoder.hevc"

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF int Foilplanet_OMX_COMPONENT_Library_Register(FoilplanetRegisterComponentType **rockchipComponents);

#ifdef __cplusplus
};
#endif

#endif /* _FOILPLANET_OMX_ENC_REG_H_ */

