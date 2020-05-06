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

#ifndef _FOILPLANET_OMX_RESOURCEMANAGER_H_
#define _FOILPLANET_OMX_RESOURCEMANAGER_H_

#include "OMX_Def.h"
#include "OMX_Component.h"

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE FP_OMX_ResourceManager_Init();
OMX_ERRORTYPE FP_OMX_ResourceManager_Deinit();
OMX_ERRORTYPE FP_OMX_Get_Resource(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE FP_OMX_Check_Resource(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE FP_OMX_Release_Resource(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE FP_OMX_In_WaitForResource(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE FP_OMX_Out_WaitForResource(OMX_COMPONENTTYPE *pOMXComponent);

extern char OMX_version[];

#ifdef __cplusplus
};
#endif

#endif /* _FOILPLANET_OMX_RESOURCEMANAGER_H_ */