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

#ifndef _FOILPLANET_OSAL_RGA_H_
#define _FOILPLANET_OSAL_RGA_H_

#include "OMX_Def.h"

//#include "vpu_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

OMX_S32 rga_dev_open(void **rga_ctx);

OMX_S32 rga_dev_close(void *rga_ctx);

#ifdef USE_RKVPU
void rga_nv12_copy(FoilplanetVideoPlane *plane, VPUMemLinear_t *vpumem, 
                   uint32_t Width, uint32_t Height, void *rga_ctx);

void rga_rgb_copy(FoilplanetVideoPlane *plane, VPUMemLinear_t *vpumem, 
                  uint32_t Width, uint32_t Height, void *rga_ctx);

void rga_rgb2nv12(FoilplanetVideoPlane *plane,  VPUMemLinear_t *vpumem ,
                  uint32_t Width, uint32_t Height, uint32_t dstWidth, uint32_t dstHeight, void *rga_ctx);

void rga_nv12_crop_scale(FoilplanetVideoPlane *plane, VPUMemLinear_t *vpumem, OMX_VIDEO_PARAMS_EXTENDED *param_video,
                         OMX_U32 orgin_w, OMX_U32 orgin_h, void *rga_ctx);

void rga_nv122rgb(FoilplanetVideoPlane *planes, VPUMemLinear_t *vpumem,
                  uint32_t mWidth, uint32_t mHeight, int dst_format, void *rga_ctx);
#endif /* USE_RKVPU */

#ifdef __cplusplus
}
#endif

#endif /* _FOILPLANET_OSAL_RGA_H_ */

