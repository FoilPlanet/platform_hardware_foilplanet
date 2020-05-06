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

#ifndef _OSAL_ANDROID_H_
#define _OSAL_ANDROID_H_

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Index.h"

#include "Foilplanet_OMX_Baseport.h"
#include "Foilplanet_OMX_Basecomponent.h"

#ifndef mpp_trace
# define mpp_trace          mpp_log
# define mpp_warn           mpp_log
#endif

//#include <hardware/gralloc.h>
//#include <system/graphics.h>

// defined in system/graphics.h
#define HAL_PIXEL_FORMAT_YCrCb_NV12             0x15    // YUY2
#define HAL_PIXEL_FORMAT_YCrCb_NV12_10          0x17    // YUY2_1obit

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FP_OMX_SHARED_BUFFER {
    OMX_S32         BufferFd;
    OMX_S32         BufferFd1;
    OMX_S32         BufferFd2;
    unsigned long  *pIonHandle;
    unsigned long  *pIonHandle1;
    unsigned long  *pIonHandle2;
    OMX_U32         cnt;
} FP_OMX_SHARED_BUFFER;

typedef struct _FP_OMX_REF_HANDLE {
    OMX_HANDLETYPE hMutex;
    FP_OMX_SHARED_BUFFER SharedBuffer[MAX_BUFFER_REF];
} FP_OMX_REF_HANDLE;

OMX_COLOR_FORMATTYPE OSAL_GetANBColorFormat(OMX_IN OMX_PTR handle);

OMX_U32 OSAL_GetANBStride(OMX_IN OMX_PTR handle);

OMX_ERRORTYPE OSAL_GetANBParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_INDEXTYPE nIndex,
                                   OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE OSAL_SetANBParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_INDEXTYPE nIndex,
                                   OMX_IN OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE OSAL_LockANB(OMX_IN OMX_PTR pBuffer,
                           OMX_IN OMX_U32 width,
                           OMX_IN OMX_U32 height,
                           OMX_IN OMX_COLOR_FORMATTYPE format,
                           OMX_OUT OMX_PTR planes);

OMX_ERRORTYPE OSAL_UnlockANB(OMX_IN OMX_PTR pBuffer);

OMX_ERRORTYPE OSAL_LockMetaData(OMX_IN OMX_PTR pBuffer,
                                OMX_IN OMX_U32 width,
                                OMX_IN OMX_U32 height,
                                OMX_IN OMX_COLOR_FORMATTYPE format,
                                OMX_OUT OMX_PTR planes);

OMX_ERRORTYPE OSAL_UnlockMetaData(OMX_IN OMX_PTR pBuffer);

OMX_ERRORTYPE OSAL_LockANBHandle(OMX_IN OMX_PTR pBuffer,
                                 OMX_IN OMX_U32 width,
                                 OMX_IN OMX_U32 height,
                                 OMX_IN OMX_COLOR_FORMATTYPE format,
                                 OMX_OUT OMX_PTR planes);

OMX_ERRORTYPE OSAL_UnlockANBHandle(OMX_IN OMX_PTR pBuffer);

OMX_ERRORTYPE OSAL_GetInfoFromMetaData(OMX_IN OMX_BYTE pBuffer,
                                       OMX_OUT OMX_PTR *pOutBuffer);

OMX_ERRORTYPE OSAL_GetInfoRkWfdMetaData(OMX_IN OMX_BOOL bRkWFD,
                                        OMX_IN OMX_BYTE pBuffer,
                                        OMX_OUT OMX_PTR *ppBuf);

OMX_ERRORTYPE OSAL_CheckANB(OMX_IN FP_OMX_DATA *pBuffer,
                            OMX_OUT OMX_BOOL *bIsANBEnabled);

OMX_ERRORTYPE OSAL_SetPrependSPSPPSToIDR(OMX_PTR pComponentParameterStructure,
                                         OMX_PTR pbPrependSpsPpsToIdr);

OMX_ERRORTYPE OSAL_CheckBuffType(OMX_U32 type);

OMX_COLOR_FORMATTYPE OSAL_Hal2OMXPixelFormat(unsigned int hal_format);

unsigned int OSAL_OMX2HalPixelFormat(OMX_COLOR_FORMATTYPE omx_format);

OMX_ERRORTYPE OSAL_Fd2VpumemPool(FP_OMX_BASECOMPONENT *pRockchipComponent,
                                 OMX_BUFFERHEADERTYPE* bufferHeader);

OMX_BUFFERHEADERTYPE *OSAL_Fd2OmxBufferHeader(FP_OMX_BASEPORT *pRockchipPort,
                                              OMX_IN OMX_S32 fd, OMX_IN OMX_PTR pVpumem);

OMX_ERRORTYPE  OSAL_FreeVpumem(OMX_IN OMX_PTR pVpumem);

OMX_ERRORTYPE  OSAL_Openvpumempool(OMX_IN FP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 portIndex);

OMX_ERRORTYPE  OSAL_Closevpumempool(OMX_IN FP_OMX_BASECOMPONENT *pRockchipComponent);

OMX_ERRORTYPE OSAL_ResetVpumemPool(OMX_IN FP_OMX_BASECOMPONENT *pRockchipComponent);

OMX_COLOR_FORMATTYPE OSAL_CheckFormat(FP_OMX_BASECOMPONENT *pRockchipComponent, OMX_IN OMX_PTR pVpuframe);

OMX_ERRORTYPE OSAL_getANBHandle(OMX_IN OMX_PTR handle, OMX_OUT OMX_PTR planes);

OMX_U32 Get_Video_HorAlign(OMX_VIDEO_CODINGTYPE codecId, OMX_U32 width, OMX_U32 height);

OMX_U32 Get_Video_VerAlign(OMX_VIDEO_CODINGTYPE codecId, OMX_U32 height);

OMX_ERRORTYPE OSAL_PowerControl(FP_OMX_BASECOMPONENT *pRockchipComponent,
                                int32_t width,
                                int32_t height,
                                int32_t mHevc,
                                int32_t frameRate,
                                OMX_BOOL mFlag,
                                int bitDepth);

#ifdef AVS80
OMX_U32 OSAL_GetVideoNativeMetaSize();
OMX_U32 OSAL_GetVideoGrallocMetaSize();
#endif

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_ANDROID_H_ */
