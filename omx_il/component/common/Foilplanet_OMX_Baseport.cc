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

#include "OMX_Macros.h"

#include "Foilplanet_OMX_Baseport.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "osal_event.h"
#include "osal/mpp_log.h"
#include "osal/mpp_list.h"
#include "osal/mpp_mem.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG     "FP_OMX_PORT"
#endif

#define GET_MPPLIST(q)      ((mpp_list *)q)

OMX_ERRORTYPE FP_OMX_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    OMX_U32               i = 0;

    MUTEX_LOCK(pFoilplanetPort->hPortMutex);
    for (i = 0; i < pFoilplanetPort->portDefinition.nBufferCountActual; i++) {
        if (bufferHeader == pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader) {
            pFoilplanetPort->extendBufferHeader[i].bBufferInOMX = OMX_FALSE;
            break;
        }
    }

    MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
    pFpComponent->pCallbacks->EmptyBufferDone(pOMXComponent, pFpComponent->callbackData, bufferHeader);

    return ret;
}

OMX_ERRORTYPE FP_OMX_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    OMX_U32               i = 0;

    MUTEX_LOCK(pFoilplanetPort->hPortMutex);
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (bufferHeader == pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader) {
            pFoilplanetPort->extendBufferHeader[i].bBufferInOMX = OMX_FALSE;
            break;
        }
    }

    MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
    pFpComponent->pCallbacks->FillBufferDone(pOMXComponent, pFpComponent->callbackData, bufferHeader);

EXIT:
    mpp_log("bufferHeader:0x%x", bufferHeader);
    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferFlushProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_S32               portIndex = 0;
    FP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};
    OMX_U32               i = 0, cnt = 0;

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

    cnt = (nPortIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        pFpComponent->fp_BufferFlush(pOMXComponent, portIndex, bEvent);
    }

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

OMX_ERRORTYPE FP_OMX_EnablePort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_U32                i = 0, cnt = 0;

    FunctionIn();

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

    if ((pFpComponent->currentState != OMX_StateLoaded) && (pFpComponent->currentState != OMX_StateWaitForResources)) {
        OSAL_SemaphoreWait(pFoilplanetPort->loadedResource);

        if (pFoilplanetPort->exceptionFlag == INVALID_STATE) {
            pFoilplanetPort->exceptionFlag = NEED_PORT_DISABLE;
            goto EXIT;
        }
        pFoilplanetPort->portDefinition.bPopulated = OMX_TRUE;
    }
    pFoilplanetPort->exceptionFlag = GENERAL_STATE;
    pFoilplanetPort->portDefinition.bEnabled = OMX_TRUE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_PortEnableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    OMX_S32                portIndex = 0;
    OMX_U32                i = 0, cnt = 0;

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

    cnt = (nPortIndex == ALL_PORT_INDEX) ? ALL_PORT_NUM : 1;

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        ret = FP_OMX_EnablePort(pOMXComponent, portIndex);
        if (ret == OMX_ErrorNone) {
            pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandPortEnable, portIndex, NULL);
        }
    }

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pFpComponent != NULL)) {
        pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pFpComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_DisablePort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_U32               i = 0, elemNum = 0;
    FP_OMX_MESSAGE       *message;

    FunctionIn();

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[portIndex];

    if (!CHECK_PORT_ENABLED(pFoilplanetPort)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pFpComponent->currentState != OMX_StateLoaded) {
        if (CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)) {
            while (!GET_MPPLIST(pFoilplanetPort->bufferQ)->del_at_head(&message, sizeof(FP_OMX_MESSAGE))) {
                mpp_free(message);
            }
        }
        pFoilplanetPort->portDefinition.bPopulated = OMX_FALSE;
        OSAL_SemaphoreWait(pFoilplanetPort->unloadedResource);
    }
    pFoilplanetPort->portDefinition.bEnabled = OMX_FALSE;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_PortDisableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_S32                portIndex = 0;
    OMX_U32                i = 0, cnt = 0;
    FP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};

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

    cnt = (nPortIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;

    /* port flush*/
    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        FP_OMX_BufferFlushProcess(pOMXComponent, portIndex, OMX_FALSE);
    }

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        ret = FP_OMX_DisablePort(pOMXComponent, portIndex);
        pFpComponent->pFoilplanetPort[portIndex].bIsPortDisabled = OMX_FALSE;
        if (ret == OMX_ErrorNone) {
            pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandPortDisable, portIndex, NULL);
        }
    }

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pFpComponent != NULL)) {
        pFpComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pFpComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_EmptyThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE      *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    OMX_BOOL               findBuffer = OMX_FALSE;
    FP_OMX_MESSAGE       *message;
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
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pBuffer->nInputPortIndex != INPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = FP_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pFpComponent->currentState != OMX_StateIdle) &&
        (pFpComponent->currentState != OMX_StateExecuting) &&
        (pFpComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    if ((!CHECK_PORT_ENABLED(pFoilplanetPort)) ||
        (CHECK_PORT_BEING_FLUSHED(pFoilplanetPort) &&
         (!CHECK_PORT_TUNNELED(pFoilplanetPort) || !CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort))) ||
        ((pFpComponent->transientState == FP_OMX_TransStateExecutingToIdle) &&
         (CHECK_PORT_TUNNELED(pFoilplanetPort) && !CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)))) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    MUTEX_LOCK(pFoilplanetPort->hPortMutex);
    for (i = 0; i < pFoilplanetPort->portDefinition.nBufferCountActual; i++) {
        if (pBuffer == pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader) {
            pFoilplanetPort->extendBufferHeader[i].bBufferInOMX = OMX_TRUE;
            findBuffer = OMX_TRUE;
            break;
        }
    }

    if (findBuffer == OMX_FALSE) {
        ret = OMX_ErrorBadParameter;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }

    message = mpp_malloc(FP_OMX_MESSAGE, 1);
    if (message == NULL) {
        ret = OMX_ErrorInsufficientResources;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }
    message->messageType = FP_OMX_CommandEmptyBuffer;
    message->messageParam = (OMX_U32) i;
    message->pCmdData = (OMX_PTR)pBuffer;

    if (GET_MPPLIST(pFoilplanetPort->bufferQ)->add_at_tail(message, sizeof(FP_OMX_MESSAGE))) {
        ret = OMX_ErrorUndefined;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }
    ret = OSAL_SemaphorePost(pFoilplanetPort->bufferSemID);
    MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_FillThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT  *pFpComponent = NULL;
    FP_OMX_BASEPORT       *pFoilplanetPort = NULL;
    OMX_BOOL               findBuffer = OMX_FALSE;
    FP_OMX_MESSAGE        *message;
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
    if (pFpComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pBuffer->nOutputPortIndex != OUTPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = FP_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pFpComponent->currentState != OMX_StateIdle) &&
        (pFpComponent->currentState != OMX_StateExecuting) &&
        (pFpComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    if ((!CHECK_PORT_ENABLED(pFoilplanetPort)) ||
        (CHECK_PORT_BEING_FLUSHED(pFoilplanetPort) &&
         (!CHECK_PORT_TUNNELED(pFoilplanetPort) || !CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort))) ||
        ((pFpComponent->transientState == FP_OMX_TransStateExecutingToIdle) &&
         (CHECK_PORT_TUNNELED(pFoilplanetPort) && !CHECK_PORT_BUFFER_SUPPLIER(pFoilplanetPort)))) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    MUTEX_LOCK(pFoilplanetPort->hPortMutex);
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (pBuffer == pFoilplanetPort->extendBufferHeader[i].OMXBufferHeader) {
            pFoilplanetPort->extendBufferHeader[i].bBufferInOMX = OMX_TRUE;
            findBuffer = OMX_TRUE;
            break;
        }
    }

    if (findBuffer == OMX_FALSE) {
        ret = OMX_ErrorBadParameter;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }

    message = mpp_malloc(FP_OMX_MESSAGE, 1);
    if (message == NULL) {
        ret = OMX_ErrorInsufficientResources;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }
    message->messageType = FP_OMX_CommandFillBuffer;
    message->messageParam = (OMX_U32) i;
    message->pCmdData = (OMX_PTR)pBuffer;

    if (GET_MPPLIST(pFoilplanetPort->bufferQ)->add_at_tail(message, sizeof(FP_OMX_MESSAGE))) {
        ret = OMX_ErrorUndefined;
        MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);
        goto EXIT;
    }

    ret = OSAL_SemaphorePost(pFoilplanetPort->bufferSemID);
    MUTEX_UNLOCK(pFoilplanetPort->hPortMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_Port_Constructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_BASEPORT      *pFoilplanetInputPort = NULL;
    FP_OMX_BASEPORT      *pFoilplanetOutputPort = NULL;
    int i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        mpp_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        mpp_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    INIT_SET_SIZE_VERSION(&pFpComponent->portParam, OMX_PORT_PARAM_TYPE);
    pFpComponent->portParam.nPorts = ALL_PORT_NUM;
    pFpComponent->portParam.nStartPortNumber = INPUT_PORT_INDEX;

    pFoilplanetPort = mpp_malloc(FP_OMX_BASEPORT, ALL_PORT_NUM);
    if (pFoilplanetPort == NULL) {
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    memset(pFoilplanetPort, 0, sizeof(FP_OMX_BASEPORT) * ALL_PORT_NUM);
    pFpComponent->pFoilplanetPort = pFoilplanetPort;

    /* Input Port */
    pFoilplanetInputPort = &pFoilplanetPort[INPUT_PORT_INDEX];

    // MAX_QUEUE_ELEMENTS
    pFoilplanetInputPort->bufferQ = new mpp_list();
    pFoilplanetInputPort->securebufferQ = new mpp_list();

    pFoilplanetInputPort->extendBufferHeader = mpp_malloc(FP_OMX_BUFFERHEADERTYPE, MAX_BUFFER_NUM);
    if (pFoilplanetInputPort->extendBufferHeader == NULL) {
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    memset(pFoilplanetInputPort->extendBufferHeader, 0, sizeof(FP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pFoilplanetInputPort->bufferStateAllocate = mpp_malloc(OMX_U32, MAX_BUFFER_NUM);
    if (pFoilplanetInputPort->bufferStateAllocate == NULL) {
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    memset(pFoilplanetInputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pFoilplanetInputPort->bufferSemID = NULL;
    pFoilplanetInputPort->assignedBufferNum = 0;
    pFoilplanetInputPort->portState = OMX_StateMax;
    pFoilplanetInputPort->bIsPortFlushed = OMX_FALSE;
    pFoilplanetInputPort->bIsPortDisabled = OMX_FALSE;
    pFoilplanetInputPort->tunneledComponent = NULL;
    pFoilplanetInputPort->tunneledPort = 0;
    pFoilplanetInputPort->tunnelBufferNum = 0;
    pFoilplanetInputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pFoilplanetInputPort->tunnelFlags = 0;
    ret = OSAL_SemaphoreCreate(&pFoilplanetInputPort->loadedResource);
    if (ret != OMX_ErrorNone) {
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        goto EXIT;
    }
    ret = OSAL_SemaphoreCreate(&pFoilplanetInputPort->unloadedResource);
    if (ret != OMX_ErrorNone) {
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->loadedResource);
        pFoilplanetInputPort->loadedResource = NULL;
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        goto EXIT;
    }

    INIT_SET_SIZE_VERSION(&pFoilplanetInputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pFoilplanetInputPort->portDefinition.nPortIndex = INPUT_PORT_INDEX;
    pFoilplanetInputPort->portDefinition.eDir = OMX_DirInput;
    pFoilplanetInputPort->portDefinition.nBufferCountActual = 0;
    pFoilplanetInputPort->portDefinition.nBufferCountMin = 0;
    pFoilplanetInputPort->portDefinition.nBufferSize = 0;
    pFoilplanetInputPort->portDefinition.bEnabled = OMX_FALSE;
    pFoilplanetInputPort->portDefinition.bPopulated = OMX_FALSE;
    pFoilplanetInputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pFoilplanetInputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pFoilplanetInputPort->portDefinition.nBufferAlignment = 0;
    pFoilplanetInputPort->markType.hMarkTargetComponent = NULL;
    pFoilplanetInputPort->markType.pMarkData = NULL;
    pFoilplanetInputPort->exceptionFlag = GENERAL_STATE;

    /* Output Port */
    pFoilplanetOutputPort = &pFoilplanetPort[OUTPUT_PORT_INDEX];

    /* For in case of "Output Buffer Share", MAX ELEMENTS(DPB + EDPB) */
    // MAX_QUEUE_ELEMENTS
    pFoilplanetOutputPort->bufferQ = new mpp_list();

    pFoilplanetOutputPort->extendBufferHeader = mpp_malloc(FP_OMX_BUFFERHEADERTYPE, MAX_BUFFER_NUM);
    if (pFoilplanetOutputPort->extendBufferHeader == NULL) {
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->unloadedResource);
        pFoilplanetInputPort->unloadedResource = NULL;
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->loadedResource);
        pFoilplanetInputPort->loadedResource = NULL;
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pFoilplanetOutputPort->extendBufferHeader, 0, sizeof(FP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pFoilplanetOutputPort->bufferStateAllocate = mpp_malloc(OMX_U32, MAX_BUFFER_NUM);
    if (pFoilplanetOutputPort->bufferStateAllocate == NULL) {
        mpp_free(pFoilplanetOutputPort->extendBufferHeader);
        pFoilplanetOutputPort->extendBufferHeader = NULL;

        OSAL_SemaphoreTerminate(pFoilplanetInputPort->unloadedResource);
        pFoilplanetInputPort->unloadedResource = NULL;
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->loadedResource);
        pFoilplanetInputPort->loadedResource = NULL;
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pFoilplanetOutputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pFoilplanetOutputPort->bufferSemID = NULL;
    pFoilplanetOutputPort->assignedBufferNum = 0;
    pFoilplanetOutputPort->portState = OMX_StateMax;
    pFoilplanetOutputPort->bIsPortFlushed = OMX_FALSE;
    pFoilplanetOutputPort->bIsPortDisabled = OMX_FALSE;
    pFoilplanetOutputPort->tunneledComponent = NULL;
    pFoilplanetOutputPort->tunneledPort = 0;
    pFoilplanetOutputPort->tunnelBufferNum = 0;
    pFoilplanetOutputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pFoilplanetOutputPort->tunnelFlags = 0;
    ret = OSAL_SemaphoreCreate(&pFoilplanetOutputPort->loadedResource);
    if (ret != OMX_ErrorNone) {
        mpp_free(pFoilplanetOutputPort->bufferStateAllocate);
        pFoilplanetOutputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetOutputPort->extendBufferHeader);
        pFoilplanetOutputPort->extendBufferHeader = NULL;

        OSAL_SemaphoreTerminate(pFoilplanetInputPort->unloadedResource);
        pFoilplanetInputPort->unloadedResource = NULL;
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->loadedResource);
        pFoilplanetInputPort->loadedResource = NULL;
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        goto EXIT;
    }
    ret = OSAL_SignalCreate(&pFoilplanetOutputPort->unloadedResource);
    if (ret != OMX_ErrorNone) {
        OSAL_SemaphoreTerminate(pFoilplanetOutputPort->loadedResource);
        pFoilplanetOutputPort->loadedResource = NULL;
        mpp_free(pFoilplanetOutputPort->bufferStateAllocate);
        pFoilplanetOutputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetOutputPort->extendBufferHeader);
        pFoilplanetOutputPort->extendBufferHeader = NULL;

        OSAL_SemaphoreTerminate(pFoilplanetInputPort->unloadedResource);
        pFoilplanetInputPort->unloadedResource = NULL;
        OSAL_SemaphoreTerminate(pFoilplanetInputPort->loadedResource);
        pFoilplanetInputPort->loadedResource = NULL;
        mpp_free(pFoilplanetInputPort->bufferStateAllocate);
        pFoilplanetInputPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetInputPort->extendBufferHeader);
        pFoilplanetInputPort->extendBufferHeader = NULL;
        mpp_free(pFoilplanetPort);
        pFoilplanetPort = NULL;
        goto EXIT;
    }

    INIT_SET_SIZE_VERSION(&pFoilplanetOutputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pFoilplanetOutputPort->portDefinition.nPortIndex = OUTPUT_PORT_INDEX;
    pFoilplanetOutputPort->portDefinition.eDir = OMX_DirOutput;
    pFoilplanetOutputPort->portDefinition.nBufferCountActual = 0;
    pFoilplanetOutputPort->portDefinition.nBufferCountMin = 0;
    pFoilplanetOutputPort->portDefinition.nBufferSize = 0;
    pFoilplanetOutputPort->portDefinition.bEnabled = OMX_FALSE;
    pFoilplanetOutputPort->portDefinition.bPopulated = OMX_FALSE;
    pFoilplanetOutputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pFoilplanetOutputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pFoilplanetOutputPort->portDefinition.nBufferAlignment = 0;
    pFoilplanetOutputPort->markType.hMarkTargetComponent = NULL;
    pFoilplanetOutputPort->markType.pMarkData = NULL;
    pFoilplanetOutputPort->exceptionFlag = GENERAL_STATE;

    pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
    pFpComponent->checkTimeStamp.startTimeStamp = 0;
    pFpComponent->checkTimeStamp.nStartFlags = 0x0;

    pOMXComponent->EmptyThisBuffer = &FP_OMX_EmptyThisBuffer;
    pOMXComponent->FillThisBuffer  = &FP_OMX_FillThisBuffer;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_Port_Destructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;

    OMX_S32 countValue = 0;
    int i = 0;

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

    if (pFpComponent->transientState == FP_OMX_TransStateLoadedToIdle) {
        pFpComponent->abendState = OMX_TRUE;
        for (i = 0; i < ALL_PORT_NUM; i++) {
            pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
            OSAL_SemaphorePost(pFoilplanetPort->loadedResource);
        }
        OSAL_SignalWait(pFpComponent->abendStateEvent, DEF_MAX_WAIT_TIME);
        OSAL_SignalReset(pFpComponent->abendStateEvent);
    }

    for (i = 0; i < ALL_PORT_NUM; i++) {
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];

        OSAL_SemaphoreTerminate(pFoilplanetPort->loadedResource);
        pFoilplanetPort->loadedResource = NULL;
        OSAL_SemaphoreTerminate(pFoilplanetPort->unloadedResource);
        pFoilplanetPort->unloadedResource = NULL;
        mpp_free(pFoilplanetPort->bufferStateAllocate);
        pFoilplanetPort->bufferStateAllocate = NULL;
        mpp_free(pFoilplanetPort->extendBufferHeader);
        pFoilplanetPort->extendBufferHeader = NULL;

        delete GET_MPPLIST(pFoilplanetPort->bufferQ);
        delete GET_MPPLIST(pFoilplanetPort->securebufferQ);
    }
    mpp_free(pFpComponent->pFoilplanetPort);
    pFpComponent->pFoilplanetPort = NULL;
    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_ResetDataBuffer(FP_OMX_DATABUFFER *pDataBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pDataBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pDataBuffer->dataValid     = OMX_FALSE;
    pDataBuffer->dataLen       = 0;
    pDataBuffer->remainDataLen = 0;
    pDataBuffer->usedDataLen   = 0;
    pDataBuffer->bufferHeader  = NULL;
    pDataBuffer->nFlags        = 0;
    pDataBuffer->timeStamp     = 0;
    pDataBuffer->pPrivate      = NULL;

EXIT:
    return ret;
}

OMX_ERRORTYPE FP_ResetCodecData(FP_OMX_DATA *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pData->dataLen       = 0;
    pData->usedDataLen   = 0;
    pData->remainDataLen = 0;
    pData->nFlags        = 0;
    pData->timeStamp     = 0;
    pData->pPrivate      = NULL;
    pData->bufferHeader  = NULL;
    pData->allocSize     = 0;

EXIT:
    return ret;
}

OMX_ERRORTYPE FP_Shared_BufferToData(FP_OMX_DATABUFFER *pUseBuffer, FP_OMX_DATA *pData, FP_OMX_PLANE nPlane)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (nPlane == ONE_PLANE) {
        /* Case of Shared Buffer, Only support singlePlaneBuffer */
        pData->buffer.singlePlaneBuffer.dataBuffer = pUseBuffer->bufferHeader->pBuffer;
    } else {
        mpp_err("Can not support plane");
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    }

    pData->allocSize     = pUseBuffer->allocSize;
    pData->dataLen       = pUseBuffer->dataLen;
    pData->usedDataLen   = pUseBuffer->usedDataLen;
    pData->remainDataLen = pUseBuffer->remainDataLen;
    pData->timeStamp     = pUseBuffer->timeStamp;
    pData->nFlags        = pUseBuffer->nFlags;
    pData->pPrivate      = pUseBuffer->pPrivate;
    pData->bufferHeader  = pUseBuffer->bufferHeader;

EXIT:
    return ret;
}

OMX_ERRORTYPE FP_Shared_DataToBuffer(FP_OMX_DATA *pData, FP_OMX_DATABUFFER *pUseBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    pUseBuffer->bufferHeader    = pData->bufferHeader;
    pUseBuffer->allocSize       = pData->allocSize;
    pUseBuffer->dataLen         = pData->dataLen;
    pUseBuffer->usedDataLen     = pData->usedDataLen;
    pUseBuffer->remainDataLen   = pData->remainDataLen;
    pUseBuffer->timeStamp       = pData->timeStamp;
    pUseBuffer->nFlags          = pData->nFlags;
    pUseBuffer->pPrivate        = pData->pPrivate;

    return ret;
}
