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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#include "OMX_Macros.h"
#include "OMX_Def.h"
#include "OMX_IndexExt.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "venc.h"
#include "venc_control.h"

#include <hardware/hardware.h>
#include "vpu.h"
#include "mpp_buffer.h"

//#include <hardware/rga.h>
//#include "csc.h"

#ifdef USE_ANB
#include "osal_android.h"
#endif

#include "osal_event.h"
#include "osal/mpp_list.h"
#include "osal/mpp_log.h"
#include "osal/mpp_mem.h"
#include "osal/mpp_thread.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG         "FP_OMX_VECTL"
#endif

#define GET_MPPLIST(q)      ((mpp_list *)q)

typedef struct {
    OMX_U32 mProfile;
    OMX_U32 mLevel;
} CodecProfileLevel;
static const CodecProfileLevel kM4VProfileLevels[] = {
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b},
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3 },
};
static const CodecProfileLevel kH263ProfileLevels[] = {
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level20 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level30 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level45 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level10 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level20 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level30 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level45 },
};
static const CodecProfileLevel kProfileLevels[] = {
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1b},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel11},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel12},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel13},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel2},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel21},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel3},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel31},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel32},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel4},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel41},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel42},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel5},
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1b},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel11},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel12},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel13},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel2},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel21},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel22},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel3},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel31},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel32},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel4},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel41},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel42},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel5},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51},
};

static const CodecProfileLevel kH265ProfileLevels[] = {
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel1  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel2  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel21 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel3  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel31 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel4  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel41 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel5  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel51 },
};

OMX_ERRORTYPE FP_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U32                i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[nPortIndex];
    if (nPortIndex >= pFpComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    if (pFoilplanetPort->portState != OMX_StateIdle) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    temp_bufferHeader = mpp_malloc(OMX_BUFFERHEADERTYPE, 1);
    if (temp_bufferHeader == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pFoilplanetPort->portDefinition.nBufferCountActual; i++) {
        if (pFoilplanetPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pFoilplanetPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            temp_bufferHeader->pBuffer        = pBuffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pFoilplanetPort->assignedBufferNum++;
            if (pFoilplanetPort->assignedBufferNum == pFoilplanetPort->portDefinition.nBufferCountActual) {
                pFoilplanetPort->portDefinition.bPopulated = OMX_TRUE;
                /* MUTEX_LOCK(pFoilplanetPort->compMutex); */
                OSAL_SemaphorePost(pFoilplanetPort->loadedResource);
                /* MUTEX_UNLOCK(pFoilplanetPort->compMutex); */
            }
            *ppBufferHdr = temp_bufferHeader;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    mpp_free(temp_bufferHeader);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT  *pFpComponent = NULL;
    FP_OMX_BASEPORT       *pFoilplanetPort = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U8                *temp_buffer = NULL;
    int                    temp_buffer_fd = -1;
    OMX_U32                i = 0;

    FunctionIn();

    mpp_err("FP_OMX_AllocateBuffer in");
    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[nPortIndex];
    if (nPortIndex >= pFpComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    /*
        if (pFoilplanetPort->portState != OMX_StateIdle ) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    */
    if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    temp_bufferHeader = mpp_malloc(OMX_BUFFERHEADERTYPE, 1);
    if (temp_bufferHeader == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    temp_buffer = mpp_malloc(OMX_U8, nSizeBytes);
    if (temp_buffer == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }


    for (i = 0; i < pFoilplanetPort->portDefinition.nBufferCountActual; i++) {
        if (pFoilplanetPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pFoilplanetPort->extendBufferHeader[i].buf_fd[0] = temp_buffer_fd;
            pFoilplanetPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
        #if 0
            if (mem_type == SECURE_MEMORY)
                temp_bufferHeader->pBuffer = temp_buffer_fd;
        #endif
            temp_bufferHeader->pBuffer     = temp_buffer;
            temp_bufferHeader->nAllocLen   = nSizeBytes;
            temp_bufferHeader->pAppPrivate = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;
            pFoilplanetPort->assignedBufferNum++;
            if (pFoilplanetPort->assignedBufferNum == pFoilplanetPort->portDefinition.nBufferCountActual) {
                pFoilplanetPort->portDefinition.bPopulated = OMX_TRUE;
                /* MUTEX_LOCK(pFpComponent->compMutex); */
                OSAL_SemaphorePost(pFoilplanetPort->loadedResource);
                /* MUTEX_UNLOCK(pFpComponent->compMutex); */
            }
            *ppBuffer = temp_bufferHeader;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    mpp_free(temp_bufferHeader);
    mpp_free(temp_buffer);

    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    mpp_err("FP_OMX_AllocateBuffer in ret = 0x%x", ret);
    return ret;
}

OMX_ERRORTYPE FP_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_U32        nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U8                *temp_buffer = NULL;
    OMX_U32                i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    pFoilplanetPort = &pFpComponent->pFoilplanetPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pFoilplanetPort->portState != OMX_StateLoaded) && (pFoilplanetPort->portState != OMX_StateInvalid)) {
        (*(pFpComponent->pCallbacks->EventHandler))(pOMXComponent,
                                                    pFpComponent->callbackData,
                                                    (OMX_EVENTTYPE)OMX_EventError,
                                                    (OMX_U32)OMX_ErrorPortUnpopulated,
                                                    nPortIndex, NULL);
    }

    for (i = 0; i < /*pFoilplanetPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if (((pFoilplanetPort->bufferStateAllocate[i] | BUFFER_STATE_FREE) != 0) && (pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            if (pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader->pBuffer == pBufferHdr->pBuffer) {
                if (pFoilplanetPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    mpp_free(pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader->pBuffer);
                    pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader->pBuffer = NULL;
                    pBufferHdr->pBuffer = NULL;
                } else if (pFoilplanetPort->bufferStateAllocate[i] & BUFFER_STATE_ASSIGNED) {
                    ; /* None*/
                }
                pFoilplanetPort->assignedBufferNum--;
                if (pFoilplanetPort->bufferStateAllocate[i] & HEADER_STATE_ALLOCATED) {
                    mpp_free(pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader);
                    pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader = NULL;
                    pBufferHdr = NULL;
                }
                pFoilplanetPort->bufferStateAllocate[i] = BUFFER_STATE_FREE;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pFoilplanetPort->assignedBufferNum == 0) {
            mpp_trace("pFoilplanetPort->unloadedResource signal set");
            /* MUTEX_LOCK(pFoilplanetPort->compMutex); */
            OSAL_SemaphorePost(pFoilplanetPort->unloadedResource);
            /* MUTEX_UNLOCK(pFoilplanetPort->compMutex); */
            pFoilplanetPort->portDefinition.bPopulated = OMX_FALSE;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_AllocateTunnelBuffer(FP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                 ret = OMX_ErrorNone;
    (void)pOMXBasePort;
    (void)nPortIndex;
    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE FP_OMX_FreeTunnelBuffer(FP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    (void)pOMXBasePort;
    (void)nPortIndex;
    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}
OMX_ERRORTYPE FP_OMX_ComponentTunnelRequest(
    OMX_IN OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32        nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32        nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    (void)hComp;
    (void)nPort;
    (void)hTunneledComp;
    (void)nTunneledPort;
    (void)pTunnelSetup;
    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE FP_OMX_GetFlushBuffer(FP_OMX_BASEPORT *pFoilplanetPort, FP_OMX_DATABUFFER *pDataBuffer[])
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    *pDataBuffer = NULL;

    if (pFoilplanetPort->portWayType == WAY1_PORT) {
        *pDataBuffer = &pFoilplanetPort->way.port1WayDataBuffer.dataBuffer;
    } else if (pFoilplanetPort->portWayType == WAY2_PORT) {
        pDataBuffer[0] = &(pFoilplanetPort->way.port2WayDataBuffer.inputDataBuffer);
        pDataBuffer[1] = &(pFoilplanetPort->way.port2WayDataBuffer.outputDataBuffer);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_FlushPort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_BUFFERHEADERTYPE *bufferHeader = NULL;
    FP_OMX_DATABUFFER    *pDataPortBuffer[2] = {NULL, NULL};
    FP_OMX_MESSAGE       *message = NULL;
    OMX_U32               flushNum = 0;
    OMX_S32               semValue = 0;
    int i = 0, maxBufferNum = 0;
    FunctionIn();

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

    while (GET_MPPLIST(pFoilplanetPort->bufferQ)->list_size() > 0) {
        OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[portIndex].bufferSemID, &semValue);
        if (semValue == 0)
            OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[portIndex].bufferSemID);

        OSAL_SemaphoreWait(pFpComponent->pFoilplanetPort[portIndex].bufferSemID);
        GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE));
        if ((message != NULL) && (message->messageType != FP_OMX_CommandFakeBuffer)) {
            bufferHeader = (OMX_BUFFERHEADERTYPE *)message->pCmdData;
            bufferHeader->nFilledLen = 0;

            if (portIndex == OUTPUT_PORT_INDEX) {
                FP_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
            } else if (portIndex == INPUT_PORT_INDEX) {
                FP_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
            }
        }
        mpp_free(message);
        message = NULL;
    }

    FP_OMX_GetFlushBuffer(pFoilplanetPort, pDataPortBuffer);
    if (portIndex == INPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            FP_InputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            FP_InputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    } else if (portIndex == OUTPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            FP_OutputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            FP_OutputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    }

    if (pFpComponent->bMultiThreadProcess == OMX_TRUE) {
        if (pFoilplanetPort->bufferProcessType == BUFFER_SHARE) {
            if (pFoilplanetPort->processData.bufferHeader != NULL) {
                if (portIndex == INPUT_PORT_INDEX) {
                    FP_OMX_InputBufferReturn(pOMXComponent, pFoilplanetPort->processData.bufferHeader);
                } else if (portIndex == OUTPUT_PORT_INDEX) {
                    FP_OMX_OutputBufferReturn(pOMXComponent, pFoilplanetPort->processData.bufferHeader);
                }
            }
            FP_ResetCodecData(&pFoilplanetPort->processData);

            maxBufferNum = pFoilplanetPort->portDefinition.nBufferCountActual;
            for (i = 0; i < maxBufferNum; i++) {
                if (pFoilplanetPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                    if (portIndex == OUTPUT_PORT_INDEX) {
                        FP_OMX_OutputBufferReturn(pOMXComponent, pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader);
                    } else if (portIndex == INPUT_PORT_INDEX) {
                        FP_OMX_InputBufferReturn(pOMXComponent, pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader);
                    }
                }
            }
        }
    } else {
        FP_ResetCodecData(&pFoilplanetPort->processData);
    }

    if ((pFoilplanetPort->bufferProcessType == BUFFER_SHARE) &&
        (portIndex == OUTPUT_PORT_INDEX)) {
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

        if (pOMXComponent->pComponentPrivate == NULL) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
        pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    }

    while (1) {
        OMX_S32 cnt = 0;
        OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[portIndex].bufferSemID, &cnt);
        if (cnt <= 0)
            break;
        OSAL_SemaphoreWait(pFpComponent->pFoilplanetPort[portIndex].bufferSemID);
    }

    // todo free node
    GET_MPPLIST(pFoilplanetPort->bufferQ)->flush();

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferFlush(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};
    OMX_U32                   i = 0, cnt = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

    mpp_trace("OMX_CommandFlush start, port:%d", nPortIndex);

    pFpComponent->pFoilplanetPort[nPortIndex].bIsPortFlushed = OMX_TRUE;

    if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
        OSAL_SignalSet(pFpComponent->pauseEvent);
    } else {
        OSAL_SignalSet(pFpComponent->pFoilplanetPort[nPortIndex].pauseEvent);
    }

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[nPortIndex];
    FP_OMX_GetFlushBuffer(pFoilplanetPort, flushPortBuffer);


    OSAL_SemaphorePost(pFoilplanetPort->bufferSemID);

    MUTEX_LOCK(flushPortBuffer[0]->bufferMutex);
    MUTEX_LOCK(flushPortBuffer[1]->bufferMutex);

    ret = FP_OMX_FlushPort(pOMXComponent, nPortIndex);

    FP_ResetCodecData(&pFoilplanetPort->processData);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            memset(pFpComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
            memset(pFpComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pFpComponent->getAllDelayBuffer = OMX_FALSE;
            pFpComponent->bSaveFlagEOS = OMX_FALSE;
            pFpComponent->bBehaviorEOS = OMX_FALSE;
            pFpComponent->reInputData = OMX_FALSE;
        }

        pFpComponent->pFoilplanetPort[nPortIndex].bIsPortFlushed = OMX_FALSE;
        mpp_trace("OMX_CommandFlush EventCmdComplete, port:%d", nPortIndex);
        if (bEvent == OMX_TRUE)
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandFlush, nPortIndex, NULL);
    }
    MUTEX_UNLOCK(flushPortBuffer[1]->bufferMutex);
    MUTEX_UNLOCK(flushPortBuffer[0]->bufferMutex);

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pFpComponent != NULL)) {
        mpp_err("ERROR");
        pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pFpComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_ResolutionUpdate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE              ret                = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent   = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc          = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pInputPort         = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pOutputPort        = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    pOutputPort->cropRectangle.nTop     = pOutputPort->newCropRectangle.nTop;
    pOutputPort->cropRectangle.nLeft    = pOutputPort->newCropRectangle.nLeft;
    pOutputPort->cropRectangle.nWidth   = pOutputPort->newCropRectangle.nWidth;
    pOutputPort->cropRectangle.nHeight  = pOutputPort->newCropRectangle.nHeight;

    pInputPort->portDefinition.format.video.nFrameWidth     = pInputPort->newPortDefinition.format.video.nFrameWidth;
    pInputPort->portDefinition.format.video.nFrameHeight    = pInputPort->newPortDefinition.format.video.nFrameHeight;
    pInputPort->portDefinition.format.video.nStride         = pInputPort->newPortDefinition.format.video.nStride;
    pInputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;

    pOutputPort->portDefinition.nBufferCountActual  = pOutputPort->newPortDefinition.nBufferCountActual;
    pOutputPort->portDefinition.nBufferCountMin     = pOutputPort->newPortDefinition.nBufferCountMin;

    UpdateFrameSize(pOMXComponent);

    return ret;
}

OMX_ERRORTYPE FP_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, FP_OMX_DATABUFFER *dataBuffer)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *rockchipOMXInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        if (rockchipOMXInputPort->markType.hMarkTargetComponent != NULL ) {
            bufferHeader->hMarkTargetComponent      = rockchipOMXInputPort->markType.hMarkTargetComponent;
            bufferHeader->pMarkData                 = rockchipOMXInputPort->markType.pMarkData;
            rockchipOMXInputPort->markType.hMarkTargetComponent = NULL;
            rockchipOMXInputPort->markType.pMarkData = NULL;
        }

        if (bufferHeader->hMarkTargetComponent != NULL) {
            if (bufferHeader->hMarkTargetComponent == pOMXComponent) {
                pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                             pFpComponent->callbackData,
                                                             OMX_EventMark,
                                                             0, 0, bufferHeader->pMarkData);
            } else {
                pFpComponent->propagateMarkType.hMarkTargetComponent = bufferHeader->hMarkTargetComponent;
                pFpComponent->propagateMarkType.pMarkData = bufferHeader->pMarkData;
            }
        }

        bufferHeader->nFilledLen = 0;
        bufferHeader->nOffset = 0;
        FP_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
    }

    /* reset dataBuffer */
    FP_ResetDataBuffer(dataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_InputBufferGetQueue(FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;
    FP_OMX_BASEPORT   *pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_MESSAGE    *message = NULL;
    FP_OMX_DATABUFFER *inputUseBuffer = NULL;

    FunctionIn();

    inputUseBuffer = &(pFoilplanetPort->way.port2WayDataBuffer.inputDataBuffer);

    if (pFpComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pFpComponent->transientState != FP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pFoilplanetPort))) {
        OSAL_SemaphoreWait(pFoilplanetPort->bufferSemID);
        if (inputUseBuffer->dataValid != OMX_TRUE) {
            if (GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE)) || message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->messageType == FP_OMX_CommandFakeBuffer) {
                mpp_free(message);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            inputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            inputUseBuffer->allocSize     = inputUseBuffer->bufferHeader->nAllocLen;
            inputUseBuffer->dataLen       = inputUseBuffer->bufferHeader->nFilledLen;
            inputUseBuffer->remainDataLen = inputUseBuffer->dataLen;
            inputUseBuffer->usedDataLen   = 0;
            inputUseBuffer->dataValid     = OMX_TRUE;
            inputUseBuffer->nFlags        = inputUseBuffer->bufferHeader->nFlags;
            inputUseBuffer->timeStamp     = inputUseBuffer->bufferHeader->nTimeStamp;

            mpp_free(message);

            if (inputUseBuffer->allocSize <= inputUseBuffer->dataLen)
                mpp_warn("Input Buffer Full, Check input buffer size! allocSize:%d, dataLen:%d", inputUseBuffer->allocSize, inputUseBuffer->dataLen);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, FP_OMX_DATABUFFER *dataBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *rockchipOMXOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        bufferHeader->nFilledLen = dataBuffer->remainDataLen;
        bufferHeader->nOffset    = 0;
        bufferHeader->nFlags     = dataBuffer->nFlags;
        bufferHeader->nTimeStamp = dataBuffer->timeStamp;

        if ((rockchipOMXOutputPort->bStoreMetaData == OMX_TRUE) && (bufferHeader->nFilledLen > 0))
            bufferHeader->nFilledLen = bufferHeader->nAllocLen;

        if (pFpComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            bufferHeader->hMarkTargetComponent = pFpComponent->propagateMarkType.hMarkTargetComponent;
            bufferHeader->pMarkData = pFpComponent->propagateMarkType.pMarkData;
            pFpComponent->propagateMarkType.hMarkTargetComponent = NULL;
            pFpComponent->propagateMarkType.pMarkData = NULL;
        }

        if ((bufferHeader->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_trace("event OMX_BUFFERFLAG_EOS!!!");
            pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventBufferFlag,
                                                         OUTPUT_PORT_INDEX,
                                                         bufferHeader->nFlags, NULL);
        }

        FP_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
    }

    /* reset dataBuffer */
    FP_ResetDataBuffer(dataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OutputBufferGetQueue(FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE      ret = OMX_ErrorUndefined;
    FP_OMX_BASEPORT   *pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_MESSAGE    *message = NULL;
    FP_OMX_DATABUFFER *outputUseBuffer = NULL;

    FunctionIn();

    outputUseBuffer = &(pFoilplanetPort->way.port2WayDataBuffer.outputDataBuffer);

    if (pFpComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pFpComponent->transientState != FP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pFoilplanetPort))) {
        OSAL_SemaphoreWait(pFoilplanetPort->bufferSemID);
        if (outputUseBuffer->dataValid != OMX_TRUE) {
            if (GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE)) || message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->messageType == FP_OMX_CommandFakeBuffer) {
                mpp_free(message);
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecFlush;
                goto EXIT;
            }

            outputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            outputUseBuffer->allocSize     = outputUseBuffer->bufferHeader->nAllocLen;
            outputUseBuffer->dataLen       = 0; //dataBuffer->bufferHeader->nFilledLen;
            outputUseBuffer->remainDataLen = outputUseBuffer->dataLen;
            outputUseBuffer->usedDataLen   = 0; //dataBuffer->bufferHeader->nOffset;
            outputUseBuffer->dataValid     = OMX_TRUE;
            mpp_free(message);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;

}

OMX_BUFFERHEADERTYPE *FP_OutputBufferGetQueue_Direct(FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_BUFFERHEADERTYPE  *retBuffer = NULL;
    FP_OMX_BASEPORT   *pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_MESSAGE    *message = NULL;

    FunctionIn();

    if (pFpComponent->currentState != OMX_StateExecuting) {
        retBuffer = NULL;
        goto EXIT;
    } else if ((pFpComponent->transientState != FP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pFoilplanetPort))) {
        OSAL_SemaphoreWait(pFoilplanetPort->bufferSemID);

        if (GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE)) || message == NULL) {
            retBuffer = NULL;
            goto EXIT;
        }
        if (message->messageType == FP_OMX_CommandFakeBuffer) {
            mpp_free(message);
            retBuffer = NULL;
            goto EXIT;
        }

        retBuffer  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
        mpp_free(message);
    }

EXIT:
    FunctionOut();

    return retBuffer;
}

OMX_ERRORTYPE FP_CodecBufferReset(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 PortIndex)
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;
    FP_OMX_BASEPORT   *pFoilplanetPort = NULL;

    FunctionIn();

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[PortIndex];

    // TODO: also clear nodes
    GET_MPPLIST(pFoilplanetPort->codecBufferQ)->flush();
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    while (1) {
        OMX_S32 cnt = 0;
        OSAL_Get_SemaphoreCount(pFoilplanetPort->codecSemID, &cnt);
        if (cnt > 0)
            OSAL_SemaphoreWait(pFoilplanetPort->codecSemID);
        else
            break;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FPV_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pFpComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch ((OMX_U32)nParamIndex) {
    case OMX_IndexParamVideoInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = FP_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        portParam->nPorts           = pFpComponent->portParam.nPorts;
        portParam->nStartPortNumber = pFpComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *portFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         portIndex = portFormat->nPortIndex;
        OMX_U32                         index    = portFormat->nIndex;
        FP_OMX_BASEPORT            *pFoilplanetPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;
        OMX_U32                         supportFormatNum = 0;

        ret = FP_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pFpComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }


        if (portIndex == INPUT_PORT_INDEX) {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
            portDefinition = &pFoilplanetPort->portDefinition;

            switch (index) {
            case supportFormat_0:
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat       = OMX_COLOR_FormatYUV420Planar;
                portFormat->xFramerate           = portDefinition->format.video.xFramerate;
                break;
            case supportFormat_1:
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                break;
            case supportFormat_2:
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat       = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque;
                portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                break;
            default:
                if (index > supportFormat_0) {
                    ret = OMX_ErrorNoMore;
                    goto EXIT;
                }
                break;
            }
        } else if (portIndex == OUTPUT_PORT_INDEX) {
            supportFormatNum = OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (index > supportFormatNum) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
            portDefinition = &pFoilplanetPort->portDefinition;

            portFormat->eCompressionFormat = portDefinition->format.video.eCompressionFormat;
            portFormat->eColorFormat       = portDefinition->format.video.eColorFormat;
            portFormat->xFramerate         = portDefinition->format.video.xFramerate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE *videoRateControl = (OMX_VIDEO_PARAM_BITRATETYPE *)ComponentParameterStructure;
        OMX_U32                      portIndex = videoRateControl->nPortIndex;
        FP_OMX_BASEPORT             *pFoilplanetPort = NULL;
        FP_OMX_VIDEOENC_COMPONENT   *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            portDefinition = &pFoilplanetPort->portDefinition;

            videoRateControl->eControlRate = pVideoEnc->eControlRate[portIndex];
            videoRateControl->nTargetBitrate = portDefinition->format.video.nBitrate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoQuantization: {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE  *videoQuantizationControl = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)ComponentParameterStructure;
        OMX_U32                            portIndex = videoQuantizationControl->nPortIndex;
        FP_OMX_BASEPORT               *pFoilplanetPort = NULL;
        FP_OMX_VIDEOENC_COMPONENT     *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE      *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            portDefinition = &pFoilplanetPort->portDefinition;

            videoQuantizationControl->nQpI = pVideoEnc->quantization.nQpI;
            videoQuantizationControl->nQpP = pVideoEnc->quantization.nQpP;
            videoQuantizationControl->nQpB = pVideoEnc->quantization.nQpB;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = portDefinition->nPortIndex;
        FP_OMX_BASEPORT          *pFoilplanetPort;

        if (portIndex >= pFpComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = FP_OMX_Check_SizeVersion(portDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
        memcpy(portDefinition, &pFoilplanetPort->portDefinition, portDefinition->nSize);

    }
    break;
    case OMX_IndexParamVideoIntraRefresh: {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)ComponentParameterStructure;
        OMX_U32                           portIndex = pIntraRefresh->nPortIndex;
        FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;

        if (portIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            pIntraRefresh->eRefreshMode = pVideoEnc->intraRefresh.eRefreshMode;
            pIntraRefresh->nAirMBs = pVideoEnc->intraRefresh.nAirMBs;
            pIntraRefresh->nAirRef = pVideoEnc->intraRefresh.nAirRef;
            pIntraRefresh->nCirMBs = pVideoEnc->intraRefresh.nCirMBs;
        }
        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParameterStructure;
        ret = FP_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
            strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_H264_ENC_ROLE);
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingVP8) {
            strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE);
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingHEVC) {
            strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_ENC_ROLE);
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(pDstAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pVideoEnc->AVCComponent[pDstAVCComponent->nPortIndex];
        memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }
    break;
    case OMX_IndexParamVideoHevc: {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHEVCComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHEVCComponent = NULL;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(pDstHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstHEVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHEVCComponent = &pVideoEnc->HEVCComponent[pDstHEVCComponent->nPortIndex];
        memcpy(pDstHEVCComponent, pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
    }
    break;
    case OMX_IndexParamVideoProfileLevelQuerySupported: {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) ComponentParameterStructure;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

        OMX_U32 index = profileLevel->nProfileIndex;
        OMX_U32 nProfileLevels = 0;
        if (profileLevel->nPortIndex  >= ALL_PORT_NUM) {
            mpp_err("Invalid port index: %ld", profileLevel->nPortIndex);
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
            nProfileLevels =
                sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
            if (index >= nProfileLevels) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            profileLevel->eProfile = kProfileLevels[index].mProfile;
            profileLevel->eLevel = kProfileLevels[index].mLevel;
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingHEVC) {
            nProfileLevels =
                sizeof(kH265ProfileLevels) / sizeof(kH265ProfileLevels[0]);
            if (index >= nProfileLevels) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            profileLevel->eProfile = kH265ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH265ProfileLevels[index].mLevel;
        } else {
            ret = OMX_ErrorNoMore;
            goto EXIT;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamRkEncExtendedVideo: {   // extern for huawei param setting
        OMX_VIDEO_PARAMS_EXTENDED  *params_extend = (OMX_VIDEO_PARAMS_EXTENDED *)ComponentParameterStructure;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        mpp_log("get OMX_IndexParamRkEncExtendedVideo in ");
        MUTEX_LOCK(pVideoEnc->bScale_Mutex);
        memcpy(params_extend, &pVideoEnc->params_extend, sizeof(OMX_VIDEO_PARAMS_EXTENDED));
        MUTEX_UNLOCK(pVideoEnc->bScale_Mutex);
    }
    break;


    default: {
        ret = FP_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    break;
    }

EXIT:
    FunctionOut();

    return ret;
}
OMX_ERRORTYPE FPV_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE    *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pFpComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch ((OMX_U32)nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *portFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         portIndex = portFormat->nPortIndex;
        OMX_U32                         index    = portFormat->nIndex;
        FP_OMX_BASEPORT                *pFoilplanetPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;
        OMX_U32                         supportFormatNum = 0;

        ret = FP_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pFpComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            portDefinition = &pFoilplanetPort->portDefinition;

            portDefinition->format.video.eColorFormat       = portFormat->eColorFormat;
            portDefinition->format.video.eCompressionFormat = portFormat->eCompressionFormat;
            portDefinition->format.video.xFramerate         = portFormat->xFramerate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE *videoRateControl = (OMX_VIDEO_PARAM_BITRATETYPE *)ComponentParameterStructure;
        OMX_U32                      portIndex = videoRateControl->nPortIndex;
        FP_OMX_BASEPORT             *pFoilplanetPort = NULL;
        FP_OMX_VIDEOENC_COMPONENT   *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE*portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            portDefinition = &pFoilplanetPort->portDefinition;

            pVideoEnc->eControlRate[portIndex] = videoRateControl->eControlRate;
            portDefinition->format.video.nBitrate = videoRateControl->nTargetBitrate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoQuantization: {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE *videoQuantizationControl = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = videoQuantizationControl->nPortIndex;
        FP_OMX_BASEPORT              *pFoilplanetPort = NULL;
        FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            portDefinition = &pFoilplanetPort->portDefinition;

            pVideoEnc->quantization.nQpI = videoQuantizationControl->nQpI;
            pVideoEnc->quantization.nQpP = videoQuantizationControl->nQpP;
            pVideoEnc->quantization.nQpB = videoQuantizationControl->nQpB;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = pPortDefinition->nPortIndex;
        FP_OMX_BASEPORT          *pFoilplanetPort;
        OMX_U32 width, height, size;

        if (portIndex >= pFpComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = FP_OMX_Check_SizeVersion(pPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

        if ((pFpComponent->currentState != OMX_StateLoaded) && (pFpComponent->currentState != OMX_StateWaitForResources)) {
            if (pFoilplanetPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }
        if (pPortDefinition->nBufferCountActual < pFoilplanetPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        memcpy(&pFoilplanetPort->portDefinition, pPortDefinition, pPortDefinition->nSize);
        if (portIndex == INPUT_PORT_INDEX) {
            FP_OMX_BASEPORT *pRockchipOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
            UpdateFrameSize(pOMXComponent);
            mpp_trace("pRockchipOutputPort->portDefinition.nBufferSize: %d",
                      pRockchipOutputPort->portDefinition.nBufferSize);
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoIntraRefresh: {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)ComponentParameterStructure;
        OMX_U32                           portIndex = pIntraRefresh->nPortIndex;
        FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;

        if (portIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
            if (pIntraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic) {
                pVideoEnc->intraRefresh.eRefreshMode = pIntraRefresh->eRefreshMode;
                pVideoEnc->intraRefresh.nCirMBs = pIntraRefresh->nCirMBs;
                mpp_trace("OMX_VIDEO_IntraRefreshCyclic Enable, nCirMBs: %d",
                          pVideoEnc->intraRefresh.nCirMBs);
            } else {
                ret = OMX_ErrorUnsupportedSetting;
                goto EXIT;
            }
        }
        ret = OMX_ErrorNone;
    }
    break;

#ifdef USE_STOREMETADATA
    case OMX_IndexParamStoreMetaDataBuffer: {
        ret = OSAL_SetANBParameter(hComponent, nIndex, ComponentParameterStructure);

    }
    break;
#endif
    case OMX_IndexParamPrependSPSPPSToIDR: {
#if 0
        RKON2_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (RKON2_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        PrependSPSPPSToIDRFramesParams *PrependParams =
            (PrependSPSPPSToIDRFramesParams*)ComponentParameterStructure;
        mpp_trace("OMX_IndexParamPrependSPSPPSToIDR set true");

        pVideoEnc->bPrependSpsPpsToIdr = PrependParams->bEnable;

        return OMX_ErrorNone;
#endif
        FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        ret = OSAL_SetPrependSPSPPSToIDR(ComponentParameterStructure, &pVideoEnc->bPrependSpsPpsToIdr);
    }
    break;

    case OMX_IndexRkEncExtendedWfdState: {
        FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        FP_OMX_WFD *pRkWFD = (FP_OMX_WFD*)ComponentParameterStructure;
        pVideoEnc->bRkWFD = pRkWFD->bEnable;
        mpp_trace("OMX_IndexRkEncExtendedWfdState set as:%d", pRkWFD->bEnable);
        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;

        ret = FP_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pFpComponent->currentState != OMX_StateLoaded) && (pFpComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_H264_ENC_ROLE)) {
            pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        } else if (!strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE)) {
            pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
        } else if (!strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_ENC_ROLE)) {
            pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else {
            ret = OMX_ErrorInvalidComponentName;
            goto EXIT;
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = NULL;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        ret = FP_OMX_Check_SizeVersion(pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pVideoEnc->AVCComponent[pSrcAVCComponent->nPortIndex];

        memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }

    break;
    case OMX_IndexParamVideoHevc: {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHEVCComponent = NULL;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHEVCComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)ComponentParameterStructure;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
        ret = FP_OMX_Check_SizeVersion(pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcHEVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHEVCComponent = &pVideoEnc->HEVCComponent[pSrcHEVCComponent->nPortIndex];
        memcpy(pDstHEVCComponent, pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
    }
    break;
    case OMX_IndexParamRkEncExtendedVideo: {   // extern for huawei param setting
        OMX_VIDEO_PARAMS_EXTENDED  *params_extend = (OMX_VIDEO_PARAMS_EXTENDED *)ComponentParameterStructure;
        FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

        mpp_log("OMX_IndexParamRkEncExtendedVideo in ");
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        MUTEX_LOCK(pVideoEnc->bScale_Mutex);
        memcpy(&pVideoEnc->params_extend, params_extend, sizeof(OMX_VIDEO_PARAMS_EXTENDED));
        mpp_log("OMX_IndexParamRkEncExtendedVideo in flags %d bEableCrop %d,cl %d cr %d ct %d cb %d, bScaling %d ScaleW %d ScaleH %d",
                pVideoEnc->params_extend.ui32Flags, pVideoEnc->params_extend.bEnableCropping, pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom, pVideoEnc->params_extend.bEnableScaling,
                pVideoEnc->params_extend.ui16ScaledWidth, pVideoEnc->params_extend.ui16ScaledHeight);
        MUTEX_UNLOCK(pVideoEnc->bScale_Mutex);
    }
    break;
    default: {
        ret = FP_OMX_SetParameter(hComponent, nIndex, ComponentParameterStructure);
    }
    break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FPV_OMX_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE         *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT      *pFpComponent = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }
    
    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

    switch (nIndex) {
    case OMX_IndexConfigVideoAVCIntraPeriod: {

        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32           portIndex = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pAVCIntraPeriod->nIDRPeriod = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames + 1;
            pAVCIntraPeriod->nPFrames = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames;
        }
    }
    break;
    case OMX_IndexConfigVideoBitrate: {
        OMX_VIDEO_CONFIG_BITRATETYPE *pEncodeBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       portIndex = pEncodeBitrate->nPortIndex;
        FP_OMX_BASEPORT          *pFoilplanetPort = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            pEncodeBitrate->nEncodeBitrate = pFoilplanetPort->portDefinition.format.video.nBitrate;
        }
    }
    break;
    case OMX_IndexConfigVideoFramerate: {
        OMX_CONFIG_FRAMERATETYPE *pFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   portIndex = pFramerate->nPortIndex;
        FP_OMX_BASEPORT      *pFoilplanetPort = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            pFramerate->xEncodeFramerate = pFoilplanetPort->portDefinition.format.video.xFramerate;
        }
    }
    break;
#ifdef AVS80
    case OMX_IndexParamRkDescribeColorAspects: {
        OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *pParam = (OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *)pComponentConfigStructure;
        OMX_U32           portIndex = pParam->nPortIndex;
        if (pParam->bRequestingDataSpace) {
            pParam->sAspects.mPrimaries = RangeUnspecified;
            pParam->sAspects.mRange = PrimariesUnspecified;
            pParam->sAspects.mTransfer = TransferUnspecified;
            pParam->sAspects.mMatrixCoeffs = MatrixUnspecified;
            return ret;//OMX_ErrorUnsupportedSetting;
        }
        if (pParam->bDataSpaceChanged == OMX_TRUE) {
            // If the dataspace says RGB, recommend 601-limited;
            // since that is the destination colorspace that C2D or Venus will convert to.
            if (pParam->nPixelFormat == HAL_PIXEL_FORMAT_RGBA_8888) {
                memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
                pParam->sAspects.mPrimaries = RangeUnspecified;
                pParam->sAspects.mRange = PrimariesUnspecified;
                pParam->sAspects.mTransfer = TransferUnspecified;
                pParam->sAspects.mMatrixCoeffs = MatrixUnspecified;
            } else {
                memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
            }
        } else {
            memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
        }
    }
    break;
#endif
    default:
        ret = FP_OMX_GetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FPV_OMX_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE              ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE         *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT      *pFpComponent = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

    switch ((OMX_U32)nIndex) {
    case OMX_IndexConfigVideoIntraPeriod: {
        OMX_U32 nPFrames = (*((OMX_U32 *)pComponentConfigStructure)) - 1;

        pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = nPFrames;

        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexConfigVideoAVCIntraPeriod: {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32           portIndex = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            if (pAVCIntraPeriod->nIDRPeriod == (pAVCIntraPeriod->nPFrames + 1))
                pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = pAVCIntraPeriod->nPFrames;
            else {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
    }
    break;
    case OMX_IndexConfigVideoBitrate: {
        OMX_VIDEO_CONFIG_BITRATETYPE *pEncodeBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       portIndex = pEncodeBitrate->nPortIndex;
        FP_OMX_BASEPORT          *pFoilplanetPort = NULL;
        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            pFoilplanetPort->portDefinition.format.video.nBitrate = pEncodeBitrate->nEncodeBitrate;
            if (p_vpu_ctx !=  NULL) {
                EncParameter_t vpug;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                vpug.bitRate = pEncodeBitrate->nEncodeBitrate;
                mpp_err("set bitRate %d", pEncodeBitrate->nEncodeBitrate);
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
            }
        }
    }
    break;
    case OMX_IndexConfigVideoFramerate: {
        OMX_CONFIG_FRAMERATETYPE *pFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   portIndex = pFramerate->nPortIndex;
        FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
            pFoilplanetPort->portDefinition.format.video.xFramerate = pFramerate->xEncodeFramerate;
        }

        if (p_vpu_ctx !=  NULL) {
            EncParameter_t vpug;
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
            vpug.framerate = (pFramerate->xEncodeFramerate >> 16);
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
        }
    }
    break;
    case OMX_IndexConfigVideoIntraVOPRefresh: {
        OMX_CONFIG_INTRAREFRESHVOPTYPE *pIntraRefreshVOP = (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentConfigStructure;
        OMX_U32 portIndex = pIntraRefreshVOP->nPortIndex;

        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc->IntraRefreshVOP = pIntraRefreshVOP->IntraRefreshVOP;
        }

        if (p_vpu_ctx !=  NULL && pVideoEnc->IntraRefreshVOP) {
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETIDRFRAME, NULL);
        }
    }
    break;
#ifdef AVS80
    case OMX_IndexParamRkDescribeColorAspects: {
        OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *params = (OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *)pComponentConfigStructure;
        memcpy(&pVideoEnc->ConfigColorAspects, pComponentConfigStructure, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
    }
    break;
#endif
    default:
        ret = FP_OMX_SetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    FP_OMX_BASECOMPONENT *pRockchioComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nIndex == 0) {
        strcpy((char *)cRole, RK_OMX_COMPONENT_H264_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 1) {
        strcpy((char *)cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }
EXIT:
    FunctionOut();

    return ret;
}


OMX_ERRORTYPE FPV_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

    FunctionIn();
    mpp_trace("cParameterName:%s", cParameterName);

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if ((cParameterName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (strcmp(cParameterName, FOILPLANET_INDEX_CONFIG_VIDEO_INTRAPERIOD) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoIntraPeriod;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamPrependSPSPPSToIDR;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (!strcmp(cParameterName, FOILPLANET_INDEX_PARAM_RKWFD)) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexRkEncExtendedWfdState;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_EXTENDED_VIDEO) == 0) {

        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkEncExtendedVideo;
        ret = OMX_ErrorNone;
    }
#ifdef AVS80
    else if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_DSECRIBECOLORASPECTS) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDescribeColorAspects;
        goto EXIT;
    }
#endif
#ifdef USE_STOREMETADATA
    else if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamStoreMetaDataBuffer;
        goto EXIT;
    } else {
        ret = FP_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);
    }
#else
    else {
        ret = FP_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);
    }
#endif

EXIT:
    FunctionOut();
    return ret;
}
