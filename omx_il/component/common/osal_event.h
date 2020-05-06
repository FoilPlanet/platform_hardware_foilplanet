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

#ifndef _OSAL_EVENT_H_
#define _OSAL_EVENT_H_

#include <pthread.h>
#include "OMX_Types.h"
#include "OMX_Core.h"

#define DEF_MAX_WAIT_TIME 0xFFFFFFFF

#define MUTEX_LOCK(h)       ((Mutex *)(h))->lock()
#define MUTEX_UNLOCK(h)     ((Mutex *)(h))->unlock()
#define MUTEX_FREE(h)       (delete (Mutex *)(h))
#define MUTEX_CREATE()      (Mutex *)(new Mutex())

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE OSAL_SignalCreate(OMX_HANDLETYPE *eventHandle);
OMX_ERRORTYPE OSAL_SignalTerminate(OMX_HANDLETYPE eventHandle);
OMX_ERRORTYPE OSAL_SignalReset(OMX_HANDLETYPE eventHandle);
OMX_ERRORTYPE OSAL_SignalSet(OMX_HANDLETYPE eventHandle);
OMX_ERRORTYPE OSAL_SignalWait(OMX_HANDLETYPE eventHandle, OMX_U32 ms);

OMX_ERRORTYPE OSAL_SemaphoreCreate(OMX_HANDLETYPE *semaphoreHandle);
OMX_ERRORTYPE OSAL_SemaphoreTerminate(OMX_HANDLETYPE semaphoreHandle);
OMX_ERRORTYPE OSAL_SemaphoreWait(OMX_HANDLETYPE semaphoreHandle);
OMX_ERRORTYPE OSAL_SemaphorePost(OMX_HANDLETYPE semaphoreHandle);
OMX_ERRORTYPE OSAL_Set_SemaphoreCount(OMX_HANDLETYPE semaphoreHandle, OMX_S32 val);
OMX_ERRORTYPE OSAL_Get_SemaphoreCount(OMX_HANDLETYPE semaphoreHandle, OMX_S32 *val);

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_EVENT_H_*/
