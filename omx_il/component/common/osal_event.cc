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
#include <errno.h>
#include <pthread.h>

#include "OMX_Def.h"

#include "osal_event.h"
#include "osal/mpp_thread.h"
#include "osal/mpp_log.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG         "OSAL_EVT"
#endif

typedef struct _OSAL_THREADEVENT {
    OMX_BOOL       signal;
    OMX_HANDLETYPE mutex;
    pthread_cond_t condition;
} OSAL_THREADEVENT;

OMX_ERRORTYPE OSAL_SignalCreate(OMX_HANDLETYPE *eventHandle)
{
    OSAL_THREADEVENT *event;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    event = (OSAL_THREADEVENT *)malloc(sizeof(OSAL_THREADEVENT));
    if (!event) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    memset(event, 0, sizeof(OSAL_THREADEVENT));
    event->signal = OMX_FALSE;

    event->mutex = MUTEX_CREATE();

    if (pthread_cond_init(&event->condition, NULL)) {
        MUTEX_FREE(event->mutex);
        free(event);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    *eventHandle = (OMX_HANDLETYPE)event;
    ret = OMX_ErrorNone;

EXIT:
    return ret;
}

OMX_ERRORTYPE OSAL_SignalTerminate(OMX_HANDLETYPE eventHandle)
{
    OSAL_THREADEVENT *event = (OSAL_THREADEVENT *)eventHandle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (!event) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    MUTEX_LOCK(event->mutex);

    if (pthread_cond_destroy(&event->condition)) {
        ret = OMX_ErrorUndefined;
        MUTEX_UNLOCK(event->mutex);
        goto EXIT;
    }

    MUTEX_UNLOCK(event->mutex);

    MUTEX_FREE(event->mutex);

    free(event);

EXIT:
    return ret;
}

OMX_ERRORTYPE OSAL_SignalReset(OMX_HANDLETYPE eventHandle)
{
    OSAL_THREADEVENT *event = (OSAL_THREADEVENT *)eventHandle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (!event) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    MUTEX_LOCK(event->mutex);

    event->signal = OMX_FALSE;

    MUTEX_UNLOCK(event->mutex);

EXIT:
    return ret;
}

OMX_ERRORTYPE OSAL_SignalSet(OMX_HANDLETYPE eventHandle)
{
    OSAL_THREADEVENT *event = (OSAL_THREADEVENT *)eventHandle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (!event) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    MUTEX_LOCK(event->mutex);

    event->signal = OMX_TRUE;
    pthread_cond_signal(&event->condition);

    MUTEX_UNLOCK(event->mutex);

EXIT:
    return ret;
}

OMX_ERRORTYPE OSAL_SignalWait(OMX_HANDLETYPE eventHandle, OMX_U32 ms)
{
    OSAL_THREADEVENT *event = (OSAL_THREADEVENT *)eventHandle;
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    struct timespec       timeout;
    struct timeval        now;
    int                   funcret = 0;
    OMX_U32               tv_us;

    FunctionIn();

    if (!event) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    gettimeofday(&now, NULL);

    tv_us = now.tv_usec + ms * 1000;
    timeout.tv_sec = now.tv_sec + tv_us / 1000000;
    timeout.tv_nsec = (tv_us % 1000000) * 1000;

    MUTEX_LOCK(event->mutex);

    if (ms == 0) {
        if (!event->signal)
            ret = OMX_ErrorTimeout;
    } else if (ms == DEF_MAX_WAIT_TIME) {
        while (!event->signal)
            pthread_cond_wait(&event->condition, (pthread_mutex_t *)(event->mutex));
        ret = OMX_ErrorNone;
    } else {
        while (!event->signal) {
            funcret = pthread_cond_timedwait(&event->condition, (pthread_mutex_t *)(event->mutex), &timeout);
            if ((!event->signal) && (funcret == ETIMEDOUT)) {
                ret = OMX_ErrorTimeout;
                break;
            }
        }
    }

    MUTEX_UNLOCK(event->mutex);

EXIT:
    FunctionOut();

    return ret;
}


/* ---- semaphore -----*/
OMX_ERRORTYPE OSAL_SemaphoreCreate(OMX_HANDLETYPE *semaphoreHandle)
{
    sem_t *sema;

    sema = (sem_t *)malloc(sizeof(sem_t));
    if (!sema)
        return OMX_ErrorInsufficientResources;

    if (sem_init(sema, 0, 0) != 0) {
        free(sema);
        return OMX_ErrorUndefined;
    }

    *semaphoreHandle = (OMX_HANDLETYPE)sema;

    mpp_log("OSAL_SemaphorePost %p", sema);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OSAL_SemaphoreTerminate(OMX_HANDLETYPE semaphoreHandle)
{
    sem_t *sema = (sem_t *)semaphoreHandle;

    if (sema == NULL)
        return OMX_ErrorBadParameter;

    if (sem_destroy(sema) != 0)
        return OMX_ErrorUndefined;

    free(sema);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OSAL_SemaphoreWait(OMX_HANDLETYPE semaphoreHandle)
{
    mpp_log("OSAL_SemaphoreWait %p", semaphoreHandle);
    sem_t *sema = (sem_t *)semaphoreHandle;

    FunctionIn();

    if (sema == NULL)
        return OMX_ErrorBadParameter;

    if (sem_wait(sema) != 0)
        return OMX_ErrorUndefined;

    FunctionOut();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OSAL_SemaphorePost(OMX_HANDLETYPE semaphoreHandle)
{

    // omx_err("OSAL_SemaphorePost %p",semaphoreHandle);
    sem_t *sema = (sem_t *)semaphoreHandle;

    FunctionIn();

    if (sema == NULL)
        return OMX_ErrorBadParameter;

    if (sem_post(sema) != 0)
        return OMX_ErrorUndefined;

    FunctionOut();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OSAL_Set_SemaphoreCount(OMX_HANDLETYPE semaphoreHandle, OMX_S32 val)
{
    sem_t *sema = (sem_t *)semaphoreHandle;

    if (sema == NULL)
        return OMX_ErrorBadParameter;

    if (sem_init(sema, 0, val) != 0)
        return OMX_ErrorUndefined;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OSAL_Get_SemaphoreCount(OMX_HANDLETYPE semaphoreHandle, OMX_S32 *val)
{
    sem_t *sema = (sem_t *)semaphoreHandle;
    int semaVal = 0;

    if (sema == NULL)
        return OMX_ErrorBadParameter;

    if (sem_getvalue(sema, &semaVal) != 0)
        return OMX_ErrorUndefined;

    *val = (OMX_S32)semaVal;

    return OMX_ErrorNone;
}
