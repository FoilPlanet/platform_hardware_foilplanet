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
#include <unistd.h>

#include "Foilplanet_OMX_Baseport.h"
#include "Foilplanet_OMX_Basecomponent.h"
#include "Foilplanet_OMX_Resourcemanager.h"

#include "OMX_Macros.h"

#include "osal_event.h"
#include "osal/mpp_log.h"
#include "osal/mpp_thread.h"
#include "osal/mpp_mem.h"
#include "osal/mpp_list.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG         "FP_OMX_COMP"
#endif

#define GET_MPPLIST(q)      ((mpp_list *)q)

/* Change CHECK_SIZE_VERSION Macro */
OMX_ERRORTYPE FP_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_VERSIONTYPE* version = NULL;
    if (header == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    version = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
    if (*((OMX_U32*)header) != size) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (version->s.nVersionMajor != VERSIONMAJOR_NUMBER ||
        version->s.nVersionMinor != VERSIONMINOR_NUMBER) {
        ret = OMX_ErrorVersionMismatch;
        goto EXIT;
    }
    ret = OMX_ErrorNone;
EXIT:
    return ret;
}

OMX_ERRORTYPE FP_OMX_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE   hComponent,
    OMX_OUT OMX_STRING       pComponentName,
    OMX_OUT OMX_VERSIONTYPE *pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE *pSpecVersion,
    OMX_OUT OMX_UUIDTYPE    *pComponentUUID)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE       *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT    *pFpComponent = NULL;
    OMX_U32                 compUUID[3];

    FunctionIn();

    /* check parameters */
    if (hComponent     == NULL ||
        pComponentName == NULL || pComponentVersion == NULL ||
        pSpecVersion   == NULL || pComponentUUID    == NULL) {
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

    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    strcpy(pComponentName, pFpComponent->componentName);
    memcpy(pComponentVersion, &(pFpComponent->componentVersion), sizeof(OMX_VERSIONTYPE));
    memcpy(pSpecVersion, &(pFpComponent->specVersion), sizeof(OMX_VERSIONTYPE));

    /* Fill UUID with handle address, PID and UID.
     * This should guarantee uiniqness */
    compUUID[0] = (OMX_U32)pOMXComponent;
    compUUID[1] = getpid();
    compUUID[2] = getuid();
    memcpy(*pComponentUUID, compUUID, 3 * sizeof(*compUUID));

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_GetState (
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_OUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pState == NULL) {
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

    *pState = pFpComponent->currentState;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_ComponentStateSet(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 messageParam)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_MESSAGE   *message;
    OMX_STATETYPE             destState = (OMX_STATETYPE)messageParam;
    OMX_STATETYPE             currentState = pFpComponent->currentState;
    FP_OMX_BASEPORT  *pFoilplanetPort = NULL;
    OMX_S32                   countValue = 0;
    unsigned int              i = 0, j = 0;
    int                       k = 0;
    int                       timeOutCnt = 200;

    FunctionIn();

    /* check parameters */
    if (currentState == destState) {
        ret = OMX_ErrorSameState;
        goto EXIT;
    }
    if (currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if ((currentState == OMX_StateLoaded) && (destState == OMX_StateIdle)) {
        ret = FP_OMX_Get_Resource(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            OSAL_SignalSet(pFpComponent->abendStateEvent);
            goto EXIT;
        }
    }
    if (((currentState == OMX_StateIdle) && (destState == OMX_StateLoaded))       ||
        ((currentState == OMX_StateIdle) && (destState == OMX_StateInvalid))      ||
        ((currentState == OMX_StateExecuting) && (destState == OMX_StateInvalid)) ||
        ((currentState == OMX_StatePause) && (destState == OMX_StateInvalid))) {
        FP_OMX_Release_Resource(pOMXComponent);
    }

    mpp_log("destState: %d currentState: %d", destState, currentState);
    switch (destState) {
    case OMX_StateInvalid:
        switch (currentState) {
        case OMX_StateWaitForResources:
            FP_OMX_Out_WaitForResource(pOMXComponent);
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
        case OMX_StateLoaded:
            pFpComponent->currentState = OMX_StateInvalid;
            ret = pFpComponent->fp_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                if (pFpComponent->pFoilplanetPort[i].portWayType == WAY1_PORT) {
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pFpComponent->pFoilplanetPort[i].portWayType == WAY2_PORT) {
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }
                MUTEX_FREE(pFpComponent->pFoilplanetPort[i].hPortMutex);
                pFpComponent->pFoilplanetPort[i].hPortMutex = NULL;
                MUTEX_FREE(pFpComponent->pFoilplanetPort[i].secureBufferMutex);
                pFpComponent->pFoilplanetPort[i].secureBufferMutex = NULL;
            }

            if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                OSAL_SignalTerminate(pFpComponent->pauseEvent);
                pFpComponent->pauseEvent = NULL;
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].pauseEvent);
                    pFpComponent->pFoilplanetPort[i].pauseEvent = NULL;
                    if (pFpComponent->pFoilplanetPort[i].bufferProcessType == BUFFER_SHARE) {
                        OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent);
                        pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent = NULL;
                    }
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                OSAL_SemaphoreTerminate(pFpComponent->pFoilplanetPort[i].bufferSemID);
                pFpComponent->pFoilplanetPort[i].bufferSemID = NULL;
            }
            if (pFpComponent->fp_codec_componentTerminate != NULL)
                pFpComponent->fp_codec_componentTerminate(pOMXComponent);

            ret = OMX_ErrorInvalidState;
            break;
        default:
            ret = OMX_ErrorInvalidState;
            break;
        }
        break;
    case OMX_StateLoaded:
        switch (currentState) {
        case OMX_StateIdle:
            ret = pFpComponent->fp_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                if (pFpComponent->pFoilplanetPort[i].portWayType == WAY1_PORT) {
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pFpComponent->pFoilplanetPort[i].portWayType == WAY2_PORT) {
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }
                MUTEX_FREE(pFpComponent->pFoilplanetPort[i].hPortMutex);
                pFpComponent->pFoilplanetPort[i].hPortMutex = NULL;
                MUTEX_FREE(pFpComponent->pFoilplanetPort[i].secureBufferMutex);
                pFpComponent->pFoilplanetPort[i].secureBufferMutex = NULL;
            }
            if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                OSAL_SignalTerminate(pFpComponent->pauseEvent);
                pFpComponent->pauseEvent = NULL;
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].pauseEvent);
                    pFpComponent->pFoilplanetPort[i].pauseEvent = NULL;
                    if (pFpComponent->pFoilplanetPort[i].bufferProcessType == BUFFER_SHARE) {
                        OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent);
                        pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent = NULL;
                    }
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                OSAL_SemaphoreTerminate(pFpComponent->pFoilplanetPort[i].bufferSemID);
                pFpComponent->pFoilplanetPort[i].bufferSemID = NULL;
            }

            pFpComponent->fp_codec_componentTerminate(pOMXComponent);

            for (i = 0; i < (pFpComponent->portParam.nPorts); i++) {
                pFoilplanetPort = (pFpComponent->pFoilplanetPort + i);
                if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    while (!GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE))) {
                        mpp_free((FP_OMX_MESSAGE*)message);
                    }
                    ret = pFpComponent->fp_FreeTunnelBuffer(pFoilplanetPort, i);
                    if (OMX_ErrorNone != ret) {
                        goto EXIT;
                    }
                } else {
                    if (CHECK_PORT_ENABLED(pFoilplanetPort)) {
                        OSAL_SemaphoreWait(pFoilplanetPort->unloadedResource);
                        pFoilplanetPort->portDefinition.bPopulated = OMX_FALSE;
                    }
                }
            }
            pFpComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateWaitForResources:
            ret = FP_OMX_Out_WaitForResource(pOMXComponent);
            pFpComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateIdle:
        switch (currentState) {
        case OMX_StateLoaded:
            mpp_log("OMX_StateLoaded in loadedResource");
            for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
                pFoilplanetPort = (pFpComponent->pFoilplanetPort + i);
                if (pFoilplanetPort == NULL) {
                    ret = OMX_ErrorBadParameter;
                    goto EXIT;
                }
                if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    if (CHECK_PORT_ENABLED(pFoilplanetPort)) {
                        ret = pFpComponent->fp_AllocateTunnelBuffer(pFoilplanetPort, i);
                        if (ret != OMX_ErrorNone)
                            goto EXIT;
                    }
                } else {
                    if (CHECK_PORT_ENABLED(pFoilplanetPort)) {
                        mpp_log("loadedResource ");
                        OSAL_SemaphoreWait(pFpComponent->pFoilplanetPort[i].loadedResource);
                        mpp_log("loadedResource out");
                        if (pFpComponent->abendState == OMX_TRUE) {
                            mpp_err("abendState == OMX_TRUE");
                            OSAL_SignalSet(pFpComponent->abendStateEvent);
                            ret = FP_OMX_Release_Resource(pOMXComponent);
                            goto EXIT;
                        }
                        pFoilplanetPort->portDefinition.bPopulated = OMX_TRUE;
                    }
                }
            }

            mpp_log("fp_codec_componentInit");
            ret = pFpComponent->fp_codec_componentInit(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
                OSAL_SignalSet(pFpComponent->abendStateEvent);
                FP_OMX_Release_Resource(pOMXComponent);
                goto EXIT;
            }
            if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                OSAL_SignalCreate(&pFpComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SignalCreate(&pFpComponent->pFoilplanetPort[i].pauseEvent);
                    if (pFpComponent->pFoilplanetPort[i].bufferProcessType == BUFFER_SHARE)
                        OSAL_SignalCreate(&pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent);
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                ret = OSAL_SemaphoreCreate(&pFpComponent->pFoilplanetPort[i].bufferSemID);
                if (ret != OMX_ErrorNone) {
                    ret = OMX_ErrorInsufficientResources;
                    mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                    goto EXIT;
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                OMX_HANDLETYPE mh;
                if (pFpComponent->pFoilplanetPort[i].portWayType == WAY1_PORT) {
                    mh = MUTEX_CREATE();
                    if (!mh) {
                        ret = OMX_ErrorInsufficientResources;
                        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                    pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = mh;
                } else if (pFpComponent->pFoilplanetPort[i].portWayType == WAY2_PORT) {
                    mh = MUTEX_CREATE();
                    if (!mh) {
                        ret = OMX_ErrorInsufficientResources;
                        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = mh;
                    mh = MUTEX_CREATE();
                    if (!mh) {
                        ret = OMX_ErrorInsufficientResources;
                        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                    pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = mh;
                }
                
                mh = MUTEX_CREATE();
                if (!mh) {
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pFpComponent->pFoilplanetPort[i].hPortMutex = mh;

                mh = MUTEX_CREATE();
                if (!mh) {
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pFpComponent->pFoilplanetPort[i].secureBufferMutex = mh;
            }
            mpp_log("fp_BufferProcessCreate");

            ret = pFpComponent->fp_BufferProcessCreate(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
                if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                    OSAL_SignalTerminate(pFpComponent->pauseEvent);
                    pFpComponent->pauseEvent = NULL;
                } else {
                    for (i = 0; i < ALL_PORT_NUM; i++) {
                        OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].pauseEvent);
                        pFpComponent->pFoilplanetPort[i].pauseEvent = NULL;
                        if (pFpComponent->pFoilplanetPort[i].bufferProcessType == BUFFER_SHARE) {
                            OSAL_SignalTerminate(pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent);
                            pFpComponent->pFoilplanetPort[i].hAllCodecBufferReturnEvent = NULL;
                        }
                    }
                }
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    if (pFpComponent->pFoilplanetPort[i].portWayType == WAY1_PORT) {
                        MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                        pFpComponent->pFoilplanetPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                    } else if (pFpComponent->pFoilplanetPort[i].portWayType == WAY2_PORT) {
                        MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                        pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                        MUTEX_FREE(pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                        pFpComponent->pFoilplanetPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                    }
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].hPortMutex);
                    pFpComponent->pFoilplanetPort[i].hPortMutex = NULL;
                    MUTEX_FREE(pFpComponent->pFoilplanetPort[i].secureBufferMutex);
                    pFpComponent->pFoilplanetPort[i].secureBufferMutex = NULL;
                }
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SemaphoreTerminate(pFpComponent->pFoilplanetPort[i].bufferSemID);
                    pFpComponent->pFoilplanetPort[i].bufferSemID = NULL;
                }

                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            mpp_log(" OMX_StateIdle");
            pFpComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
            if (currentState == OMX_StateExecuting) {
                pFpComponent->nRkFlags |= RK_VPU_NEED_FLUSH_ON_SEEK;
            }
            FP_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);
            pFpComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateWaitForResources:
            pFpComponent->currentState = OMX_StateIdle;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateExecuting:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
                pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
                if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort) && CHECK_PORT_ENABLED(pFoilplanetPort)) {
                    for (j = 0; j < pFoilplanetPort->tunnelBufferNum; j++) {
                        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[i].bufferSemID);
                    }
                }
            }

            pFpComponent->transientState = FP_OMX_TransStateMax;
            pFpComponent->currentState = OMX_StateExecuting;
            if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                OSAL_SignalSet(pFpComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SignalSet(pFpComponent->pFoilplanetPort[i].pauseEvent);
                }
            }
            break;
        case OMX_StatePause:
            for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
                pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
                if (CHECK_PORT_TUNNELED(pFoilplanetPort) && CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort) && CHECK_PORT_ENABLED(pFoilplanetPort)) {
                    OMX_S32 semaValue = 0, cnt = 0;
                    OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[i].bufferSemID, &semaValue);
                    OMX_S32 n = GET_MPPLIST(pFoilplanetPort->bufferQ)->list_size();
                    cnt = n - semaValue;
                    for (k = 0; k < cnt; k++) {
                        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[i].bufferSemID);
                    }
                }
            }

            pFpComponent->currentState = OMX_StateExecuting;
            if (pFpComponent->bMultiThreadProcess == OMX_FALSE) {
                OSAL_SignalSet(pFpComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    OSAL_SignalSet(pFpComponent->pFoilplanetPort[i].pauseEvent);
                }
            }
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StatePause:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            pFpComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateExecuting:
            pFpComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateWaitForResources:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = FP_OMX_In_WaitForResource(pOMXComponent);
            pFpComponent->currentState = OMX_StateWaitForResources;
            break;
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    default:
        ret = OMX_ErrorIncorrectStateTransition;
        break;
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pFpComponent->pCallbacks != NULL) {
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventCmdComplete, OMX_CommandStateSet,
                                                         destState, NULL);
        }
    } else {
        mpp_err("ERROR");
        if (pFpComponent->pCallbacks != NULL) {
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventError, ret, 0, NULL);
        }
    }
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE _OMX_MessageHandlerThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_MESSAGE       *message = NULL;
    OMX_U32                   messageType = 0, portIndex = 0;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    while (pFpComponent->bExitMessageHandlerThread == OMX_FALSE) {
        GET_MPPLIST(pFpComponent->messageQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE));
        if (message != NULL) {
            messageType = message->messageType;
            switch (messageType) {
            case OMX_CommandStateSet:
                ret = FP_OMX_ComponentStateSet(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandFlush:
                ret = FP_OMX_BufferFlushProcess(pOMXComponent, message->messageParam, OMX_TRUE);
                break;
            case OMX_CommandPortDisable:
                ret = FP_OMX_PortDisableProcess(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandPortEnable:
                ret = FP_OMX_PortEnableProcess(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandMarkBuffer:
                portIndex = message->messageParam;
                pFpComponent->pFoilplanetPort[portIndex].markType.hMarkTargetComponent = ((OMX_MARKTYPE *)message->pCmdData)->hMarkTargetComponent;
                pFpComponent->pFoilplanetPort[portIndex].markType.pMarkData            = ((OMX_MARKTYPE *)message->pCmdData)->pMarkData;
                break;
            case (OMX_COMMANDTYPE)FP_OMX_CommandComponentDeInit:
                pFpComponent->bExitMessageHandlerThread = OMX_TRUE;
                break;
            default:
                break;
            }
            mpp_free(message);
            message = NULL;
        }
    }

    pthread_exit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Foilplanet_StateSet(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nParam)
{
    OMX_U32 destState = nParam;
    OMX_U32 i = 0;

    if ((destState == OMX_StateIdle) && (pFpComponent->currentState == OMX_StateLoaded)) {
        pFpComponent->transientState = FP_OMX_TransStateLoadedToIdle;
        for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
            pFpComponent->pFoilplanetPort[i].portState = OMX_StateIdle;
        }
        mpp_log("to OMX_StateIdle");
    } else if ((destState == OMX_StateLoaded) && (pFpComponent->currentState == OMX_StateIdle)) {
        pFpComponent->transientState = FP_OMX_TransStateIdleToLoaded;
        for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
            pFpComponent->pFoilplanetPort[i].portState = OMX_StateLoaded;
        }
        mpp_log("to OMX_StateLoaded");
    } else if ((destState == OMX_StateIdle) && (pFpComponent->currentState == OMX_StateExecuting)) {
        FP_OMX_BASEPORT *pFoilplanetPort = NULL;

        pFoilplanetPort = &(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX]);
        if ((pFoilplanetPort->portDefinition.bEnabled == OMX_FALSE) &&
            (pFoilplanetPort->portState == OMX_StateIdle)) {
            pFoilplanetPort->exceptionFlag = INVALID_STATE;
            OSAL_SemaphorePost(pFoilplanetPort->loadedResource);
        }

        pFoilplanetPort = &(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX]);
        if ((pFoilplanetPort->portDefinition.bEnabled == OMX_FALSE) &&
            (pFoilplanetPort->portState == OMX_StateIdle)) {
            pFoilplanetPort->exceptionFlag = INVALID_STATE;
            OSAL_SemaphorePost(pFoilplanetPort->loadedResource);
        }

        pFpComponent->transientState = FP_OMX_TransStateExecutingToIdle;
        mpp_log("to OMX_StateIdle");
    } else if ((destState == OMX_StateExecuting) && (pFpComponent->currentState == OMX_StateIdle)) {
        pFpComponent->transientState = FP_OMX_TransStateIdleToExecuting;
        mpp_log("to OMX_StateExecuting");
    } else if (destState == OMX_StateInvalid) {
        for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
            pFpComponent->pFoilplanetPort[i].portState = OMX_StateInvalid;
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE Foilplanet_SetPortFlush(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    FP_OMX_BASEPORT     *pFoilplanetPort = NULL;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0, cnt = 0, index = 0;


    if ((pFpComponent->currentState == OMX_StateExecuting) ||
        (pFpComponent->currentState == OMX_StatePause)) {
        if ((portIndex != ALL_PORT_INDEX) &&
            ((OMX_S32)portIndex >= (OMX_S32)pFpComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        /*********************
        *    need flush event set ?????
        **********************/
        cnt = (portIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;
        for (i = 0; i < cnt; i++) {
            if (portIndex == ALL_PORT_INDEX)
                index = i;
            else
                index = portIndex;
            pFpComponent->pFoilplanetPort[index].bIsPortFlushed = OMX_TRUE;
        }
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }
    ret = OMX_ErrorNone;

EXIT:
    return ret;
}

static OMX_ERRORTYPE Foilplanet_SetPortEnable(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    FP_OMX_BASEPORT *pFoilplanetPort = NULL;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0, cnt = 0;

    FunctionIn();

    if ((portIndex != ALL_PORT_INDEX) &&
        ((OMX_S32)portIndex >= (OMX_S32)pFpComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (portIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
            if (CHECK_PORT_ENABLED(pFoilplanetPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            } else {
                pFoilplanetPort->portState = OMX_StateIdle;
            }
        }
    } else {
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
        if (CHECK_PORT_ENABLED(pFoilplanetPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        } else {
            pFoilplanetPort->portState = OMX_StateIdle;
        }
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;

}

static OMX_ERRORTYPE Foilplanet_SetPortDisable(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    FP_OMX_BASEPORT *pFoilplanetPort = NULL;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0, cnt = 0;

    FunctionIn();

    if ((portIndex != ALL_PORT_INDEX) &&
        ((OMX_S32)portIndex >= (OMX_S32)pFpComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (portIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pFpComponent->portParam.nPorts; i++) {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
            if (!CHECK_PORT_ENABLED(pFoilplanetPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
            pFoilplanetPort->portState = OMX_StateLoaded;
            pFoilplanetPort->bIsPortDisabled = OMX_TRUE;
        }
    } else {
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];
        pFoilplanetPort->portState = OMX_StateLoaded;
        pFoilplanetPort->bIsPortDisabled = OMX_TRUE;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Foilplanet_SetMarkBuffer(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    FP_OMX_BASEPORT *pFoilplanetPort = NULL;
    OMX_U32              portIndex = nParam;
    OMX_U16              i = 0, cnt = 0;


    if (nParam >= pFpComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pFpComponent->currentState == OMX_StateExecuting) ||
        (pFpComponent->currentState == OMX_StatePause)) {
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE FP_OMX_CommandQueue(
    FP_OMX_BASECOMPONENT *pFpComponent,
    OMX_COMMANDTYPE        Cmd,
    OMX_U32                nParam,
    OMX_PTR                pCmdData)
{
    OMX_ERRORTYPE    ret = OMX_ErrorNone;
    FP_OMX_MESSAGE *command = mpp_malloc(FP_OMX_MESSAGE, 1);

    if (command == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    command->messageType  = (OMX_U32)Cmd;
    command->messageParam = nParam;
    command->pCmdData     = pCmdData;

    if (GET_MPPLIST(pFpComponent->messageQ)->add_at_tail(command, sizeof(FP_OMX_MESSAGE))) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    ret = OSAL_SemaphorePost(pFpComponent->msgSemaphoreHandle);

EXIT:
    return ret;
}

OMX_ERRORTYPE FP_OMX_SendCommand(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_COMMANDTYPE Cmd,
    OMX_IN OMX_U32         nParam,
    OMX_IN OMX_PTR         pCmdData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_MESSAGE       *message = NULL;

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

    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (Cmd) {
    case OMX_CommandStateSet :
        mpp_log("Command: OMX_CommandStateSet");
        Foilplanet_StateSet(pFpComponent, nParam);
        break;
    case OMX_CommandFlush :
        mpp_log("Command: OMX_CommandFlush");
        pFpComponent->nRkFlags |= RK_VPU_NEED_FLUSH_ON_SEEK;
        ret = Foilplanet_SetPortFlush(pFpComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandPortDisable :
        mpp_log("Command: OMX_CommandPortDisable");
        ret = Foilplanet_SetPortDisable(pFpComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandPortEnable :
        mpp_log("Command: OMX_CommandPortEnable");
        ret = Foilplanet_SetPortEnable(pFpComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandMarkBuffer :
        mpp_log("Command: OMX_CommandMarkBuffer");
        ret = Foilplanet_SetMarkBuffer(pFpComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    default:
        break;
    }

    ret = FP_OMX_CommandQueue(pFpComponent, Cmd, nParam, pCmdData);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = FP_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        portParam->nPorts         = 0;
        portParam->nStartPortNumber     = 0;
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
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)ComponentParameterStructure;

        ret = FP_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        compPriority->nGroupID       = pFpComponent->compPriority.nGroupID;
        compPriority->nGroupPriority = pFpComponent->compPriority.nGroupPriority;
    }
    break;

    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = bufferSupplier->nPortIndex;
        FP_OMX_BASEPORT          *pFoilplanetPort;

        if ((pFpComponent->currentState == OMX_StateLoaded) ||
            (pFpComponent->currentState == OMX_StateWaitForResources)) {
            if (portIndex >= pFpComponent->portParam.nPorts) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            ret = FP_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
            if (ret != OMX_ErrorNone) {
                goto EXIT;
            }

            pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];


            if (pFoilplanetPort->portDefinition.eDir == OMX_DirInput) {
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else if (CHECK_PORT_TUNNELED(pFoilplanetPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            } else {
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else if (CHECK_PORT_TUNNELED(pFoilplanetPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            }
        } else {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    }
    break;
    default: {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
    break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE    *pOMXComponent = NULL;
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

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = FP_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pFpComponent->currentState != OMX_StateLoaded) &&
            (pFpComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
        ret = OMX_ErrorUndefined;
        /* memcpy(&pFpComponent->portParam, portParam, sizeof(OMX_PORT_PARAM_TYPE)); */
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

        if ((pFpComponent->currentState != OMX_StateLoaded) && (pFpComponent->currentState != OMX_StateWaitForResources)) {
            if (pFoilplanetPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }
        if (portDefinition->nBufferCountActual < pFoilplanetPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        memcpy(&pFoilplanetPort->portDefinition, portDefinition, portDefinition->nSize);
    }
    break;
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)ComponentParameterStructure;

        if ((pFpComponent->currentState != OMX_StateLoaded) &&
            (pFpComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = FP_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pFpComponent->compPriority.nGroupID = compPriority->nGroupID;
        pFpComponent->compPriority.nGroupPriority = compPriority->nGroupPriority;
    }
    break;
    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)ComponentParameterStructure;
        OMX_U32           portIndex = bufferSupplier->nPortIndex;
        FP_OMX_BASEPORT *pFoilplanetPort = NULL;


        if (portIndex >= pFpComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = FP_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
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

        if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyUnspecified) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
        if (CHECK_PORT_TUNNELED(pFoilplanetPort) == 0) {
            ret = OMX_ErrorNone; /*OMX_ErrorNone ?????*/
            goto EXIT;
        }

        if (pFoilplanetPort->portDefinition.eDir == OMX_DirInput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pFoilplanetPort->tunnelFlags |= FOILPLANET_TUNNEL_IS_SUPPLIER;
                bufferSupplier->nPortIndex = pFoilplanetPort->tunneledPort;
                ret = OMX_SetParameter(pFoilplanetPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    pFoilplanetPort->tunnelFlags &= ~FOILPLANET_TUNNEL_IS_SUPPLIER;
                    bufferSupplier->nPortIndex = pFoilplanetPort->tunneledPort;
                    ret = OMX_SetParameter(pFoilplanetPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                }
                goto EXIT;
            }
        } else if (pFoilplanetPort->portDefinition.eDir == OMX_DirOutput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    pFoilplanetPort->tunnelFlags &= ~FOILPLANET_TUNNEL_IS_SUPPLIER;
                    ret = OMX_ErrorNone;
                }
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pFoilplanetPort->tunnelFlags |= FOILPLANET_TUNNEL_IS_SUPPLIER;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }
    break;
    default: {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
    break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_INOUT OMX_PTR     pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    ret = OMX_ErrorBadParameter;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_SetCallbacks (
    OMX_IN OMX_HANDLETYPE    hComponent,
    OMX_IN OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN OMX_PTR           pAppData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;

        mpp_err("OMX_ErrorBadParameter :%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {

        mpp_err("OMX_ErrorNone :%d", __LINE__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {

        mpp_err("OMX_ErrorBadParameter :%d", __LINE__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pCallbacks == NULL) {
        mpp_err("OMX_ErrorBadParameter :%d", __LINE__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pFpComponent->currentState == OMX_StateInvalid) {

        mpp_err("OMX_ErrorInvalidState :%d", __LINE__);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }
    if (pFpComponent->currentState != OMX_StateLoaded) {
        mpp_err("OMX_StateLoaded :%d", __LINE__);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pFpComponent->pCallbacks = pCallbacks;
    pFpComponent->callbackData = pAppData;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_UseEGLImage(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN void                     *eglImage)
{
    (void)hComponent;
    (void)ppBufferHdr;
    (void)nPortIndex;
    (void)pAppPrivate;
    (void)eglImage;
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE FP_OMX_BaseComponent_Constructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        mpp_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pFpComponent = mpp_malloc(FP_OMX_BASECOMPONENT, 1);
    if (pFpComponent == NULL) {
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    memset(pFpComponent, 0, sizeof(FP_OMX_BASECOMPONENT));
    pFpComponent->rkversion = &OMX_version[0];
    pOMXComponent->pComponentPrivate = (OMX_PTR)pFpComponent;

    ret = OSAL_SemaphoreCreate(&pFpComponent->msgSemaphoreHandle);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    pFpComponent->compMutex = MUTEX_CREATE();
    if (!pFpComponent->compMutex) {
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    ret = OSAL_SignalCreate(&pFpComponent->abendStateEvent);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    pFpComponent->bExitMessageHandlerThread = OMX_FALSE;

    // MAX_QUEUE_ELEMENTS
    pFpComponent->messageQ = new mpp_list();

    pFpComponent->hMessageHandler = new MppThread((void *(*)(void *))_OMX_MessageHandlerThread, pOMXComponent);

    pFpComponent->bMultiThreadProcess = OMX_FALSE;

    pOMXComponent->GetComponentVersion = &FP_OMX_GetComponentVersion;
    pOMXComponent->SendCommand         = &FP_OMX_SendCommand;
    pOMXComponent->GetState            = &FP_OMX_GetState;
    pOMXComponent->SetCallbacks        = &FP_OMX_SetCallbacks;
    pOMXComponent->UseEGLImage         = &FP_OMX_UseEGLImage;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BaseComponent_Destructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    OMX_S32                   semaValue = 0;

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

    FP_OMX_CommandQueue(pFpComponent, (OMX_COMMANDTYPE)FP_OMX_CommandComponentDeInit, 0, NULL);
    usleep(0); // wait
    OSAL_Get_SemaphoreCount(pFpComponent->msgSemaphoreHandle, &semaValue);
    if (semaValue == 0)
        OSAL_SemaphorePost(pFpComponent->msgSemaphoreHandle);
    OSAL_SemaphorePost(pFpComponent->msgSemaphoreHandle);

    delete ((MppThread *)pFpComponent->hMessageHandler);
    pFpComponent->hMessageHandler = NULL;

    OSAL_SignalTerminate(pFpComponent->abendStateEvent);
    pFpComponent->abendStateEvent = NULL;
    MUTEX_FREE(pFpComponent->compMutex);
    pFpComponent->compMutex = NULL;
    OSAL_SemaphoreTerminate(pFpComponent->msgSemaphoreHandle);
    pFpComponent->msgSemaphoreHandle = NULL;
    delete GET_MPPLIST(pFpComponent->messageQ);

    mpp_free(pFpComponent);
    pFpComponent = NULL;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}


