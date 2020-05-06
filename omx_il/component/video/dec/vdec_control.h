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

#ifndef _FOILPLANET_OMX_VIDEO_DECODE_CONTROL_H_
#define _FOILPLANET_OMX_VIDEO_DECODE_CONTROL_H_

#include "OMX_Component.h"
#include "OMX_Def.h"
#include "OMX_VideoExt.h"

#include "Foilplanet_OMX_Baseport.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "library_register.h"

// #include "vpu_global.h"

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE FP_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer);

OMX_ERRORTYPE FP_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes);

OMX_ERRORTYPE FP_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_U32        nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr);

OMX_ERRORTYPE FP_OMX_AllocateTunnelBuffer(
    FP_OMX_BASEPORT  *pOMXBasePort,
    OMX_U32           nPortIndex);

OMX_ERRORTYPE FP_OMX_FreeTunnelBuffer(
    FP_OMX_BASEPORT  *pOMXBasePort,
    OMX_U32           nPortIndex);

OMX_ERRORTYPE FP_OMX_ComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32         nPort,
    OMX_IN OMX_HANDLETYPE  hTunneledComp,
    OMX_IN OMX_U32         nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup);

OMX_ERRORTYPE FPV_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure);

OMX_ERRORTYPE FPV_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure);

OMX_ERRORTYPE FPV_OMX_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE FPV_OMX_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE FP_OMX_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex);

OMX_ERRORTYPE FPV_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType);

OMX_ERRORTYPE FP_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, FP_OMX_DATABUFFER *dataBuffer);
OMX_ERRORTYPE FP_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, FP_OMX_DATABUFFER *dataBuffer);
OMX_ERRORTYPE FP_OMX_BufferFlush(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
OMX_ERRORTYPE FP_Frame2Outbuf(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* pOutputBuffer, VPU_FRAME *pframe);
OMX_ERRORTYPE FP_OutputBufferGetQueue(FP_OMX_BASECOMPONENT *pRockchipComponent);
OMX_ERRORTYPE FP_InputBufferGetQueue(FP_OMX_BASECOMPONENT *pRockchipComponent);
OMX_ERRORTYPE FP_ResolutionUpdate(OMX_COMPONENTTYPE *pOMXComponent);


#ifdef USE_ANB
OMX_ERRORTYPE FP_Shared_ANBBufferToData(FP_OMX_DATABUFFER *pUseBuffer, FP_OMX_DATA *pData, FP_OMX_BASEPORT *pFoilplanetPort, FP_OMX_PLANE nPlane);
OMX_ERRORTYPE FP_Shared_DataToANBBuffer(FP_OMX_DATA *pData, FP_OMX_DATABUFFER *pUseBuffer, FP_OMX_BASEPORT *pFoilplanetPort);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _FOILPLANET_OMX_VIDEO_DECODE_CONTROL_H_ */

