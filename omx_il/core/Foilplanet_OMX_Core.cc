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
#include <pthread.h>

#include "Foilplanet_OMX_Core.h"
#include "Foilplanet_OMX_Component_Register.h"
#include "Foilplanet_OMX_Resourcemanager.h"

#include "osal_event.h"
#include "osal/mpp_log.h"
#include "osal/mpp_mem.h"
#include "osal/mpp_thread.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG     "FP_OMX_CORE"
#endif

static int gInitialized = 0;
static OMX_U32 gComponentNum = 0;
static OMX_U32 gCount = 0;
static FOILPLANET_OMX_COMPONENT_REGLIST *gComponentList = NULL;
static FOILPLANET_OMX_COMPONENT *gLoadComponentList = NULL;
static OMX_HANDLETYPE ghLoadComponentListMutex = NULL;

Mutex *gMutex = new Mutex();

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_Init(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();
    MUTEX_LOCK(gMutex);
    gCount++;
    if (gInitialized == 0) {
        if (Foilplanet_OMX_Component_Register(&gComponentList, &gComponentNum)) {
            ret = OMX_ErrorInsufficientResources;
            mpp_err("Foilplanet_OMX_Init : %s", "OMX_ErrorInsufficientResources");
            goto EXIT;
        }

        ret = FP_OMX_ResourceManager_Init();
        if (OMX_ErrorNone != ret) {
            mpp_err("Foilplanet_OMX_Init : FP_OMX_ResourceManager_Init failed");
            goto EXIT;
        }

        ghLoadComponentListMutex = MUTEX_CREATE();
        gInitialized = 1;
        mpp_log("Foilplanet_OMX_Init : %s", "OMX_ErrorNone");
    }

EXIT:

    MUTEX_UNLOCK(gMutex);
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_DeInit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();
    MUTEX_LOCK(gMutex);
    gCount--;
    if (gCount == 0) {
        MUTEX_FREE(ghLoadComponentListMutex);
        ghLoadComponentListMutex = NULL;

        FP_OMX_ResourceManager_Deinit();

        if (OMX_ErrorNone != Foilplanet_OMX_Component_Unregister(gComponentList)) {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
        gComponentList = NULL;
        gComponentNum = 0;
        gInitialized = 0;
    }
EXIT:
    MUTEX_UNLOCK(gMutex);
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if (nIndex >= gComponentNum) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    snprintf(cComponentName, nNameLength, "%s", gComponentList[nIndex].component.componentName);
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE *pHandle,
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE *pCallBacks)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FOILPLANET_OMX_COMPONENT *loadComponent;
    FOILPLANET_OMX_COMPONENT *currentComponent;
    unsigned int i = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    if ((pHandle == NULL) || (cComponentName == NULL) || (pCallBacks == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    mpp_log("ComponentName : %s", cComponentName);

    for (i = 0; i < gComponentNum; i++) {
        if (strcmp(cComponentName, (char *)&gComponentList[i].component.componentName[0]) == 0) {
            loadComponent = mpp_malloc(FOILPLANET_OMX_COMPONENT, 1);
            memset(loadComponent, 0, sizeof(FOILPLANET_OMX_COMPONENT));

            strcpy((char *)loadComponent->libName, (const char *)gComponentList[i].libName);
            strcpy((char *)loadComponent->componentName, (const char *)gComponentList[i].component.componentName);
            ret = Foilplanet_OMX_ComponentLoad(loadComponent);
            if (ret != OMX_ErrorNone) {
                free(loadComponent);
                mpp_err("OMX_Error, Line:%d", __LINE__);
                goto EXIT;
            }

            ret = loadComponent->pOMXComponent->SetCallbacks(loadComponent->pOMXComponent, pCallBacks, pAppData);
            if (ret != OMX_ErrorNone) {
                Foilplanet_OMX_ComponentUnload(loadComponent);
                free(loadComponent);
                mpp_err("OMX_Error 0x%x, Line:%d", ret, __LINE__);
                goto EXIT;
            }

            ret = FP_OMX_Check_Resource(loadComponent->pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Foilplanet_OMX_ComponentUnload(loadComponent);
                free(loadComponent);
                mpp_err("OMX_Error 0x%x, Line:%d", ret, __LINE__);

                goto EXIT;
            }
            MUTEX_LOCK(ghLoadComponentListMutex);
            if (gLoadComponentList == NULL) {
                gLoadComponentList = loadComponent;
            } else {
                currentComponent = gLoadComponentList;
                while (currentComponent->nextOMXComp != NULL) {
                    currentComponent = currentComponent->nextOMXComp;
                }
                currentComponent->nextOMXComp = loadComponent;
            }
            MUTEX_UNLOCK(ghLoadComponentListMutex);

            *pHandle = loadComponent->pOMXComponent;
            ret = OMX_ErrorNone;
            mpp_log("Foilplanet_OMX_GetHandle : %s", "OMX_ErrorNone");
            goto EXIT;
        }
    }

    ret = OMX_ErrorComponentNotFound;

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    FOILPLANET_OMX_COMPONENT *currentComponent = NULL;
    FOILPLANET_OMX_COMPONENT *deleteComponent = NULL;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    if (!hComponent) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    MUTEX_LOCK(ghLoadComponentListMutex);
    currentComponent = gLoadComponentList;
    if (gLoadComponentList->pOMXComponent == hComponent) {
        deleteComponent = gLoadComponentList;
        gLoadComponentList = gLoadComponentList->nextOMXComp;
    } else {
        while ((currentComponent != NULL) && (((FOILPLANET_OMX_COMPONENT *)(currentComponent->nextOMXComp))->pOMXComponent != hComponent))
            currentComponent = currentComponent->nextOMXComp;

        if (((FOILPLANET_OMX_COMPONENT *)(currentComponent->nextOMXComp))->pOMXComponent == hComponent) {
            deleteComponent = currentComponent->nextOMXComp;
            currentComponent->nextOMXComp = deleteComponent->nextOMXComp;
        } else if (currentComponent == NULL) {
            ret = OMX_ErrorComponentNotFound;
            MUTEX_UNLOCK(ghLoadComponentListMutex);
            goto EXIT;
        }
    }
    MUTEX_UNLOCK(ghLoadComponentListMutex);

    Foilplanet_OMX_ComponentUnload(deleteComponent);
    free(deleteComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY FP_OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput)
{
    OMX_ERRORTYPE ret = OMX_ErrorNotImplemented;
    (void)hOutput;
    (void)nPortOutput;
    (void)hInput;
    (void)nPortInput;
EXIT:
    return ret;
}

OMX_API OMX_ERRORTYPE FP_OMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE *hPipe,
    OMX_IN  OMX_STRING szURI)
{
    OMX_ERRORTYPE ret = OMX_ErrorNotImplemented;
    (void)hPipe;
    (void)szURI;
EXIT:
    return ret;
}

OMX_API OMX_ERRORTYPE FP_OMX_GetComponentsOfRole (
    OMX_IN    OMX_STRING role,
    OMX_INOUT OMX_U32 *pNumComps,
    OMX_INOUT OMX_U8  **compNames)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int           max_role_num = 0;
    OMX_STRING    RoleString[MAX_OMX_COMPONENT_ROLE_SIZE];
    int i = 0, j = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    *pNumComps = 0;

    for (i = 0; i < MAX_OMX_COMPONENT_NUM; i++) {
        max_role_num = gComponentList[i].component.totalRoleNum;

        for (j = 0; j < max_role_num; j++) {
            if (strcmp((const char *)gComponentList[i].component.roles[j], (const char *)role) == 0) {
                if (compNames != NULL) {
                    strcpy((OMX_STRING)compNames[*pNumComps], (const char *)gComponentList[i].component.componentName);
                }
                *pNumComps = (*pNumComps + 1);
            }
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE FP_OMX_GetRolesOfComponent (
    OMX_IN    OMX_STRING compName,
    OMX_INOUT OMX_U32 *pNumRoles,
    OMX_OUT   OMX_U8 **roles)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BOOL      detectComp = OMX_FALSE;
    int           compNum = 0, totalRoleNum = 0;
    int i = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    for (i = 0; i < MAX_OMX_COMPONENT_NUM; i++) {
        if (gComponentList != NULL) {
            if (strcmp((char *)&gComponentList[i].component.componentName[0], compName) == 0) {
                *pNumRoles = totalRoleNum = gComponentList[i].component.totalRoleNum;
                compNum = i;
                detectComp = OMX_TRUE;
                break;
            }
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }

    if (detectComp == OMX_FALSE) {
        *pNumRoles = 0;
        ret = OMX_ErrorComponentNotFound;
        goto EXIT;
    }

    if (roles != NULL) {
        for (i = 0; i < totalRoleNum; i++) {
            strcpy((char *)&roles[i][0], (char *)&gComponentList[compNum].component.roles[i][0]);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}
