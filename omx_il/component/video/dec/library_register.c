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

#include "library_register.h"
#include "OMX_Core.inc"

OSCL_EXPORT_REF int 
Foilplanet_OMX_COMPONENT_Library_Register(FoilplanetRegisterComponentType **fpComponents)
{
    if (fpComponents == NULL)
        return 0;

    for (uint32_t i = 0; i < SIZE_OF_DEC_CORE; i++) {
        strcpy((char *)fpComponents[i]->componentName, dec_core[i].compName);
        strcpy((char *)fpComponents[i]->roles[0], dec_core[i].roles);
        fpComponents[i]->totalRoleNum = MAX_COMPONENT_ROLE_NUM;
    }

    return SIZE_OF_DEC_CORE;
}