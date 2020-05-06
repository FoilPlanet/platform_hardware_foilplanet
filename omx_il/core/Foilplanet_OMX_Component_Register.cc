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
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "OMX_Component.h"
#include "OMX_Macros.h"

#include "osal_event.h"
#include "osal/mpp_log.h"
#include "osal/mpp_mem.h"

#include "Foilplanet_OMX_Component_Register.h"
#include "Foilplanet_OMX_Resourcemanager.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG     "FP_OMX_REGS"
#endif

static const FOILPLANET_COMPONENT_INFO kCompInfo[] = {
    { "fp.omx_dec", "libomxvpu_dec.so" },
    { "fp.omx_enc", "libomxvpu_enc.so" },
};

OMX_ERRORTYPE Foilplanet_OMX_Component_Register(FOILPLANET_OMX_COMPONENT_REGLIST **compList, OMX_U32 *compNum)
{
    OMX_ERRORTYPE  ret = OMX_ErrorNone;
    int            componentNum = 0, roleNum = 0, totalCompNum = 0;
    int            i = 0;
    const char    *errorMsg;
    int kNumEntries = sizeof(kCompInfo) / sizeof(kCompInfo[0]);

    int (*Foilplanet_OMX_COMPONENT_Library_Register)(FoilplanetRegisterComponentType **rockchipComponents);
    FoilplanetRegisterComponentType **fpComponentsTemp;
    FOILPLANET_OMX_COMPONENT_REGLIST *componentList;

    FunctionIn();

    componentList = mpp_malloc(FOILPLANET_OMX_COMPONENT_REGLIST, MAX_OMX_COMPONENT_NUM);
    memset(componentList, 0, sizeof(FOILPLANET_OMX_COMPONENT_REGLIST) * MAX_OMX_COMPONENT_NUM);

    for (i = 0; i < kNumEntries; i++) {
        FOILPLANET_COMPONENT_INFO com_inf = kCompInfo[i];
        OMX_PTR soHandle = NULL;
        if ((soHandle = dlopen(com_inf.lib_name, RTLD_NOW)) != NULL) {
            dlerror();    /* clear error*/
            if ((Foilplanet_OMX_COMPONENT_Library_Register = (int (*)(FoilplanetRegisterComponentType **))dlsym(soHandle, "Foilplanet_OMX_COMPONENT_Library_Register")) != NULL) {
                int i = 0;
                unsigned int j = 0;
                componentNum = (*Foilplanet_OMX_COMPONENT_Library_Register)(NULL);
                fpComponentsTemp = mpp_malloc(FoilplanetRegisterComponentType*, componentNum);
                for (i = 0; i < componentNum; i++) {
                    fpComponentsTemp[i] = mpp_malloc(FoilplanetRegisterComponentType, 1);
                    memset(fpComponentsTemp[i], 0, sizeof(FoilplanetRegisterComponentType));
                }
                (*Foilplanet_OMX_COMPONENT_Library_Register)(fpComponentsTemp);

                for (i = 0; i < componentNum; i++) {
                    strcpy((char *)&componentList[totalCompNum].component.componentName[0], (char *)&fpComponentsTemp[i]->componentName[0]);
                    for (j = 0; j < fpComponentsTemp[i]->totalRoleNum; j++)
                        strcpy((char *)&componentList[totalCompNum].component.roles[j][0], (char *)&fpComponentsTemp[i]->roles[j][0]);
                    componentList[totalCompNum].component.totalRoleNum = fpComponentsTemp[i]->totalRoleNum;

                    strcpy((char *)&componentList[totalCompNum].libName[0], com_inf.lib_name);

                    totalCompNum++;
                }
                for (i = 0; i < componentNum; i++) {
                    mpp_free(fpComponentsTemp[i]);
                }

                mpp_free(fpComponentsTemp);
            } else {
                if ((errorMsg = dlerror()) != NULL)
                    mpp_log("dlsym failed: %s", errorMsg);
            }
            dlclose(soHandle);
        }
    }
    *compList = componentList;
    *compNum = totalCompNum;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Foilplanet_OMX_Component_Unregister(FOILPLANET_OMX_COMPONENT_REGLIST *componentList)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    mpp_free(componentList);

EXIT:
    return ret;
}

OMX_ERRORTYPE Foilplanet_OMX_ComponentAPICheck(OMX_COMPONENTTYPE *component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if ((NULL == component->GetComponentVersion)    ||
        (NULL == component->SendCommand)            ||
        (NULL == component->GetParameter)           ||
        (NULL == component->SetParameter)           ||
        (NULL == component->GetConfig)              ||
        (NULL == component->SetConfig)              ||
        (NULL == component->GetExtensionIndex)      ||
        (NULL == component->GetState)               ||
        (NULL == component->ComponentTunnelRequest) ||
        (NULL == component->UseBuffer)              ||
        (NULL == component->AllocateBuffer)         ||
        (NULL == component->FreeBuffer)             ||
        (NULL == component->EmptyThisBuffer)        ||
        (NULL == component->FillThisBuffer)         ||
        (NULL == component->SetCallbacks)           ||
        (NULL == component->ComponentDeInit)        ||
        (NULL == component->UseEGLImage)            ||
        (NULL == component->ComponentRoleEnum))
        ret = OMX_ErrorInvalidComponent;
    else
        ret = OMX_ErrorNone;

    return ret;
}

OMX_ERRORTYPE Foilplanet_OMX_ComponentLoad(FOILPLANET_OMX_COMPONENT *rockchip_component)
{
    OMX_ERRORTYPE      ret = OMX_ErrorNone;
    OMX_HANDLETYPE     libHandle;
    OMX_COMPONENTTYPE *pOMXComponent;

    FunctionIn();

    OMX_ERRORTYPE (*Foilplanet_OMX_ComponentConstructor)(OMX_HANDLETYPE hComponent, OMX_STRING componentName);

    libHandle = dlopen((OMX_STRING)rockchip_component->libName, RTLD_NOW);
    if (!libHandle) {
        ret = OMX_ErrorInvalidComponentName;
        mpp_err("OMX_ErrorInvalidComponentName, Line:%d", __LINE__);
        goto EXIT;
    }

    Foilplanet_OMX_ComponentConstructor = (OMX_ERRORTYPE (*)(OMX_HANDLETYPE, OMX_STRING))dlsym(libHandle, "Foilplanet_OMX_ComponentConstructor");
    if (!Foilplanet_OMX_ComponentConstructor) {
        dlclose(libHandle);
        ret = OMX_ErrorInvalidComponent;
        mpp_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)malloc(sizeof(OMX_COMPONENTTYPE));
    INIT_SET_SIZE_VERSION(pOMXComponent, OMX_COMPONENTTYPE);
    ret = (*Foilplanet_OMX_ComponentConstructor)((OMX_HANDLETYPE)pOMXComponent, (OMX_STRING)rockchip_component->componentName);
    if (ret != OMX_ErrorNone) {
        mpp_free(pOMXComponent);
        dlclose(libHandle);
        ret = OMX_ErrorInvalidComponent;
        mpp_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
        goto EXIT;
    } else {
        if (Foilplanet_OMX_ComponentAPICheck(pOMXComponent) != OMX_ErrorNone) {
            if (NULL != pOMXComponent->ComponentDeInit)
                pOMXComponent->ComponentDeInit(pOMXComponent);
            mpp_free(pOMXComponent);
            dlclose(libHandle);
            ret = OMX_ErrorInvalidComponent;
            mpp_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
            goto EXIT;
        }
        rockchip_component->libHandle = libHandle;
        rockchip_component->pOMXComponent = pOMXComponent;
        rockchip_component->rkversion = &OMX_version[0];
        ret = OMX_ErrorNone;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Foilplanet_OMX_ComponentUnload(FOILPLANET_OMX_COMPONENT *rockchip_component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pOMXComponent = NULL;

    FunctionIn();

    if (!rockchip_component) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = rockchip_component->pOMXComponent;
    if (pOMXComponent != NULL) {
        pOMXComponent->ComponentDeInit(pOMXComponent);
        mpp_free(pOMXComponent);
        rockchip_component->pOMXComponent = NULL;
    }

    if (rockchip_component->libHandle != NULL) {
        dlclose(rockchip_component->libHandle);
        rockchip_component->libHandle = NULL;
    }

EXIT:
    FunctionOut();

    return ret;
}

