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

/**
 * @brief   Foilplanet_OMX specific define
 */

#ifndef _FP_OMX_DEF_
#define _FP_OMX_DEF_

#include "OMX_Types.h"
#include "OMX_IVCommon.h"

#define VERSIONMAJOR_NUMBER             1
#define VERSIONMINOR_NUMBER             0
#define REVISION_NUMBER                 4
#define STEP_NUMBER                     0

#define OMX_COMPILE_INFO                "anbox-20200501"

#define MAX_OMX_COMPONENT_NUM           28
#define MAX_OMX_COMPONENT_ROLE_NUM      20
#define MAX_OMX_COMPONENT_NAME_SIZE     OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_ROLE_SIZE     OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_LIBNAME_SIZE  (OMX_MAX_STRINGNAME_SIZE * 2)
#define MAX_OMX_MIMETYPE_SIZE           OMX_MAX_STRINGNAME_SIZE

#define MAX_TIMESTAMP                   40
#define MAX_FLAGS                       40
#define MAX_BUFFER_REF                  40

#define MAX_BUFFER_PLANE                1

#define FOILPLANET_OMX_INSTALL_PATH     "/system/lib/"

#define OMX_COLORSPACE_MASK             (0x00f00000)
#define OMX_DYNCRANGE_MASK              (0x0f000000)

/* note: must sync with gralloc */
typedef enum _ANB_PRIVATE_BUF_TYPE {
    ANB_PRIVATE_BUF_NONE    = 0,
    ANB_PRIVATE_BUF_VIRTUAL = 0x01,
    ANB_PRIVATE_BUF_BUTT,
} ANB_PRIVATE_BUF_TYPE;

typedef enum _FOILPLANET_CODEC_TYPE {
    SW_CODEC,
    HW_VIDEO_DEC_CODEC,
    HW_VIDEO_ENC_CODEC,
} FOILPLANET_CODEC_TYPE;

typedef struct _FP_OMX_PRIORITYMGMTTYPE {
    OMX_U32 nGroupPriority; /* the value 0 represents the highest priority */
    /* for a group of components                   */
    OMX_U32 nGroupID;
} FP_OMX_PRIORITYMGMTTYPE;

/*set when wfd*/
typedef struct _FP_OMX_WFD {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
} FP_OMX_WFD;

typedef struct _OMX_VIDEO_PARAMS_EXTENDED {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 ui32Flags;
    OMX_BOOL bEnableScaling; // Resolution Scaling
    OMX_U16 ui16ScaledWidth;
    OMX_U16 ui16ScaledHeight;
    OMX_BOOL bEnableCropping; // Resolution Cropping
    OMX_U16 ui16CropLeft;//Number of columns to be cropped from lefthand-side edge
    OMX_U16 ui16CropRight;//Number of columns to be cropped from righthand-side edge
    OMX_U16 ui16CropTop;//Number of rows to be cropped from the top edge
    OMX_U16 ui16CropBottom;// Number of rows to be cropped from the bottom edge
} OMX_VIDEO_PARAMS_EXTENDED;

typedef enum _FP_OMX_INDEXTYPE {

#define FOILPLANET_INDEX_PARAM_ENABLE_THUMBNAIL "OMX.SEC.index.enableThumbnailMode"
    OMX_IndexParamEnableThumbnailMode       = 0x7F000001,
#define FOILPLANET_INDEX_CONFIG_VIDEO_INTRAPERIOD "OMX.SEC.index.VideoIntraPeriod"
    OMX_IndexConfigVideoIntraPeriod         = 0x7F000002,

    /* for Android Native Window */
#define FOILPLANET_INDEX_PARAM_ENABLE_ANB   "OMX.google.android.index.enableAndroidNativeBuffers"
    OMX_IndexParamEnableAndroidBuffers      = 0x7F000011,
#define FOILPLANET_INDEX_PARAM_GET_ANB_Usage "OMX.google.android.index.getAndroidNativeBufferUsage"
    OMX_IndexParamGetAndroidNativeBufferUsage    = 0x7F000012,
#define FOILPLANET_INDEX_PARAM_USE_ANB      "OMX.google.android.index.useAndroidNativeBuffer2"
    OMX_IndexParamUseAndroidNativeBuffer    = 0x7F000013,
    /* for Android Store Metadata Inbuffer */
#define FOILPLANET_INDEX_PARAM_STORE_METADATA_BUFFER "OMX.google.android.index.storeMetaDataInBuffers"
    OMX_IndexParamStoreMetaDataBuffer       = 0x7F000014,
    /* prepend SPS/PPS to I/IDR for H.264 Encoder */
#define FOILPLANET_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR "OMX.google.android.index.prependSPSPPSToIDRFrames"
    OMX_IndexParamPrependSPSPPSToIDR        = 0x7F000015,
#define FOILPLANET_INDEX_PARAM_RKWFD        "OMX.fp.index.encoder.wifidisplay"
    OMX_IndexRkEncExtendedWfdState          = 0x7F000018,

#define FOILPLANET_INDEX_PREPARE_ADAPTIVE_PLAYBACK  "OMX.google.android.index.prepareForAdaptivePlayback"
    OMX_IndexParamprepareForAdaptivePlayback    = 0x7F000016,

#define FOILPLANET_INDEX_DESCRIBE_COLORFORMAT   "OMX.google.android.index.describeColorFormat"
    OMX_IndexParamdescribeColorFormat           = 0x7F000017,

#define FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_DIV3 "OMX.fp.index.decoder.extension.div3"
    OMX_IndexParamRkDecoderExtensionDiv3        = 0x7F050000,
#define FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_USE_DTS "OMX.fp.index.decoder.extension.useDts"
    OMX_IndexParamRkDecoderExtensionUseDts      = 0x7F050001,
#define FOILPLANET_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_THUMBNAIL "OMX.fp.index.decoder.extension.thumbNail"
    OMX_IndexParamRkDecoderExtensionThumbNail   = 0x7F050002,

#define FOILPLANET_INDEX_PARAM_EXTENDED_VIDEO "OMX.Topaz.index.param.extended_video"
    OMX_IndexParamRkEncExtendedVideo        = 0x7F050003,

#define FOILPLANET_INDEX_PARAM_DSECRIBECOLORASPECTS "OMX.google.android.index.describeColorAspects"
    OMX_IndexParamRkDescribeColorAspects    = 0x7F000062,

#define FOILPLANET_INDEX_PARAM_ALLOCATENATIVEHANDLE "OMX.google.android.index.allocateNativeHandle"
    OMX_IndexParamAllocateNativeHandle      = 0x7F00005D,

    /* for Android PV OpenCore*/
    OMX_COMPONENT_CAPABILITY_TYPE_INDEX     = 0xFF7A347
} FP_OMX_INDEXTYPE;

typedef enum _FP_OMX_ERRORTYPE {
    OMX_ErrorNoEOF              = (OMX_S32) 0x90000001,
    OMX_ErrorInputDataDecodeYet = (OMX_S32) 0x90000002,
    OMX_ErrorInputDataEncodeYet = (OMX_S32) 0x90000003,
    OMX_ErrorCodecInit          = (OMX_S32) 0x90000004,
    OMX_ErrorCodecDecode        = (OMX_S32) 0x90000005,
    OMX_ErrorCodecEncode        = (OMX_S32) 0x90000006,
    OMX_ErrorCodecFlush         = (OMX_S32) 0x90000007,
    OMX_ErrorOutputBufferUseYet = (OMX_S32) 0x90000008
} FP_OMX_ERRORTYPE;

typedef enum _FP_OMX_COMMANDTYPE {
    FP_OMX_CommandComponentDeInit = 0x7F000001,
    FP_OMX_CommandEmptyBuffer,
    FP_OMX_CommandFillBuffer,
    FP_OMX_CommandFakeBuffer
} FP_OMX_COMMANDTYPE;

typedef enum _FP_OMX_TRANS_STATETYPE {
    FP_OMX_TransStateInvalid,
    FP_OMX_TransStateLoadedToIdle,
    FP_OMX_TransStateIdleToExecuting,
    FP_OMX_TransStateExecutingToIdle,
    FP_OMX_TransStateIdleToLoaded,
    FP_OMX_TransStateMax = 0X7FFFFFFF
} FP_OMX_TRANS_STATETYPE;

typedef enum _FP_OMX_COLOR_FORMATTYPE {

    /* to copy a encoded data for drm component using gsc or fimc */
    OMX_SEC_COLOR_FormatEncodedData     = OMX_COLOR_FormatYCbYCr,
    /* for Android SurfaceMediaSource*/
    OMX_COLOR_FormatAndroidOpaque       = 0x7F000789,
    OMX_COLOR_FormatYUV420Flexible      = 0x7F420888
} FP_OMX_COLOR_FORMATTYPE;

typedef enum _FP_OMX_SUPPORTFORMAT_TYPE {
    supportFormat_0 = 0x00,
    supportFormat_1,
    supportFormat_2,
    supportFormat_3,
    supportFormat_4,
    supportFormat_5,
    supportFormat_6,
    supportFormat_7
} FP_OMX_SUPPORTFORMAT_TYPE;

typedef enum _FP_OMX_BUFFERPROCESS_TYPE {
    BUFFER_DEFAULT  = 0x00,
    BUFFER_COPY     = 0x01,
    BUFFER_SHARE    = 0x02,
    BUFFER_METADATA = 0x04,
    BUFFER_ANBSHARE = 0x08
} FP_OMX_BUFFERPROCESS_TYPE;

typedef struct _FP_OMX_VIDEO_PROFILELEVEL {
    OMX_S32         profile;
    OMX_S32         level;
} FP_OMX_VIDEO_PROFILELEVEL;

typedef struct _FP_OMX_VIDEO_THUMBNAILMODE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnable;
} FP_OMX_VIDEO_THUMBNAILMODE;

typedef enum _FOILPLANET_VIDEO_CODINGTYPE_EXT {
    OMX_VIDEO_CodingFLV1 = 0x01000000,      /**< Sorenson H.263 */
    OMX_VIDEO_CodingDIVX3,                  /**< DIVX3 */
    OMX_VIDEO_CodingVP6,                    /**< VP6 */
    OMX_VIDEO_CodingVC1,                    /**< VP6 */
    OMX_VIDEO_OLD_CodingHEVC,
} FOILPLANET_VIDEO_CODINGTYPE_EXT;

typedef enum {
    UNSUPPORT_PROFILE           = -1,
    BASELINE_PROFILE            = 66,
    MAIN_PROFILE                = 77,
    HIGHT_PROFILE               = 100,
} EncProfile;

typedef enum {
    HEVC_UNSUPPORT_PROFILE      = -1,
    HEVC_MAIN_PROFILE           = 0x1,
    HEVC_MAIN10_PROFILE         = 0x2,
    HEVC_MAIN10HDR10_PROFILE    = 0x1000,
} HEVCEncProfile;

typedef enum {
    UNSUPPORT_BITMODE           = -1,
    Video_RC_Mode_Disable       = 0,
    Video_RC_Mode_VBR           = 1,
    Video_RC_Mode_CBR           = 2,
} EncRCMode;

typedef struct _FoilplanetVideoPlane {
    void     *addr;
    OMX_U32   allocSize;
    OMX_U32   dataSize;
    OMX_U32   offset;
    OMX_S32   fd;
    OMX_S32   type;
    OMX_U32   stride;
} FoilplanetVideoPlane;


/* TO REMOVE */
typedef void *OMX_QUEUE;
#define FunctionIn()    ((void *)0)
#define FunctionOut()   ((void *)0)

#ifndef __OMX_EXPORTS
#define __OMX_EXPORTS
#define FOILPLANET_EXPORT_REF __attribute__((visibility("default")))
#define FOILPLANET_IMPORT_REF __attribute__((visibility("default")))
#endif

#endif /* _FP_OMX_DEF_ */
