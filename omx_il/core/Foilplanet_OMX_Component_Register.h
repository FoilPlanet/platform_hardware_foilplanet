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

#ifndef _FOILPLANET_OMX_COMPONENT_REG_H_
#define _FOILPLANET_OMX_COMPONENT_REG_H_

#include "OMX_Def.h"
#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Component.h"

typedef struct _FoilplanetRegisterComponentType {
    OMX_U8  componentName[MAX_OMX_COMPONENT_NAME_SIZE];
    OMX_U8  roles[MAX_OMX_COMPONENT_ROLE_NUM][MAX_OMX_COMPONENT_ROLE_SIZE];
    OMX_U32 totalRoleNum;
} FoilplanetRegisterComponentType;

typedef struct _FOILPLANET_OMX_COMPONENT_REGLIST {
    FoilplanetRegisterComponentType component;
    OMX_U8  libName[MAX_OMX_COMPONENT_LIBNAME_SIZE];
} FOILPLANET_OMX_COMPONENT_REGLIST;

struct _FOILPLANET_OMX_COMPONENT;
typedef struct _FOILPLANET_OMX_COMPONENT {
    OMX_U8                          componentName[MAX_OMX_COMPONENT_NAME_SIZE];
    OMX_U8                          libName[MAX_OMX_COMPONENT_LIBNAME_SIZE];
    OMX_HANDLETYPE                  libHandle;
    OMX_STRING                      rkversion;
    OMX_COMPONENTTYPE               *pOMXComponent;
    struct _FOILPLANET_OMX_COMPONENT  *nextOMXComp;
} FOILPLANET_OMX_COMPONENT;

typedef struct _FOILPLANET_COMPONENT_INFO {
    const char *comp_type;
    const char *lib_name;
} FOILPLANET_COMPONENT_INFO;

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Foilplanet_OMX_Component_Register(FOILPLANET_OMX_COMPONENT_REGLIST **compList, OMX_U32 *compNum);
OMX_ERRORTYPE Foilplanet_OMX_Component_Unregister(FOILPLANET_OMX_COMPONENT_REGLIST *componentList);
OMX_ERRORTYPE Foilplanet_OMX_ComponentLoad(FOILPLANET_OMX_COMPONENT *rockchip_component);
OMX_ERRORTYPE Foilplanet_OMX_ComponentUnload(FOILPLANET_OMX_COMPONENT *rockchip_component);

#ifdef __cplusplus
}
#endif

#endif /* _FOILPLANET_OMX_COMPONENT_REG_H_ */
