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
#include <sys/ioctl.h>
#include <unistd.h>

#include "OMX_Macros.h"
#include "OMX_Def.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "vdec.h"
#include "vdec_control.h"

#include <hardware/hardware.h>
#include "vpu.h"
#include "mpp_buffer.h"

//#include <hardware/rga.h>
//#include <vpu_mem_pool.h>
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
# define MODULE_TAG         "FP_OMX_VDCTL"
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
static const CodecProfileLevel kH264ProfileLevels[] = {
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
    { OMX_VIDEO_AVCProfileHigh10, OMX_VIDEO_AVCLevel52},
};

//only report echo profile highest level, Reference soft avc dec
static const CodecProfileLevel kH264ProfileLevelsMax[] = {
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51},
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
    { OMX_VIDEO_HEVCProfileMain10, OMX_VIDEO_HEVCMainTierLevel51 },
};


OMX_ERRORTYPE FP_OMX_UseBuffer(
    OMX_IN    OMX_HANDLETYPE         hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN    OMX_U32                nPortIndex,
    OMX_IN    OMX_PTR                pAppPrivate,
    OMX_IN    OMX_U32                nSizeBytes,
    OMX_IN    OMX_U8                *pBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT  *pFpComponent = NULL;
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
            pFoilplanetPort->extendBufferHeader[i].pRegisterFlag = 0;
            pFoilplanetPort->extendBufferHeader[i].pPrivate = NULL;
            pFoilplanetPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            temp_bufferHeader->pBuffer        = pBuffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else {
                mpp_log("bufferHeader[%d] = 0x%x ", i, temp_bufferHeader);
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;
            }

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

    mpp_log("FP_OMX_UseBuffer in ret = 0x%x", ret);
    return ret;
}

OMX_ERRORTYPE FP_OMX_AllocateBuffer(
    OMX_IN    OMX_HANDLETYPE         hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN    OMX_U32                nPortIndex,
    OMX_IN    OMX_PTR                pAppPrivate,
    OMX_IN    OMX_U32                nSizeBytes)
{
    OMX_ERRORTYPE               ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE          *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT       *pFpComponent = NULL;
    FP_OMX_VIDEODEC_COMPONENT  *pVideoDec = NULL;
    FP_OMX_BASEPORT            *pFoilplanetPort = NULL;
    OMX_BUFFERHEADERTYPE       *temp_bufferHeader = NULL;
    OMX_U8                     *temp_buffer = NULL;
    int                         temp_buffer_fd = -1;
    OMX_U32                     i = 0;

    FunctionIn();

    mpp_log("FP_OMX_AllocateBuffer in");
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
    pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

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

#if 0
    MEMORY_TYPE mem_type = NORMAL_MEMORY;
    if ((pVideoDec->bDRMPlayerMode == OMX_TRUE) && (nPortIndex == INPUT_PORT_INDEX)) {
        mem_type = SECURE_MEMORY;
    } else if (pFoilplanetPort->bufferProcessType == BUFFER_SHARE) {
        mem_type = NORMAL_MEMORY;
    } else {
        mem_type = SYSTEM_MEMORY;
    }
#endif

    temp_bufferHeader = mpp_malloc(OMX_BUFFERHEADERTYPE, 1);
    if (temp_bufferHeader == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
        MppBuffer mppbuf = NULL;
        if (MPP_OK == mpp_buffer_get(NULL, &mppbuf, nSizeBytes)) {
            temp_buffer = (OMX_U8 *)mpp_buffer_get_ptr(mppbuf);
            temp_bufferHeader->pPlatformPrivate = mppbuf;
        }
    } 
    
    if (temp_buffer == NULL) {
        temp_buffer = mpp_malloc(OMX_U8, nSizeBytes);
        if (temp_buffer == NULL) {
            ret = OMX_ErrorInsufficientResources;
            mpp_free(temp_bufferHeader);
            goto EXIT;
        }
    }

    for (i = 0; i < pFoilplanetPort->portDefinition.nBufferCountActual; i++) {
        if (pFoilplanetPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pFoilplanetPort->extendBufferHeader[i].buf_fd[0] = temp_buffer_fd;
            pFoilplanetPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            temp_bufferHeader->pBuffer        = temp_buffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
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
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
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
    pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    pFoilplanetPort = &pFpComponent->pFoilplanetPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pFoilplanetPort->portState != OMX_StateLoaded) && (pFoilplanetPort->portState != OMX_StateInvalid)) {
        (*(pFpComponent->pCallbacks->EventHandler))(
            pOMXComponent,
            pFpComponent->callbackData,
            OMX_EventError,
            (OMX_U32)OMX_ErrorPortUnpopulated,
            nPortIndex, NULL);
    }

    for (i = 0; i < /*pFoilplanetPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if (((pFoilplanetPort->bufferStateAllocate[i] | BUFFER_STATE_FREE) != 0) && (pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            if (pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader->pBuffer == pBufferHdr->pBuffer) {
                if (pFoilplanetPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    if (pVideoDec->bDRMPlayerMode != 1)
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
            mpp_log("pFoilplanetPort->unloadedResource signal set");
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
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    (void) pOMXBasePort;
    (void) nPortIndex;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE FP_OMX_FreeTunnelBuffer(FP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                 ret = OMX_ErrorNone;
    (void) pOMXBasePort;
    (void) nPortIndex;
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
    (void) hComp;
    (void) nPort;
    (void) hTunneledComp;
    (void) nTunneledPort;
    (void) pTunnelSetup;
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
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
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
                pFoilplanetPort->extendBufferHeader[i].pRegisterFlag = 0;
                pFoilplanetPort->extendBufferHeader[i].buf_fd[0] = 0;
                if (pFoilplanetPort->extendBufferHeader[i].pPrivate != NULL) {
                    OSAL_FreeVpumem(pFoilplanetPort->extendBufferHeader[i].pPrivate);
                    pFoilplanetPort->extendBufferHeader[i].pPrivate = NULL;
                }

                if (pFoilplanetPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                    if (portIndex == OUTPUT_PORT_INDEX) {
                        FP_OMX_OutputBufferReturn(pOMXComponent, pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader);
                    } else if (portIndex == INPUT_PORT_INDEX) {
                        FP_OMX_InputBufferReturn(pOMXComponent, pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader);
                    }
                }
            }
            OSAL_ResetVpumemPool(pFpComponent);
        }
    } else {
        FP_ResetCodecData(&pFoilplanetPort->processData);
    }

    if ((pFoilplanetPort->bufferProcessType == BUFFER_SHARE) &&
        (portIndex == OUTPUT_PORT_INDEX)) {
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
        if (pOMXComponent->pComponentPrivate == NULL) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
        pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    }

    while (1) {
        OMX_S32 cnt = 0;
        OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[portIndex].bufferSemID, &cnt);
        if (cnt <= 0)
            break;
        OSAL_SemaphoreWait(pFpComponent->pFoilplanetPort[portIndex].bufferSemID);
    }

    // node_destructor ?
    GET_MPPLIST(pFoilplanetPort->bufferQ)->flush();

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferFlush(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};
    OMX_U32               i = 0, cnt = 0;
    FP_OMX_BASEPORT      *pInputPort  = NULL;
    VpuCodecContext_t    *p_vpu_ctx = NULL;

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
    pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    pInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];

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

    p_vpu_ctx = pVideoDec->vpu_ctx;
    if (pFpComponent->nRkFlags & RK_VPU_NEED_FLUSH_ON_SEEK) {
        p_vpu_ctx->flush(p_vpu_ctx);
        pFpComponent->nRkFlags &= ~RK_VPU_NEED_FLUSH_ON_SEEK;
        MUTEX_LOCK(pInputPort->secureBufferMutex);
        pVideoDec->invalidCount = 0;
        MUTEX_UNLOCK(pInputPort->secureBufferMutex);
    }

    mpp_log("OMX_CommandFlush start, port:%d", nPortIndex);
    FP_ResetCodecData(&pFoilplanetPort->processData);

    MUTEX_LOCK(pInputPort->secureBufferMutex);
    if (pVideoDec->bDRMPlayerMode == OMX_TRUE && pVideoDec->bInfoChange == OMX_FALSE) {
        int securebufferNum = GET_MPPLIST(pInputPort->securebufferQ)->list_size();
        mpp_log("FP_OMX_BufferFlush in securebufferNum = %d", securebufferNum);
        while (securebufferNum != 0) {
            FP_OMX_DATABUFFER *securebuffer = NULL;
            GET_MPPLIST(pInputPort->securebufferQ)->del_at_head(&securebuffer, sizeof(FP_OMX_DATABUFFER));
            FP_InputBufferReturn(pOMXComponent, securebuffer);
            mpp_free(securebuffer);
            securebufferNum = GET_MPPLIST(pInputPort->securebufferQ)->list_size();
        }
        mpp_log("FP_OMX_BufferFlush out securebufferNum = %d", securebufferNum);
    }
    MUTEX_UNLOCK(pInputPort->secureBufferMutex);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            memset(pFpComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
            memset(pFpComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pFpComponent->getAllDelayBuffer = OMX_FALSE;
            pFpComponent->bSaveFlagEOS = OMX_FALSE;
            pFpComponent->bBehaviorEOS = OMX_FALSE;
            pVideoDec->bDecSendEOS = OMX_FALSE;
            pFpComponent->reInputData = OMX_FALSE;
        }

        pFpComponent->pFoilplanetPort[nPortIndex].bIsPortFlushed = OMX_FALSE;
        mpp_log("OMX_CommandFlush EventCmdComplete, port:%d", nPortIndex);
        if (bEvent == OMX_TRUE)
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandFlush, nPortIndex, NULL);
    }

    if (pVideoDec->bInfoChange == OMX_TRUE)
        pVideoDec->bInfoChange = OMX_FALSE;

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
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent   = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec          = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pInputPort         = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pOutputPort        = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    pOutputPort->cropRectangle.nTop     = pOutputPort->newCropRectangle.nTop;
    pOutputPort->cropRectangle.nLeft    = pOutputPort->newCropRectangle.nLeft;
    pOutputPort->cropRectangle.nWidth   = pOutputPort->newCropRectangle.nWidth;
    pOutputPort->cropRectangle.nHeight  = pOutputPort->newCropRectangle.nHeight;

    pInputPort->portDefinition.format.video.nFrameWidth     = pInputPort->newPortDefinition.format.video.nFrameWidth;
    pInputPort->portDefinition.format.video.nFrameHeight    = pInputPort->newPortDefinition.format.video.nFrameHeight;
    pInputPort->portDefinition.format.video.nStride         = pInputPort->newPortDefinition.format.video.nStride;
    pOutputPort->portDefinition.format.video.nStride        = pInputPort->newPortDefinition.format.video.nStride;
    pInputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;
    pOutputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;
    pOutputPort->portDefinition.format.video.eColorFormat    = pOutputPort->newPortDefinition.format.video.eColorFormat;

    pOutputPort->portDefinition.nBufferCountActual  = pOutputPort->newPortDefinition.nBufferCountActual;
    pOutputPort->portDefinition.nBufferCountMin     = pOutputPort->newPortDefinition.nBufferCountMin;

    UpdateFrameSize(pOMXComponent);

    return ret;
}

OMX_ERRORTYPE FP_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, FP_OMX_DATABUFFER *dataBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *fpOMXInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        if (fpOMXInputPort->markType.hMarkTargetComponent != NULL ) {
            bufferHeader->hMarkTargetComponent      = fpOMXInputPort->markType.hMarkTargetComponent;
            bufferHeader->pMarkData                 = fpOMXInputPort->markType.pMarkData;
            fpOMXInputPort->markType.hMarkTargetComponent = NULL;
            fpOMXInputPort->markType.pMarkData = NULL;
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
        mpp_log("FP_OMX_InputBufferReturn in");
        FP_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
    }

    /* reset dataBuffer */
    FP_ResetDataBuffer(dataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE  FP_Frame2Outbuf(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* pOutputBuffer, VPU_FRAME *pframe)
{

    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent   = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec          = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pOutputPort        = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

#ifdef USE_STOREMETADATA
    if (pVideoDec->bStoreMetaData == OMX_TRUE) {
        OMX_U32 mWidth  = pOutputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 mHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
        FoilplanetVideoPlane vplanes;
        OMX_PTR pGrallocHandle;
        OMX_COLOR_FORMATTYPE omx_format = OMX_COLOR_FormatUnused;
        OMX_U32 pixel_format = 0;
        int res;

        if (OSAL_GetInfoFromMetaData(pOutputBuffer->pBuffer, &pGrallocHandle)) {
            return OMX_ErrorBadParameter;
        }

        if (pVideoDec->bPvr_Flag == OMX_TRUE) {
            pixel_format = RK_FORMAT_BGRA_8888;
        } else {
            omx_format = OSAL_GetANBColorFormat(pGrallocHandle);
            pixel_format = OSAL_OMX2HalPixelFormat(omx_format);
            if (pixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                pixel_format = RK_FORMAT_RGBA_8888;
            } else {
                pixel_format = RK_FORMAT_BGRA_8888;
            }
        }
        if (pVideoDec->rga_ctx != NULL) {
            OSAL_LockANB(pGrallocHandle, mWidth, mHeight, omx_format, &vplanes);
            VPUMemLink(&pframe->vpumem);
            rga_nv122rgb(&vplanes, &pframe->vpumem, mWidth, mHeight, pixel_format, pVideoDec->rga_ctx);
            VPUFreeLinear(&pframe->vpumem);
            OSAL_UnlockANB(pGrallocHandle);
        }
        return ret;
    }
#endif

#ifdef USE_ANB
    if (pVideoDec->bIsANBEnabled == OMX_TRUE) {
        mpp_log("enableNativeBuffer");
        OMX_U32 mWidth = pOutputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 mHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
        FoilplanetVideoPlane vplanes;
        OMX_U32 mStride = 0;
        OMX_U32 mSliceHeight =  0;
        OMX_COLOR_FORMATTYPE omx_format = OMX_COLOR_FormatUnused;
        OMX_U32 pixel_format = 0;
        mStride = Get_Video_HorAlign(pVideoDec->codecId, mWidth, mHeight);
        mSliceHeight = Get_Video_VerAlign(pVideoDec->codecId, mHeight);
        omx_format = OSAL_GetANBColorFormat(pOutputBuffer->pBuffer);
        pixel_format = OSAL_OMX2HalPixelFormat(omx_format);
        OSAL_LockANB(pOutputBuffer->pBuffer, mWidth, mHeight, omx_format, &vplanes);
        {
            VPUMemLink(&pframe->vpumem);
            VPUMemInvalidate(&pframe->vpumem);
            {
                OMX_U8 * buff_vir = (OMX_U8 *)pframe->vpumem.vir_addr;
                pOutputBuffer->nFilledLen = mWidth * mHeight * 3 / 2;
                OMX_U32 uv_offset = mStride * mSliceHeight;
                OMX_U32 y_size = mWidth * mHeight;
                OMX_U32 uv_size = mWidth * mHeight / 2;
                OMX_U8 *dst_uv = (OMX_U8 *)((OMX_U8 *)vplanes.addr + y_size);
                OMX_U8 *src_uv =  (OMX_U8 *)(buff_vir + uv_offset);
                OMX_U32 i = 0, j = 0;

                mpp_log("mWidth = %d mHeight = %d mStride = %d,mSlicHeight %d", mWidth, mHeight, mStride, mSliceHeight);
                for (i = 0; i < mHeight; i++) {
                    memcpy((char*)vplanes.addr + i * mWidth, buff_vir + i * mStride, mWidth);
                }

                for (i = 0; i < mHeight / 2; i++) {
                    memcpy((OMX_U8*)dst_uv, (OMX_U8*)src_uv, mWidth);
                    dst_uv += mWidth;
                    src_uv += mStride;
                }
            }
            VPUFreeLinear(&pframe->vpumem);
        }
        OSAL_UnlockANB(pOutputBuffer->pBuffer);
        return ret;
    }
#endif
    FP_OMX_BASEPORT *pFpOutputPort =  &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    OMX_U32 mStride = 0;
    OMX_U32 mSliceHeight =  0;
    OMX_U32 mWidth = (pOutputPort->portDefinition.format.video.nFrameWidth );
    OMX_U32 mHeight =  (pOutputPort->portDefinition.format.video.nFrameHeight );
    VPUMemLink(&pframe->vpumem);
    VPUMemInvalidate(&pframe->vpumem);

    mpp_log("width:%d,height:%d ", mWidth, mHeight);
    mStride = Get_Video_HorAlign(pVideoDec->codecId, mWidth, mHeight);
    mSliceHeight = Get_Video_VerAlign(pVideoDec->codecId, mHeight);
    {
        //csy@rock-chips.com
        OMX_U8 *buff_vir = (OMX_U8 *)pframe->vpumem.vir_addr;
        OMX_U32 uv_offset = mStride * mSliceHeight;
        OMX_U32 y_size = mWidth * mHeight;
        OMX_U32 uv_size = mWidth * mHeight / 2;
        OMX_U8 *dst_uv = (OMX_U8 *)(pOutputBuffer->pBuffer + y_size);
        OMX_U8 *src_uv =  (OMX_U8 *)(buff_vir + uv_offset);
        OMX_U32 i = 0, j = 0;
        mpp_err("mWidth = %d mHeight = %d mStride = %d,mSlicHeight %d", mWidth, mHeight, mStride, mSliceHeight);
        pOutputBuffer->nFilledLen = mWidth * mHeight * 3 / 2;
        for (i = 0; i < mHeight; i++) {
            memcpy((char*)pOutputBuffer->pBuffer + i * mWidth, buff_vir + i * mStride, mWidth);
        }

        for (i = 0; i < mHeight / 2; i++) {
            memcpy((OMX_U8*)dst_uv, (OMX_U8*)src_uv, mWidth);
            dst_uv += mWidth;
            src_uv += mStride;
        }
#if 0

        if (pVideoDec->fp_out != NULL) {
            fwrite(pOutputBuffer->pBuffer, 1, (mWidth * mHeight) * 3 / 2, pVideoDec->fp_out);
            fflush(pVideoDec->fp_out);
        }
#endif
    }
    VPUFreeLinear(&pframe->vpumem);

    return ret;

}

OMX_ERRORTYPE FP_InputBufferGetQueue(FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE      ret = OMX_ErrorUndefined;
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
            mpp_log("input buffer count = %d", GET_MPPLIST(pFoilplanetPort->bufferQ)->list_size());
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
                mpp_log("Input Buffer Full, Check input buffer size! allocSize:%d, dataLen:%d", inputUseBuffer->allocSize, inputUseBuffer->dataLen);
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
    FP_OMX_BASEPORT      *fpOMXOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        bufferHeader->nFilledLen = dataBuffer->remainDataLen;
        bufferHeader->nOffset    = 0;
        bufferHeader->nFlags     = dataBuffer->nFlags;
        bufferHeader->nTimeStamp = dataBuffer->timeStamp;

        if ((fpOMXOutputPort->bStoreMetaData == OMX_TRUE) && (bufferHeader->nFilledLen > 0))
            bufferHeader->nFilledLen = bufferHeader->nAllocLen;
        if (pFpComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            bufferHeader->hMarkTargetComponent = pFpComponent->propagateMarkType.hMarkTargetComponent;
            bufferHeader->pMarkData = pFpComponent->propagateMarkType.pMarkData;
            pFpComponent->propagateMarkType.hMarkTargetComponent = NULL;
            pFpComponent->propagateMarkType.pMarkData = NULL;
        }

        if ((bufferHeader->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_err("event OMX_BUFFERFLAG_EOS!!!");
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

    // TODO: node_destructor ?
    if (GET_MPPLIST(pFoilplanetPort->codecBufferQ)->flush() != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    while (1) {
        OMX_S32 cnt = 0;
        OSAL_Get_SemaphoreCount(pFoilplanetPort->codecSemID, (OMX_S32*)&cnt);
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
        FP_OMX_BASEPORT               *pFoilplanetPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;
        OMX_U32                         supportFormatNum = 0; /* supportFormatNum = N-1 */
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pFpComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }


        if (portIndex == INPUT_PORT_INDEX) {
            supportFormatNum = INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (index > supportFormatNum) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
            portDefinition = &pFoilplanetPort->portDefinition;

            portFormat->eCompressionFormat = portDefinition->format.video.eCompressionFormat;
            portFormat->eColorFormat       = portDefinition->format.video.eColorFormat;
            portFormat->xFramerate           = portDefinition->format.video.xFramerate;
        } else if (portIndex == OUTPUT_PORT_INDEX) {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
            portDefinition = &pFoilplanetPort->portDefinition;

            if (pFoilplanetPort->bStoreMetaData == OMX_FALSE) {
                switch (index) {
                case supportFormat_0:
                    portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                    break;
                case supportFormat_1:
                    if (pVideoDec->codecId != OMX_VIDEO_CodingVP8) {
                        portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                        portFormat->eColorFormat       = OMX_COLOR_FormatYUV420Planar;
                        portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                    }
                    break;
                default:
                    if (index > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            } else {
                switch (index) {
                case supportFormat_0:
                    portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                    break;
                default:
                    if (index > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            }
        }
        ret = OMX_ErrorNone;
    }
    break;
#ifdef USE_ANB
    case OMX_IndexParamGetAndroidNativeBufferUsage:
    case OMX_IndexParamdescribeColorFormat: {

        mpp_log("OSAL_GetANBParameter!!");
        ret = OSAL_GetANBParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    break;
    case OMX_IndexParamPortDefinition: {
        FP_OMX_BASEPORT *pFoilplanetPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = portDefinition->nPortIndex;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

        ret = FP_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        /* at this point, GetParameter has done all the verification, we
         * just dereference things directly here
         */
        FP_OMX_BASEPORT *pFpOutputPort =  &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
        if ((pVideoDec->bIsANBEnabled == OMX_TRUE) ||
            (pVideoDec->bStoreMetaData == OMX_TRUE)) {
            portDefinition->format.video.eColorFormat = pFoilplanetPort->portDefinition.format.video.eColorFormat;
            // (OMX_COLOR_FORMATTYPE)OSAL_OMX2HalPixelFormat(portDefinition->format.video.eColorFormat);
            mpp_log("portDefinition->format.video.eColorFormat:0x%x", portDefinition->format.video.eColorFormat);
        }
        if (portIndex == OUTPUT_PORT_INDEX &&
            pFoilplanetPort->bufferProcessType != BUFFER_SHARE) {
            portDefinition->format.video.nStride = portDefinition->format.video.nFrameWidth;
            portDefinition->format.video.nSliceHeight = portDefinition->format.video.nFrameHeight;
        }
#ifdef AVS80
        if (portIndex == OUTPUT_PORT_INDEX &&
            pFoilplanetPort->bufferProcessType == BUFFER_SHARE) {
            portDefinition->format.video.nFrameWidth = portDefinition->format.video.nStride;
            portDefinition->format.video.nFrameHeight = portDefinition->format.video.nSliceHeight;
        }
#endif

    }
    break;
#endif

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParameterStructure;
        ret = FP_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_H264_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMPEG4) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_MPEG4_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingH263) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_H263_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMPEG2) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_MPEG2_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingVP8) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_VP8_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_HEVC_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_FLV_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_VP6_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMJPEG) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_MJPEG_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_VC1_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingWMV) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_WMV3_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingRV) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_RMVB_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingVP9) {
            strcpy((char *)pComponentRole->cRole, FP_OMX_COMPONENT_VP9_DEC_ROLE);
        }

    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(pDstAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pVideoDec->AVCComponent[pDstAVCComponent->nPortIndex];
        memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }
    break;
    case OMX_IndexParamVideoProfileLevelQuerySupported: {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        OMX_U32 index = profileLevel->nProfileIndex;
        OMX_U32 nProfileLevels = 0;
        if (profileLevel->nPortIndex != 0) {
            mpp_err("Invalid port index: %ld", profileLevel->nPortIndex);
            return OMX_ErrorUnsupportedIndex;
        }
        if (pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
            nProfileLevels =
                sizeof(kH264ProfileLevelsMax) / sizeof(kH264ProfileLevelsMax[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH264ProfileLevelsMax[index].mProfile;
            profileLevel->eLevel = kH264ProfileLevelsMax[index].mLevel;
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
            nProfileLevels =
                sizeof(kH265ProfileLevels) / sizeof(kH265ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH265ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH265ProfileLevels[index].mLevel;
        } else if (pVideoDec->codecId  == OMX_VIDEO_CodingMPEG4) {
            nProfileLevels =
                sizeof(kM4VProfileLevels) / sizeof(kM4VProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kM4VProfileLevels[index].mProfile;
            profileLevel->eLevel = kM4VProfileLevels[index].mLevel;
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingH263) {
            nProfileLevels =
                sizeof(kH263ProfileLevels) / sizeof(kH263ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH263ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH263ProfileLevels[index].mLevel;
        } else {
            return OMX_ErrorNoMore;
        }
        return OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoHDRRockchipExtensions: {
        OMX_EXTENSION_VIDEO_PARAM_HDR *hdrParams =
            (OMX_EXTENSION_VIDEO_PARAM_HDR *) ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(hdrParams, sizeof(OMX_EXTENSION_VIDEO_PARAM_HDR));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        hdrParams->eColorSpace = pVideoDec->extColorSpace;
        hdrParams->eDyncRange = pVideoDec->extDyncRange;
        ret = OMX_ErrorNone;
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

    switch ((OMX_U32)nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *portFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         portIndex = portFormat->nPortIndex;
        OMX_U32                         index    = portFormat->nIndex;
        FP_OMX_BASEPORT               *pFoilplanetPort = NULL;
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

            mpp_log("portIndex:%d, portFormat->eColorFormat:0x%x", portIndex, portFormat->eColorFormat);
        }
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = pPortDefinition->nPortIndex;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
        FP_OMX_BASEPORT             *pFoilplanetPort;
        OMX_U32 stride, strideheight, size;
        OMX_U32 realWidth, realHeight;
        OMX_U32 supWidth = 0;

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


        realWidth = pFoilplanetPort->portDefinition.format.video.nFrameWidth;
        realHeight = pFoilplanetPort->portDefinition.format.video.nFrameHeight;

        stride = Get_Video_HorAlign(pVideoDec->codecId, realWidth, realHeight);
        strideheight = Get_Video_VerAlign(pVideoDec->codecId, realHeight);

        size = (stride * strideheight * 3) / 2;

        supWidth = VPUCheckSupportWidth();
        if (supWidth == 0) {
            mpp_log("VPUCheckSupportWidth is failed , force max width to 4096.");
            supWidth = 4096;
        }
        mpp_log("decoder width %d support %d", stride, supWidth);
        if (realWidth > supWidth) {
            if (access("/dev/rkvdec", 06) == 0) {
                if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC ||
                    pVideoDec->codecId == OMX_VIDEO_CodingAVC ||
                    pVideoDec->codecId == OMX_VIDEO_CodingVP9) {
                    ;
                } else {
                    mpp_err("decoder width %d big than support width %d return error", stride, VPUCheckSupportWidth());
                    ret = OMX_ErrorBadParameter;
                    goto EXIT;
                }
            } else {
                mpp_err("decoder width %d big than support width %d return error", stride, VPUCheckSupportWidth());
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
        pFoilplanetPort->portDefinition.format.video.nStride = stride;
        pFoilplanetPort->portDefinition.format.video.nSliceHeight = strideheight;
        pFoilplanetPort->portDefinition.nBufferSize = (size > pFoilplanetPort->portDefinition.nBufferSize) ? size : pFoilplanetPort->portDefinition.nBufferSize;

        if (portIndex == INPUT_PORT_INDEX) {
            FP_OMX_BASEPORT *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
            pFpOutputPort->portDefinition.format.video.nFrameWidth = pFoilplanetPort->portDefinition.format.video.nFrameWidth;
            pFpOutputPort->portDefinition.format.video.nFrameHeight = pFoilplanetPort->portDefinition.format.video.nFrameHeight;
            pFpOutputPort->portDefinition.format.video.nStride = stride;
            pFpOutputPort->portDefinition.format.video.nSliceHeight = strideheight;

#ifdef AVS80
            memset(&(pFpOutputPort->cropRectangle), 0, sizeof(OMX_CONFIG_RECTTYPE));
            pFpOutputPort->cropRectangle.nWidth = pFpOutputPort->portDefinition.format.video.nFrameWidth;
            pFpOutputPort->cropRectangle.nHeight = pFpOutputPort->portDefinition.format.video.nFrameHeight;
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent, pFpComponent->callbackData, OMX_EventPortSettingsChanged, OUTPUT_PORT_INDEX, OMX_IndexConfigCommonOutputCrop, NULL);
            if (pFpOutputPort->portDefinition.format.video.nFrameWidth
                * pFpOutputPort->portDefinition.format.video.nFrameHeight > 1920 * 1088) {
                pFpOutputPort->portDefinition.nBufferCountActual = 14;
                pFpOutputPort->portDefinition.nBufferCountMin = 10;
            }
#endif

            switch ((OMX_U32)pFpOutputPort->portDefinition.format.video.eColorFormat) {

            case OMX_COLOR_FormatYUV420Planar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
                pFpOutputPort->portDefinition.nBufferSize = (stride * strideheight * 3) / 2;
                break;
#ifdef USE_STOREMETADATA
            case OMX_COLOR_FormatAndroidOpaque:
                pFpOutputPort->portDefinition.nBufferSize = stride * strideheight * 4;
                if (pVideoDec->bPvr_Flag == OMX_TRUE) {
                    pFpOutputPort->portDefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_BGRA_8888;
                } else {
                    pFpOutputPort->portDefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_RGBA_8888;
                }
                break;
#endif
            default:
                mpp_err("Color format is not support!! use default YUV size!!");
                ret = OMX_ErrorUnsupportedSetting;
                break;
            }

            if (pFpOutputPort->bufferProcessType == BUFFER_SHARE) {
                pFpOutputPort->portDefinition.nBufferSize =
                    calc_plane(pFoilplanetPort->portDefinition.format.video.nFrameWidth, pFpOutputPort->portDefinition.format.video.nFrameHeight) +
                    calc_plane(pFoilplanetPort->portDefinition.format.video.nFrameWidth, pFpOutputPort->portDefinition.format.video.nFrameHeight >> 1);
            }
        }
    }
    break;
#ifdef USE_ANB
    case OMX_IndexParamEnableAndroidBuffers:
    case OMX_IndexParamUseAndroidNativeBuffer:
    case OMX_IndexParamStoreMetaDataBuffer:
    case OMX_IndexParamprepareForAdaptivePlayback:
    case OMX_IndexParamAllocateNativeHandle: {
        mpp_log("OSAL_SetANBParameter!!");
        ret = OSAL_SetANBParameter(hComponent, nIndex, ComponentParameterStructure);
    }
    break;
#endif

    case OMX_IndexParamEnableThumbnailMode: {
        FP_OMX_VIDEO_THUMBNAILMODE *pThumbnailMode = (FP_OMX_VIDEO_THUMBNAILMODE *)ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        ret = FP_OMX_Check_SizeVersion(pThumbnailMode, sizeof(FP_OMX_VIDEO_THUMBNAILMODE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pVideoDec->bThumbnailMode = pThumbnailMode->bEnable;
        if (pVideoDec->bThumbnailMode == OMX_TRUE) {
            FP_OMX_BASEPORT *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
            pFpOutputPort->portDefinition.nBufferCountMin = 1;
            pFpOutputPort->portDefinition.nBufferCountActual = 1;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionDiv3: {
        OMX_BOOL *pIsDiv3 = (OMX_BOOL *)ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        if ((*pIsDiv3) == OMX_TRUE) {
            pVideoDec->flags |= FP_OMX_VDEC_IS_DIV3;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionUseDts: {
        OMX_BOOL *pUseDtsFlag = (OMX_BOOL *)ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

        if ((*pUseDtsFlag) == OMX_TRUE) {
            pVideoDec->flags |= FP_OMX_VDEC_USE_DTS;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionThumbNail: {
        OMX_PARAM_U32TYPE *tmp = (OMX_PARAM_U32TYPE *)ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
        OMX_U32 thumbFlag = tmp->nU32;
        if (thumbFlag) {
            pVideoDec->flags |= FP_OMX_VDEC_THUMBNAIL;
        }

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

        if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_H264_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_MPEG4_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_H263_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_MPEG2_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_VP8_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_VP9_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_HEVC_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_FLV_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_VP6_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_MJPEG_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_VC1_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_WMV3_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
        } else if (!strcmp((char*)pComponentRole->cRole, FP_OMX_COMPONENT_RMVB_DEC_ROLE)) {
            pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingRV;
        } else {
            ret = OMX_ErrorInvalidComponentName;
            goto EXIT;
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = NULL;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
        ret = FP_OMX_Check_SizeVersion(pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pVideoDec->AVCComponent[pSrcAVCComponent->nPortIndex];

        memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
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
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

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

    switch (nIndex) {
#ifdef AVS80
    case OMX_IndexConfigCommonOutputCrop: {
        OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_U32 portIndex = rectParams->nPortIndex;
        FP_OMX_BASEPORT *pFoilplanetPort = NULL;
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

        if (rectParams->nPortIndex != OUTPUT_PORT_INDEX) {
            return OMX_ErrorUndefined;
        }
        /*Avoid rectParams->nWidth and rectParams->nHeight to be set as 0*/
        if (pFoilplanetPort->cropRectangle.nHeight > 0 && pFoilplanetPort->cropRectangle.nWidth > 0)
            memcpy(rectParams, &(pFoilplanetPort->cropRectangle), sizeof(OMX_CONFIG_RECTTYPE));
        else
            rectParams->nWidth = rectParams->nHeight = 1;
        omx_info("rectParams:%d %d %d %d", rectParams->nLeft, rectParams->nTop, rectParams->nWidth, rectParams->nHeight);
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
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

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

    switch (nIndex) {
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
    FP_OMX_BASECOMPONENT *pFpComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nIndex == 0) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_H264_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 1) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_MPEG4_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 2) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_H263_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 3) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_FLV_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 4) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_MPEG2_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 5) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_RMVB_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 6) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_VP8_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 7) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_VC1_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 8) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_WMV3_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 9) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_VP6_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 10) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_HEVC_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 12) {
        strcpy((char *)cRole, FP_OMX_COMPONENT_H264_DEC_ROLE);
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
#ifdef USE_ANB
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ENABLE_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamEnableAndroidBuffers;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_GET_ANB_Usage) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamGetAndroidNativeBufferUsage;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_USE_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) NULL;//OMX_IndexParamUseAndroidNativeBuffer;
        goto EXIT;
    }

    if (strcmp(cParameterName, FOILPLANET_INDEX_PREPARE_ADAPTIVE_PLAYBACK) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamprepareForAdaptivePlayback;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_DESCRIBE_COLORFORMAT) == 0) {
        mpp_err("OMX_IndexParamdescribeColorFormat get ");
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamdescribeColorFormat;
        goto EXIT;
    }
#endif

    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ENABLE_THUMBNAIL) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamEnableThumbnailMode;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_DIV3) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionDiv3;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_THUMBNAIL) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionThumbNail;
        goto EXIT;
    }
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_USE_DTS) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionUseDts;
        goto EXIT;
    }
#ifdef USE_STOREMETADATA
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) NULL;
        goto EXIT;
    }
#endif

#ifdef AVS80
#ifdef HAVE_L1_SVP_MODE
    if (strcmp(cParameterName, FOILPLANET_INDEX_PARAM_ALLOCATENATIVEHANDLE) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamAllocateNativeHandle;
        goto EXIT;
    }
#endif
#endif

    ret = FP_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

