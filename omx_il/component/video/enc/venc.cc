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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <dlfcn.h>
#include <cutils/properties.h>

#include "OMX_Macros.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "venc.h"
#include "venc_control.h"

//#include <hardware/rga.h>
//#include "vpu_type.h"
//#include "gralloc_priv_omx.h"

#ifdef USE_ANB
#include "osal_android.h"
#include <hardware/hardware.h>
#endif

#include "osal_event.h"
#include "osal_rga.h"
#include "osal/mpp_thread.h"
#include "osal/mpp_list.h"
#include "osal/mpp_mem.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG     "FP_OMX_VDEC"
#endif

/** 
 * TO remove: old librkvpu/vpu_api.h
 */
typedef enum
{
    VPU_H264ENC_YUV420_PLANAR = 0,              /* YYYY... UUUU... VVVV */
    VPU_H264ENC_YUV420_SEMIPLANAR = 1,          /* YYYY... UVUVUV...    */
    VPU_H264ENC_YUV422_INTERLEAVED_YUYV = 2,    /* YUYVYUYV...          */
    VPU_H264ENC_YUV422_INTERLEAVED_UYVY = 3,    /* UYVYUYVY...          */
    VPU_H264ENC_RGB565 = 4,                     /* 16-bit RGB           */
    VPU_H264ENC_BGR565 = 5,                     /* 16-bit RGB           */
    VPU_H264ENC_RGB555 = 6,                     /* 15-bit RGB           */
    VPU_H264ENC_BGR555 = 7,                     /* 15-bit RGB           */
    VPU_H264ENC_RGB444 = 8,                     /* 12-bit RGB           */
    VPU_H264ENC_BGR444 = 9,                     /* 12-bit RGB           */
    VPU_H264ENC_RGB888 = 10,                    /* 24-bit RGB           */
    VPU_H264ENC_BGR888 = 11,                    /* 24-bit RGB           */
    VPU_H264ENC_RGB101010 = 12,                 /* 30-bit RGB           */
    VPU_H264ENC_BGR101010 = 13                  /* 30-bit RGB           */
} H264EncPictureType;

/* Using for the encode rate statistic*/
#ifdef ENCODE_RATE_STATISTIC
#define STATISTIC_PER_TIME 5  // statistic once per 5s
struct timeval nowGetTime;
static OMX_U64 lastEncodeTime = 0;
static OMX_U64 currentEncodeTime = 0;
static OMX_U32 lastEncodeFrameCount = 0;
static OMX_U32 currentEncodeFrameCount = 0;
#endif

/**
This enumeration is for levels. The value follows the level_idc in sequence
parameter set rbsp. See Annex A.
@published All
*/
typedef enum AVCLevel {
    AVC_LEVEL_AUTO = 0,
    AVC_LEVEL1_B = 9,
    AVC_LEVEL1 = 10,
    AVC_LEVEL1_1 = 11,
    AVC_LEVEL1_2 = 12,
    AVC_LEVEL1_3 = 13,
    AVC_LEVEL2 = 20,
    AVC_LEVEL2_1 = 21,
    AVC_LEVEL2_2 = 22,
    AVC_LEVEL3 = 30,
    AVC_LEVEL3_1 = 31,
    AVC_LEVEL3_2 = 32,
    AVC_LEVEL4 = 40,
    AVC_LEVEL4_1 = 41,
    AVC_LEVEL4_2 = 42,
    AVC_LEVEL5 = 50,
    AVC_LEVEL5_1 = 51
} AVCLevel;

typedef enum HEVCLevel {
    HEVC_UNSUPPORT_LEVEL = -1,
    HEVC_LEVEL4_1 = 0,
    HEVC_LEVEL_MAX = 0x7FFFFFFF,
} HEVCLevel;

typedef struct {
    OMX_RK_VIDEO_CODINGTYPE codec_id;
    OMX_VIDEO_CODINGTYPE     omx_id;
} CodeMap;


static const CodeMap kCodeMap[] = {
    { OMX_RK_VIDEO_CodingAVC,   OMX_VIDEO_CodingAVC},
    { OMX_RK_VIDEO_CodingVP8,   OMX_VIDEO_CodingVP8},
    { OMX_RK_VIDEO_CodingHEVC,  OMX_VIDEO_CodingHEVC},
};

int calc_plane(int width, int height)
{
    int mbX, mbY;

    mbX = (width + 15) / 16;
    mbY = (height + 15) / 16;

    /* Alignment for interlaced processing */
    mbY = (mbY + 1) / 2 * 2;

    return (mbX * 16) * (mbY * 16);
}

void UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    if ((fpOutputPort->portDefinition.format.video.nFrameWidth !=
         fpInputPort->portDefinition.format.video.nFrameWidth) ||
        (fpOutputPort->portDefinition.format.video.nFrameHeight !=
         fpInputPort->portDefinition.format.video.nFrameHeight)) {
        OMX_U32 width = 0, height = 0;

        fpOutputPort->portDefinition.format.video.nFrameWidth =
            fpInputPort->portDefinition.format.video.nFrameWidth;
        fpOutputPort->portDefinition.format.video.nFrameHeight =
            fpInputPort->portDefinition.format.video.nFrameHeight;
        width = fpOutputPort->portDefinition.format.video.nStride =
                    fpInputPort->portDefinition.format.video.nStride;
        height = fpOutputPort->portDefinition.format.video.nSliceHeight =
                     fpInputPort->portDefinition.format.video.nSliceHeight;

        switch (fpOutputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            if (width && height)
                fpOutputPort->portDefinition.nBufferSize = (width * height * 3) / 2;
            break;
        default:
            if (width && height)
                fpOutputPort->portDefinition.nBufferSize = width * height * 2;
            break;
        }
    }

    return;
}

OMX_BOOL FP_Check_BufferProcess_State(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nPortIndex)
{
    OMX_BOOL ret = OMX_FALSE;

    if ((pFpComponent->currentState == OMX_StateExecuting) &&
        (pFpComponent->pFoilplanetPort[nPortIndex].portState == OMX_StateIdle) &&
        (pFpComponent->transientState != FP_OMX_TransStateExecutingToIdle) &&
        (pFpComponent->transientState != FP_OMX_TransStateIdleToExecuting)) {
        ret = OMX_TRUE;
    } else {
        ret = OMX_FALSE;
    }

    return ret;
}

OMX_ERRORTYPE FP_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE              ret               = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT           *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    /* Input port */
    pFpInputPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pFpInputPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pFpInputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pFpInputPort->portDefinition.format.video.nSliceHeight = 0;
    pFpInputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pFpInputPort->portDefinition.format.video.pNativeRender = 0;
    pFpInputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFpInputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pFpInputPort->portDefinition.bEnabled = OMX_TRUE;
    pFpInputPort->bufferProcessType = BUFFER_COPY;
    pFpInputPort->portWayType = WAY2_PORT;

    /* Output port */
    pFpOutputPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pFpOutputPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pFpOutputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pFpOutputPort->portDefinition.format.video.nSliceHeight = 0;
    pFpOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pFpOutputPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    memset(pFpOutputPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    strcpy(pFpOutputPort->portDefinition.format.video.cMIMEType, "raw/video");
    pFpOutputPort->portDefinition.format.video.pNativeRender = 0;
    pFpOutputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFpOutputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pFpOutputPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pFpOutputPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pFpOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pFpOutputPort->portDefinition.bEnabled = OMX_TRUE;
    pFpOutputPort->bufferProcessType = (FP_OMX_BUFFERPROCESS_TYPE)(BUFFER_COPY | BUFFER_ANBSHARE);
    pFpOutputPort->portWayType = WAY2_PORT;

    return ret;
}


void FP_Wait_ProcessPause(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nPortIndex)
{
    FP_OMX_BASEPORT *rockchipOMXInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT *rockchipOMXOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_BASEPORT *rockchipOMXPort = NULL;

    FunctionIn();

    rockchipOMXPort = &pFpComponent->pFoilplanetPort[nPortIndex];

    if (((pFpComponent->currentState == OMX_StatePause) ||
         (pFpComponent->currentState == OMX_StateIdle) ||
         (pFpComponent->transientState == FP_OMX_TransStateLoadedToIdle) ||
         (pFpComponent->transientState == FP_OMX_TransStateExecutingToIdle)) &&
        (pFpComponent->transientState != FP_OMX_TransStateIdleToLoaded) &&
        (!CHECK_PORT_BEING_FLUSHED(rockchipOMXPort))) {
        OSAL_SignalWait(pFpComponent->pFoilplanetPort[nPortIndex].pauseEvent, DEF_MAX_WAIT_TIME);
        OSAL_SignalReset(pFpComponent->pFoilplanetPort[nPortIndex].pauseEvent);
    }

    FunctionOut();

    return;
}

static void mpeg_rgb2yuv(unsigned char *src, unsigned char *dstY, unsigned char *dstUV, int width, int height, int src_format, int need_32align)
{
#define MIN(X, Y)           ((X)<(Y)?(X):(Y))
#define MAX(X, Y)           ((X)>(Y)?(X):(Y))

    int R, G, B;
    int Y, U, V;
    int i, j;
    int stride = (width + 31) & (~31);

    width = ((width + 15) & (~15));

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            if (src_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                R = *src++;
                G = *src++;
                B = *src++;
                src++;
            } else {
                B = *src++;
                G = *src++;
                R = *src++;
                src++;
            }

            Y = (( 66 * R + 129 * G +  25 * B + 128) >> 8) +  16;

            *dstY++ = (unsigned char)(MIN(MAX(0, Y), 0xff));
            if ((i & 1) == 0 && (j & 1) == 0) {
                U = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
                V = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
                *dstUV++ = (unsigned char)(MIN(MAX(0, U), 0xff));
                *dstUV++ = (unsigned char)(MIN(MAX(0, V), 0xff));
            }
        }

        if (need_32align) {
            if (stride != width) {
                src += (stride - width) * 4;
            }
        }

    }
}

OMX_ERRORTYPE FP_Enc_ReConfig(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 new_width, OMX_U32 new_height)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc    =  (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    OMX_RK_VIDEO_CODINGTYPE codecId = OMX_RK_VIDEO_CodingUnused;
    H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
    VpuCodecContext_t *p_vpu_ctx = NULL;
    EncParameter_t *EncParam;
    EncParameter_t preEncParam;
    MUTEX_LOCK(pVideoEnc->bRecofig_Mutex);
    if (pVideoEnc->vpu_ctx) {
        memset(&preEncParam, 0, sizeof(EncParameter_t));
        pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_GETCFG, &preEncParam);
        if (pVideoEnc->rkvpu_close_cxt) {
            pVideoEnc->rkvpu_close_cxt(&pVideoEnc->vpu_ctx);
        }
    }
    if (pVideoEnc->vpu_ctx == NULL) {
        if (pVideoEnc->rkvpu_open_cxt) {
            pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
        }
    }
    p_vpu_ctx->width = new_width;
    p_vpu_ctx->height = new_height;
    p_vpu_ctx->codecType = CODEC_ENCODER;
    {
        int32_t kNumMapEntries = sizeof(kCodeMap) / sizeof(kCodeMap[0]);
        int i = 0;
        for (i = 0; i < kNumMapEntries; i++) {
            if (kCodeMap[i].omx_id == pVideoEnc->codecId) {
                codecId = kCodeMap[i].codec_id;
                break;
            }
        }
    }

    p_vpu_ctx->videoCoding = codecId;
    p_vpu_ctx->codecType = CODEC_ENCODER;
    p_vpu_ctx->private_data = mpp_malloc(EncParameter_t, 1);
    memcpy(p_vpu_ctx->private_data, &preEncParam, sizeof(EncParameter_t));
    EncParam = (EncParameter_t*)p_vpu_ctx->private_data;
    EncParam->height = new_height;
    EncParam->width = new_width;
    if (p_vpu_ctx) {
        if (p_vpu_ctx->init(p_vpu_ctx, NULL, 0)) {
            ret = OMX_ErrorInsufficientResources;
            MUTEX_UNLOCK(pVideoEnc->bRecofig_Mutex);
            goto EXIT;

        }
        memcpy(pVideoEnc->bSpsPpsbuf, p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
        pVideoEnc->bSpsPpsLen = p_vpu_ctx->extradata_size;
    }
    EncParam->rc_mode = 1;
    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, EncParam);
    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
    pVideoEnc->vpu_ctx = p_vpu_ctx;
    pVideoEnc->bPrependSpsPpsToIdr = OMX_TRUE;
    MUTEX_UNLOCK(pVideoEnc->bRecofig_Mutex);
EXIT:
    FunctionOut();
    return ret;
}

OMX_U32 FP_N12_Process(OMX_COMPONENTTYPE *pOMXComponent, FoilplanetVideoPlane *vplanes, OMX_U32 *aPhy_address)
{

    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT *pInPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT *pOutPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    RK_U32 new_width = 0, new_height = 0, len = 0;
    OMX_U32 Width_in = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height_in =  pOutPort->portDefinition.format.video.nFrameHeight;
    OMX_U32 Width = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height =  pOutPort->portDefinition.format.video.nFrameHeight;
    if (pVideoEnc->params_extend.bEnableScaling || pVideoEnc->params_extend.bEnableCropping) {
        MUTEX_LOCK(pVideoEnc->bScale_Mutex);
        if (pVideoEnc->params_extend.bEnableScaling) {
            new_width = pVideoEnc->params_extend.ui16ScaledWidth;
            new_height = pVideoEnc->params_extend.ui16ScaledHeight;
        } else if (pVideoEnc->params_extend.bEnableCropping) {
            new_width = Width_in - pVideoEnc->params_extend.ui16CropLeft - pVideoEnc->params_extend.ui16CropRight;
            new_height = Height_in - pVideoEnc->params_extend.ui16CropTop - pVideoEnc->params_extend.ui16CropBottom;
            mpp_trace("CropLeft = %d CropRight = %d CropTop %d CropBottom %d",
                      pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                      pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom);
        }
        mpp_trace("new_width = %d new_height = %d orign width %d orign height %d",
                  new_width, new_height, Width_in, Height_in);
        if (new_width != pVideoEnc->bCurrent_width ||
            new_height != pVideoEnc->bCurrent_height) {
            pVideoEnc->bCurrent_width  =  new_width;
            pVideoEnc->bCurrent_height =  new_height;
            FP_Enc_ReConfig(pOMXComponent, new_width, new_height);
        }
      #ifdef USE_RKVPU
        rga_nv12_crop_scale(vplanes, pVideoEnc->enc_vpumem, &pVideoEnc->params_extend, Width, Height, pVideoEnc->rga_ctx);
        *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
        len = new_width * new_height * 3 / 2;
      #else
        (void)vplanes;
        mpp_assert(0);
      #endif
        MUTEX_UNLOCK(pVideoEnc->bScale_Mutex);
    } else {
      #ifdef USE_ION
        OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, vplanes->fd, aPhy_address);
        len = Width * Height * 3 / 2;
      #else
        (void)aPhy_address;
        mpp_assert(0);
      #endif
    }
    return len;
}

#ifdef USE_STOREMETADATA
OMX_ERRORTYPE FP_ProcessStoreMetaData(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_U32 *aPhy_address, OMX_U32 *len)
{

    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT *pInPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT *pOutPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    OMX_U32 Width = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height =  pOutPort->portDefinition.format.video.nFrameHeight;
    OMX_PTR pGrallocHandle;
    *len = 0;
    *aPhy_address = 0;
    if (!OSAL_GetInfoRkWfdMetaData(pVideoEnc->bRkWFD, pInputBuffer->pBuffer, &pGrallocHandle)) {
        if (!((FP_OMX_COLOR_FORMATTYPE)pInPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque)) {
            mpp_log("Error colorformat != OMX_COLOR_FormatAndroidOpaque");
        }
        gralloc_private_handle_t priv_hnd_wfd;
        memset(&priv_hnd_wfd, 0, sizeof(priv_hnd_wfd));
        Foilplanet_get_gralloc_private(pGrallocHandle, &priv_hnd_wfd);
        if (VPUMemJudgeIommu() == 0) {
            OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, priv_hnd_wfd.share_fd, aPhy_address);
        } else {
            *aPhy_address = priv_hnd_wfd.share_fd;
        }
        *len = Width * Height * 4;
        if (pVideoEnc->bPixel_format < 0) {
            pVideoEnc->bPixel_format = priv_hnd_wfd.format;
            if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                H264EncPictureType encType = VPU_H264ENC_BGR888;    // add by lance 2014.01.20
                pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            } else {
                H264EncPictureType encType = VPU_H264ENC_RGB888;    // add by lance 2014.01.20
                pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }
        }
    } else {
        FoilplanetVideoPlane vplanes;
        OMX_COLOR_FORMATTYPE omx_format = OMX_COLOR_FormatUnused;
        OMX_U32 res;

#ifdef AVS80
        if (pInputBuffer->nFilledLen != OSAL_GetVideoNativeMetaSize() && pInputBuffer->nFilledLen != OSAL_GetVideoGrallocMetaSize()) {
            mpp_log("MetaData buffer is wrong size! "
                     "(got %lu bytes, expected 8 or 12)", pInputBuffer->nFilledLen);
            return OMX_ErrorBadParameter;
        }
#else
        if (pInputBuffer->nFilledLen != 8) {
            mpp_log("MetaData buffer is wrong size! "
                     "(got %lu bytes, expected 8)", pInputBuffer->nFilledLen);
            return OMX_ErrorBadParameter;
        }
#endif

        if (OSAL_GetInfoFromMetaData(pInputBuffer->pBuffer, &pGrallocHandle)) {
            return OMX_ErrorBadParameter;
        }

        if (pVideoEnc->bPixel_format < 0) {
            int gpu_fd = -1;
            omx_format = OSAL_GetANBColorFormat(pGrallocHandle);
            pVideoEnc->bPixel_format = OSAL_OMX2HalPixelFormat(omx_format);//mali_gpu
            gpu_fd = open("/dev/pvrsrvkm", O_RDWR, 0);
            if (gpu_fd > 0) {
                pVideoEnc->bRgb2yuvFlag = OMX_TRUE;
                close(gpu_fd);
            } else {
                if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                    pVideoEnc->bRgb2yuvFlag = OMX_TRUE;
                }
            }
        }
        res = OSAL_getANBHandle(pGrallocHandle, &vplanes);
        if (res != 0) {
            mpp_err("Unable to lock image buffer %p for access", pGrallocHandle);
            pGrallocHandle = NULL;
            return OMX_ErrorBadParameter;
        }

        if (pVideoEnc->bRgb2yuvFlag == OMX_TRUE) {
            VPUMemLinear_t tmp_vpumem;
            int new_width = 0;
            int new_height = 0;
            if (pVideoEnc->params_extend.bEnableScaling) {
                new_width = pVideoEnc->params_extend.ui16ScaledWidth;
                new_height = pVideoEnc->params_extend.ui16ScaledHeight;
                if (new_width != pVideoEnc->bCurrent_width ||
                    new_height != pVideoEnc->bCurrent_height) {
                    pVideoEnc->bCurrent_width  =  new_width;
                    pVideoEnc->bCurrent_height =  new_height;
                    FP_Enc_ReConfig(pOMXComponent, new_width, new_height);
                }
            } else {
                new_width = Width;
                new_height = Height;
            }
            uint8_t *Y = (uint8_t*)pVideoEnc->enc_vpumem->vir_addr;
            uint8_t *UV = Y + ((Width + 15) & (~15)) * Height;
            memset(&tmp_vpumem, 0, sizeof(VPUMemLinear_t));
            rga_rgb2nv12(&vplanes, pVideoEnc->enc_vpumem, Width, Height, new_width, new_height, pVideoEnc->rga_ctx);
            VPUMemClean(pVideoEnc->enc_vpumem);
            *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
            *len = new_width * new_width * 3 / 2;
            if (pVideoEnc->fp_enc_in) {
                VPUMemInvalidate(pVideoEnc->enc_vpumem);
                fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, new_width * new_width * 3 / 2, pVideoEnc->fp_enc_in);
                fflush(pVideoEnc->fp_enc_in);
            }
        } else if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCrCb_NV12) {
            *len = FP_N12_Process(pOMXComponent, &vplanes, aPhy_address);
        } else if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
            EncParameter_t EncParam;
            H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
            pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_GETCFG, (void*)&EncParam);
            EncParam.rc_mode = 1 << 16; //set intraDeltaqp as 4 to fix encoder cts issue
            pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETCFG, (void*)&EncParam);
            if (Width != vplanes.stride || (Height & 0xf)) {
                rga_nv12_copy(&vplanes, pVideoEnc->enc_vpumem, Width, Height, pVideoEnc->rga_ctx);
                *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
                if (pVideoEnc->fp_enc_in) {
                    fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, Width * Height * 3 / 2, pVideoEnc->fp_enc_in);
                    fflush(pVideoEnc->fp_enc_in);
                }
            } else {
                OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, vplanes.fd, aPhy_address);
            }

            mpp_err("aPhy_address = 0x%08x", *aPhy_address);
            *len = Width * Height * 3 / 2;
        } else {
            rga_rgb_copy(&vplanes, pVideoEnc->enc_vpumem, Width, Height, pVideoEnc->rga_ctx);
            *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
            *len = Width * Height * 4;
        }

#if 0 // def WRITE_FILE
        VPUMemInvalidate(pVideoEnc->enc_vpumem);
        fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, Width_in * Height_in * 4, pVideoEnc->fp_h264);
        fflush(pVideoEnc->fp_h264);
#endif
    }
    return OMX_ErrorNone;
}
#endif
OMX_BOOL FP_SendInputData(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_BOOL ret = OMX_FALSE;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *inputUseBuffer = &fpInputPort->way.port2WayDataBuffer.inputDataBuffer;
    VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;
    FunctionIn();
    OMX_PTR pGrallocHandle;
    OMX_COLOR_FORMATTYPE omx_format = OMX_COLOR_FormatUnused;
    if (inputUseBuffer->dataValid == OMX_TRUE) {
        EncInputStream_t aInput;

        if (pVideoEnc->bFirstFrame) {
            EncParameter_t vpug;
            if ((FP_OMX_COLOR_FORMATTYPE)fpInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
                OSAL_GetInfoFromMetaData(inputUseBuffer->bufferHeader->pBuffer, &pGrallocHandle);
                if (pGrallocHandle == NULL) {
                    mpp_err("pGrallocHandle is NULL set omx_format default");
                    omx_format = OMX_COLOR_FormatUnused;
                } else {
                    omx_format = OSAL_GetANBColorFormat(pGrallocHandle);
                }
                if (OSAL_OMX2HalPixelFormat(omx_format)  == HAL_PIXEL_FORMAT_YCbCr_420_888) {
                    H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
                } else {
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                    vpug.rc_mode = 1;

                    mpp_trace("set vpu_enc %d", vpug.rc_mode);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
                    mpp_trace("VPU_API_ENC_SETFORMAT in");
                    H264EncPictureType encType = VPU_H264ENC_RGB888;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
                }
            } else if (fpInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar) {
                H264EncPictureType encType = VPU_H264ENC_YUV420_PLANAR;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }
            pVideoEnc->bFirstFrame = OMX_FALSE;
        }

        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_trace("bSaveFlagEOS : OMX_TRUE");
            pFpComponent->bSaveFlagEOS = OMX_TRUE;
        }
        memset(&aInput, 0, sizeof(EncInputStream_t));

#ifdef USE_STOREMETADATA
        if (pVideoEnc->bStoreMetaData && !pFpComponent->bSaveFlagEOS) {
            OMX_U32 aPhy_address = 0, len = 0;

            ret = FP_ProcessStoreMetaData(pOMXComponent, inputUseBuffer->bufferHeader, &aPhy_address, &len);
            p_vpu_ctx = pVideoEnc->vpu_ctx; // may be reconfig in preprocess

            if (ret != OMX_ErrorNone) {
                mpp_err("FP_ProcessStoreMetaData return %d ", ret);
                FP_InputBufferReturn(pOMXComponent, inputUseBuffer);
                pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pFpComponent->callbackData, OMX_EventError,
                                                             OUTPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
                goto EXIT;
            }

            aInput.buf =  NULL;
            aInput.bufPhyAddr = aPhy_address;
            aInput.size = len;
            aInput.timeUs = inputUseBuffer->timeStamp;
        } else {
            OMX_BUFFERHEADERTYPE* pInputBuffer = inputUseBuffer->bufferHeader;
            if (pInputBuffer->nFilledLen == 4) {
                aInput.bufPhyAddr = *(int32_t*)((uint8_t*)pInputBuffer->pBuffer + pInputBuffer->nOffset);
                mpp_trace("rk camera metadata 0x%x", aInput.bufPhyAddr);
                aInput.buf = NULL;
            } else {
                aInput.buf =  inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
                aInput.bufPhyAddr = 0x80000000;
                if (pVideoEnc->fp_enc_in) {
                    fwrite(aInput.buf, 1, inputUseBuffer->dataLen, pVideoEnc->fp_enc_in);
                    fflush(pVideoEnc->fp_enc_in);
                }
                // while bufPhyAddr < 0 && buf != NULL
                // assign bufPhyAddr 8000000 to match rk_vpuapi to copy data from aInput.buf
            }
            aInput.size = inputUseBuffer->dataLen;
            aInput.timeUs = inputUseBuffer->timeStamp;
        }
#else
        {
            OMX_BUFFERHEADERTYPE* pInputBuffer = inputUseBuffer->bufferHeader;
            if (pInputBuffer->nFilledLen == 4) {
                aInput.bufPhyAddr = *(int32_t*)((uint8_t*)pInputBuffer->pBuffer + pInputBuffer->nOffset);
                mpp_trace("rk camera metadata 0x%x", aInput.bufPhyAddr);
                aInput.buf = NULL;
            } else {
                aInput.buf =  inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
                aInput.bufPhyAddr = 0x80000000;
                // while bufPhyAddr < 0 && buf != NULL
                // assign bufPhyAddr 8000000 to match rk_vpuapi to copy data from aInput.buf
            }
            aInput.size = inputUseBuffer->dataLen;
            aInput.timeUs = inputUseBuffer->timeStamp;
        }
#endif

        if ((FP_OMX_COLOR_FORMATTYPE)fpInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
            if ((pVideoEnc->bRgb2yuvFlag == OMX_TRUE) || (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCrCb_NV12)) {
                mpp_trace("set as nv12 format");
                H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }
        }

        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
            if ((FP_OMX_COLOR_FORMATTYPE)fpInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
                if (pVideoEnc->bFrame_num < 60 && (pVideoEnc->bFrame_num % 5 == 0)) {
                    EncParameter_t vpug;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETIDRFRAME, NULL);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, &vpug);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, &vpug);
                }
                if (pVideoEnc->bFrame_num - pVideoEnc->bLast_config_frame == 60) {
                    EncParameter_t vpug;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, &vpug);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, &vpug);
                    mpp_trace("pVideoEnc->bFrame_num %d pVideoEnc->mLast_config_frame %d",
                              pVideoEnc->bFrame_num, pVideoEnc->bLast_config_frame);
                    pVideoEnc->bLast_config_frame = pVideoEnc->bFrame_num;

                }
            }
        }

        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_log("send eos");
            aInput.nFlags |= OMX_BUFFERFLAG_EOS;
        }

        p_vpu_ctx->encoder_sendframe(p_vpu_ctx, &aInput);

        pVideoEnc->bFrame_num++;
        FP_InputBufferReturn(pOMXComponent, inputUseBuffer);

        if (pFpComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pFpComponent->checkTimeStamp.startTimeStamp = inputUseBuffer->timeStamp;
            pFpComponent->checkTimeStamp.nStartFlags = inputUseBuffer->nFlags;
            pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
            mpp_trace("first frame timestamp after seeking %lld us (%.2f secs)",
                      inputUseBuffer->timeStamp, inputUseBuffer->timeStamp / 1E6);
        }
        ret = OMX_TRUE;
    }

EXIT:
    FunctionOut();
    return ret;
}

OMX_BOOL FP_Post_OutputStream(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_BOOL               ret = OMX_FALSE;
    FP_OMX_BASECOMPONENT  *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *pOutputPort      = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *outputUseBuffer  = &pOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    VpuCodecContext_t           *p_vpu_ctx = pVideoEnc->vpu_ctx;

    FunctionIn();


    if ((p_vpu_ctx == NULL) || (pVideoEnc->bEncSendEos == OMX_TRUE)) {
        goto EXIT;
    }
    if (outputUseBuffer->dataValid == OMX_TRUE) {
        OMX_U32 width = 0, height = 0;
        int imageSize = 0;
        EncoderOut_t pOutput;
        OMX_U8 *aOut_buf = outputUseBuffer->bufferHeader->pBuffer;
        memset(&pOutput, 0, sizeof(EncoderOut_t));
        if ((OMX_FALSE == pVideoEnc->bSpsPpsHeaderFlag) && (pVideoEnc->codecId == OMX_VIDEO_CodingAVC)) {
            if (pVideoEnc->bSpsPpsLen > 0) {
                memcpy(aOut_buf, pVideoEnc->bSpsPpsbuf, pVideoEnc->bSpsPpsLen);
                outputUseBuffer->remainDataLen = pVideoEnc->bSpsPpsLen;
                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                mpp_log("set bSpsPpsLen %d", pVideoEnc->bSpsPpsLen);
                pVideoEnc->bSpsPpsHeaderFlag = OMX_TRUE;
                ret = OMX_TRUE;
                if (pVideoEnc->fp_enc_out != NULL) {
                    fwrite(aOut_buf, 1, pVideoEnc->bSpsPpsLen, pVideoEnc->fp_enc_out);
                    fflush(pVideoEnc->fp_enc_out);
                }

                FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                goto EXIT;
            }
        }

        mpp_trace("encoder_getstream in ");
        if (p_vpu_ctx->encoder_getstream(p_vpu_ctx, &pOutput) < 0) {
            outputUseBuffer->dataLen = 0;
            outputUseBuffer->remainDataLen = 0;
            outputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
            outputUseBuffer->timeStamp = 0;
            ret = OMX_TRUE;
            mpp_log("OMX_BUFFERFLAG_EOS");
            FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            pVideoEnc->bEncSendEos = OMX_TRUE;
            goto EXIT;
        }
        if ((pOutput.size > 0) && (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
#ifdef ENCODE_RATE_STATISTIC
            gettimeofday(&nowGetTime, NULL);
            currentEncodeTime = nowGetTime.tv_sec * 1000000 + nowGetTime.tv_usec;
            if (lastEncodeTime != 0) {
                ++currentEncodeFrameCount;
                if (currentEncodeTime - lastEncodeTime >= (STATISTIC_PER_TIME * 1000000)) {
                    mpp_err("Statistic Encode Rate %d", ((currentEncodeFrameCount - lastEncodeFrameCount) / STATISTIC_PER_TIME));
                    lastEncodeTime = currentEncodeTime;
                    lastEncodeFrameCount = currentEncodeFrameCount;
                }
            } else
                lastEncodeTime = currentEncodeTime;
#endif
            if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
                if (pVideoEnc->bPrependSpsPpsToIdr && pOutput.keyFrame) {
                    mpp_log("IDR outputUseBuffer->remainDataLen  %d spslen %d size %d", outputUseBuffer->remainDataLen
                             , pVideoEnc->bSpsPpsLen, outputUseBuffer->allocSize);
                    memcpy(aOut_buf, pVideoEnc->bSpsPpsbuf, pVideoEnc->bSpsPpsLen);
                    memcpy(aOut_buf + pVideoEnc->bSpsPpsLen, "\x00\x00\x00\x01", 4);
                    memcpy(aOut_buf + pVideoEnc->bSpsPpsLen + 4, pOutput.data, pOutput.size);
                    outputUseBuffer->remainDataLen = pVideoEnc->bSpsPpsLen + pOutput.size + 4;
                    outputUseBuffer->usedDataLen += pVideoEnc->bSpsPpsLen;
                    outputUseBuffer->usedDataLen += 4;
                    outputUseBuffer->usedDataLen += pOutput.size;
                    mpp_log("IDR outputUseBuffer->remainDataLen 1 %d spslen %d size %d", outputUseBuffer->remainDataLen
                             , pVideoEnc->bSpsPpsLen, outputUseBuffer->allocSize);
                } else {
                    memcpy(aOut_buf, "\x00\x00\x00\x01", 4);
                    memcpy(aOut_buf + 4, pOutput.data, pOutput.size);
                    outputUseBuffer->remainDataLen = pOutput.size + 4;
                    outputUseBuffer->usedDataLen += 4;
                    outputUseBuffer->usedDataLen += pOutput.size;
                }
            } else {
                memcpy(aOut_buf, pOutput.data, pOutput.size);
                outputUseBuffer->remainDataLen = pOutput.size;
                outputUseBuffer->usedDataLen = pOutput.size;
            }
            if (pVideoEnc->fp_enc_out != NULL) {
                fwrite(aOut_buf, 1, outputUseBuffer->remainDataLen , pVideoEnc->fp_enc_out);
                fflush(pVideoEnc->fp_enc_out);
            }

            outputUseBuffer->timeStamp = pOutput.timeUs;
            if (pOutput.keyFrame) {
                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
            }
            if (pOutput.data) {
                free(pOutput.data);
                pOutput.data = NULL;
            }
            if ((outputUseBuffer->remainDataLen > 0) ||
                ((outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
                mpp_trace("FP_OutputBufferReturn");
                FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            }
            ret = OMX_TRUE;
        } else if (CHECK_PORT_BEING_FLUSHED(pOutputPort)) {
            if (pOutput.data) {
                free(pOutput.data);
                pOutput.data = NULL;
            }
            outputUseBuffer->dataLen = 0;
            outputUseBuffer->remainDataLen = 0;
            outputUseBuffer->nFlags = 0;
            outputUseBuffer->timeStamp = 0;
            ret = OMX_TRUE;
            FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
        } else {
            //mpp_err("output buffer is smaller than decoded data size Out Length");
            // pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
            //                                        pFpComponent->callbackData,
            //                                         OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            ret = OMX_FALSE;
        }
    } else {
        ret = OMX_FALSE;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_InputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *srcInputUseBuffer = &fpInputPort->way.port2WayDataBuffer.inputDataBuffer;
    OMX_BOOL               bCheckInputData = OMX_FALSE;
    OMX_BOOL               bValidCodecData = OMX_FALSE;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        usleep(0);
        FP_Wait_ProcessPause(pFpComponent, INPUT_PORT_INDEX);
        mpp_trace("FP_Check_BufferProcess_State in");
        while ((FP_Check_BufferProcess_State(pFpComponent, INPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {


            if ((CHECK_PORT_BEING_FLUSHED(fpInputPort)) ||
                (((FP_OMX_EXCEPTION_STATE)fpOutputPort->exceptionFlag != GENERAL_STATE) && ((FP_OMX_ERRORTYPE)ret == OMX_ErrorInputDataDecodeYet)))
                break;

            if (fpInputPort->portState != OMX_StateIdle)
                break;

            MUTEX_LOCK(srcInputUseBuffer->bufferMutex);
            if ((FP_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) {
                if ((srcInputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(fpInputPort))) {

                    ret = FP_InputBufferGetQueue(pFpComponent);
                    if (ret != OMX_ErrorNone) {
                        MUTEX_UNLOCK(srcInputUseBuffer->bufferMutex);
                        break;
                    }
                }

                if (srcInputUseBuffer->dataValid == OMX_TRUE) {
                    if (FP_SendInputData((OMX_COMPONENTTYPE *)hComponent) != OMX_TRUE) {
                        usleep(5 * 1000);
                    }
                }
                if (CHECK_PORT_BEING_FLUSHED(fpInputPort)) {
                    MUTEX_UNLOCK(srcInputUseBuffer->bufferMutex);
                    break;
                }
            }
            MUTEX_UNLOCK(srcInputUseBuffer->bufferMutex);
            if ((FP_OMX_ERRORTYPE)ret == OMX_ErrorCodecInit)
                pVideoEnc->bExitBufferProcessThread = OMX_TRUE;
        }
    }

EXIT:

    FunctionOut();

    return ret;
}


OMX_ERRORTYPE FP_OMX_OutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *dstOutputUseBuffer = &fpOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    VpuCodecContext_t    *p_vpu_ctx = pVideoEnc->vpu_ctx;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        usleep(0);
        FP_Wait_ProcessPause(pFpComponent, OUTPUT_PORT_INDEX);

        while ((FP_Check_BufferProcess_State(pFpComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {

            if (CHECK_PORT_BEING_FLUSHED(fpOutputPort))
                break;

            MUTEX_LOCK(dstOutputUseBuffer->bufferMutex);
            if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                (!CHECK_PORT_BEING_FLUSHED(fpOutputPort))) {

                mpp_trace("FP_OutputBufferGetQueue in");
                ret = FP_OutputBufferGetQueue(pFpComponent);
                mpp_trace("FP_OutputBufferGetQueue out");
                if (ret != OMX_ErrorNone) {
                    MUTEX_UNLOCK(dstOutputUseBuffer->bufferMutex);
                    break;
                }
            }

            if (dstOutputUseBuffer->dataValid == OMX_TRUE) {
                MUTEX_LOCK(pVideoEnc->bRecofig_Mutex);
                ret = (OMX_ERRORTYPE)FP_Post_OutputStream(pOMXComponent);
                MUTEX_UNLOCK(pVideoEnc->bRecofig_Mutex);
                if (ret != (OMX_ERRORTYPE)OMX_TRUE) {
                    usleep(5 * 1000);
                }
            }
            MUTEX_UNLOCK(dstOutputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE FP_OMX_InputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_MESSAGE       *message = NULL;

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
    FP_OMX_InputBufferProcess(pOMXComponent);

    pthread_exit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE FP_OMX_OutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_MESSAGE       *message = NULL;

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
    FP_OMX_OutputBufferProcess(pOMXComponent);

    pthread_exit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferProcess_Create( OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_FALSE;

    pVideoEnc->hOutputThread = new MppThread((MppThreadFunc)FP_OMX_OutputProcessThread, pOMXComponent);

    pVideoEnc->hInputThread = new MppThread((MppThreadFunc)FP_OMX_InputProcessThread, pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferProcess_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    OMX_S32                countValue = 0;
    unsigned int           i = 0;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_TRUE;

    OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].bufferSemID);

    OSAL_SignalSet(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].pauseEvent);

    delete (MppThread *)pVideoEnc->hInputThread;
    pVideoEnc->hInputThread = NULL;

    OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].bufferSemID);


    OSAL_SignalSet(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].pauseEvent);

    OSAL_SignalSet(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].pauseEvent);

    delete (MppThread *)pVideoEnc->hOutputThread;
    pVideoEnc->hOutputThread = NULL;

    pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE ConvertOmxAvcLevelToAvcSpecLevel(int32_t omxLevel, AVCLevel *pvLevel)
{
    mpp_log("ConvertOmxAvcLevelToAvcSpecLevel: %d", omxLevel);
    AVCLevel level = AVC_LEVEL5_1;
    switch (omxLevel) {
    case OMX_VIDEO_AVCLevel1:
    case OMX_VIDEO_AVCLevel1b:
        level = AVC_LEVEL1;
        break;
    case OMX_VIDEO_AVCLevel11:
        level = AVC_LEVEL1_1;
        break;
    case OMX_VIDEO_AVCLevel12:
        level = AVC_LEVEL1_2;
        break;
    case OMX_VIDEO_AVCLevel13:
        level = AVC_LEVEL1_3;
        break;
    case OMX_VIDEO_AVCLevel2:
        level = AVC_LEVEL2;
        break;
    case OMX_VIDEO_AVCLevel21:
        level = AVC_LEVEL2_1;
        break;
    case OMX_VIDEO_AVCLevel22:
        level = AVC_LEVEL2_2;
        break;
    case OMX_VIDEO_AVCLevel3:
        level = AVC_LEVEL3;
        break;
    case OMX_VIDEO_AVCLevel31:
        level = AVC_LEVEL3_1;
        break;
    case OMX_VIDEO_AVCLevel32:
        level = AVC_LEVEL3_2;
        break;
    case OMX_VIDEO_AVCLevel4:
        level = AVC_LEVEL4;
        break;
    case OMX_VIDEO_AVCLevel41:
        level = AVC_LEVEL4_1;
        break;
    case OMX_VIDEO_AVCLevel42:
        level = AVC_LEVEL4_2;
        break;
    case OMX_VIDEO_AVCLevel5:
        level = AVC_LEVEL5;
        break;
    case OMX_VIDEO_AVCLevel51:
        level = AVC_LEVEL5_1;
        break;
    default:
        mpp_err("Unknown omx level: %d", omxLevel);
        return OMX_ErrorMax;
    }
    *pvLevel = level;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE ConvertOmxHevcProfile2HalHevcProfile(
    OMX_VIDEO_HEVCPROFILETYPE omxHevcProfile, HEVCEncProfile *halHevcProfile)
{
    HEVCEncProfile hevcProfile = HEVC_MAIN_PROFILE;
    switch (omxHevcProfile) {
    case OMX_VIDEO_HEVCProfileMain:
        hevcProfile = HEVC_MAIN_PROFILE;
        break;
    case OMX_VIDEO_HEVCProfileMain10:
        hevcProfile = HEVC_MAIN10_PROFILE;
        break;
    case OMX_VIDEO_HEVCProfileMain10HDR10:
        hevcProfile = HEVC_MAIN10HDR10_PROFILE;
        break;
    default:
        mpp_err("Unknown omx profile: %d, forced to convert HEVC_MAIN_PROFILE",
                omxHevcProfile);
        break;
    }
    *halHevcProfile = hevcProfile;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE ConvertOmxHevcLevel2HalHevcLevel(
    OMX_VIDEO_HEVCLEVELTYPE omxHevcLevel, HEVCLevel *halHevcLevel)
{
    HEVCLevel hevcLevel = HEVC_LEVEL4_1;
    switch (omxHevcLevel) {
    case OMX_VIDEO_HEVCMainTierLevel41:
        hevcLevel = HEVC_LEVEL4_1;
        break;
    default:
        mpp_err("Unknown omx level: %d, forced to convert HEVC_LEVEL4_1",
                omxHevcLevel);
        break;
    }
    *halHevcLevel = hevcLevel;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_open_vpuenc_context(FP_OMX_VIDEOENC_COMPONENT *pVideoEnc)
{
    pVideoEnc->rkapi_hdl = dlopen("libvpu.so", RTLD_LAZY | RTLD_GLOBAL);
    if (pVideoEnc->rkapi_hdl == NULL) {
        return OMX_ErrorHardware;
    }
    pVideoEnc->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_open_context");
    if (pVideoEnc->rkvpu_open_cxt == NULL) {
        dlclose(pVideoEnc->rkapi_hdl);
        pVideoEnc->rkapi_hdl = NULL;
        mpp_trace("used old version lib");
        pVideoEnc->rkapi_hdl = dlopen("librk_vpuapi.so", RTLD_LAZY | RTLD_GLOBAL);
        if (pVideoEnc->rkapi_hdl == NULL) {
            mpp_err("dll open fail librk_vpuapi.so");
            return OMX_ErrorHardware;
        }
        pVideoEnc->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_open_context");
        if (pVideoEnc->rkvpu_open_cxt == NULL) {
            mpp_err("dlsym vpu_open_context fail");
            dlclose( pVideoEnc->rkapi_hdl);
            return OMX_ErrorHardware;
        }
        pVideoEnc->bIsNewVpu = OMX_FALSE;
    } else {
        pVideoEnc->bIsNewVpu = OMX_TRUE;
    }
    pVideoEnc->rkvpu_close_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_close_context");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FP_Enc_DebugSwitchfromPropget(
    FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE               ret       = OMX_ErrorNone;
    FP_OMX_VIDEOENC_COMPONENT  *pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    char                        value[PROPERTY_VALUE_MAX];
    memset(value, 0, sizeof(value));
    if (property_get("record_omx_enc_out", value, "0") && (atoi(value) > 0)) {
        mpp_log("Start recording stream to /data/video/enc_out.bin");
        if (pVideoEnc->fp_enc_out != NULL) {
            fclose(pVideoEnc->fp_enc_out);
        }
        pVideoEnc->fp_enc_out = fopen("data/video/enc_out.bin", "wb");
    }

    memset(value, 0, sizeof(value));
    if (property_get("record_omx_enc_in", value, "0") && (atoi(value) > 0)) {
        mpp_log("Start recording stream to /data/video/enc_in.bin");
        if (pVideoEnc->fp_enc_in != NULL) {
            fclose(pVideoEnc->fp_enc_in);
        }
        pVideoEnc->fp_enc_in = fopen("data/video/enc_in.bin", "wb");
    }

    return ret;
}


OMX_ERRORTYPE FP_Enc_ComponentInit(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE              ret          = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc    =  (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    OMX_RK_VIDEO_CODINGTYPE   codecId = OMX_RK_VIDEO_CodingUnused;
    FP_OMX_BASEPORT           *pFpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pRockchipOutPort  = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    VpuCodecContext_t         *p_vpu_ctx    = NULL;
    EncParameter_t *EncParam = NULL;
    RK_U32 new_width = 0, new_height = 0;
    int32_t kNumMapEntries, i;

    if (omx_open_vpuenc_context(pVideoEnc) != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    if (pFpComponent->rkversion != NULL) {
        mpp_err("omx decoder info : %s", pFpComponent->rkversion);
    }
    if (pVideoEnc->bIsNewVpu == OMX_TRUE) {
        p_vpu_ctx = mpp_malloc(VpuCodecContext_t, 1);
    }

    if (pVideoEnc->rkvpu_open_cxt && pVideoEnc->bIsNewVpu == OMX_FALSE) {
        mpp_err("open vpu context FALSE");
        pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
    }

    kNumMapEntries = sizeof(kCodeMap) / sizeof(kCodeMap[0]);
    i = 0;
    for (i = 0; i < kNumMapEntries; i++) {
        if (kCodeMap[i].omx_id == pVideoEnc->codecId) {
            codecId = kCodeMap[i].codec_id;
            break;
        }
    }
    if (pVideoEnc->bIsNewVpu == OMX_TRUE) {
        memset(p_vpu_ctx, 0, sizeof(VpuCodecContext_t));
    }
    pVideoEnc->bCurrent_height = pFpInputPort->portDefinition.format.video.nFrameHeight;
    pVideoEnc->bCurrent_width = pFpInputPort->portDefinition.format.video.nFrameWidth;
    if (pVideoEnc->params_extend.bEnableScaling || pVideoEnc->params_extend.bEnableCropping) {
        if (pVideoEnc->params_extend.bEnableScaling) {
            new_width = pVideoEnc->params_extend.ui16ScaledWidth;
            new_height = pVideoEnc->params_extend.ui16ScaledHeight;
        } else if (pVideoEnc->params_extend.bEnableCropping) {
            new_width =  p_vpu_ctx->width - pVideoEnc->params_extend.ui16CropLeft - pVideoEnc->params_extend.ui16CropRight;
            new_height = p_vpu_ctx->height - pVideoEnc->params_extend.ui16CropTop - pVideoEnc->params_extend.ui16CropBottom;
            mpp_trace("CropLeft = %d CropRight = %d CropTop %d CropBottom %d",
                      pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                      pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom);
        }
        if (new_width != pVideoEnc->bCurrent_width ||
            new_height != pVideoEnc->bCurrent_height) {
            pVideoEnc->bCurrent_width  =  new_width;
            pVideoEnc->bCurrent_height =  new_height;
        }
    }

    p_vpu_ctx->codecType = CODEC_ENCODER;
    p_vpu_ctx->videoCoding = codecId;
    p_vpu_ctx->width =  pVideoEnc->bCurrent_width;
    p_vpu_ctx->height = pVideoEnc->bCurrent_height;
    if (pVideoEnc->rkvpu_open_cxt && pVideoEnc->bIsNewVpu == OMX_TRUE) {
        mpp_err("open vpu context new");
        pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
    }
    if (p_vpu_ctx == NULL) {
        mpp_err("open vpu context fail!");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (p_vpu_ctx->extra_cfg.reserved[0] == 1) {
        mpp_log("use vpuapi.");
        pVideoEnc->bIsUseMpp = OMX_FALSE;
    } else {
        mpp_log("use mpp.");
        pVideoEnc->bIsUseMpp = OMX_TRUE;
    }
    p_vpu_ctx->private_data = mpp_malloc(EncParameter_t, 1);
    memset(p_vpu_ctx->private_data, 0, sizeof(EncParameter_t));
    EncParam = (EncParameter_t*)p_vpu_ctx->private_data;
    FP_Enc_GetEncParams(pOMXComponent, &EncParam);

#ifdef ENCODE_RATE_STATISTIC
    lastEncodeTime = 0;
    currentEncodeTime = 0;
    lastEncodeFrameCount = 0;
    currentEncodeFrameCount = 0;
#endif

    if (p_vpu_ctx) {
        if (p_vpu_ctx->init(p_vpu_ctx, NULL, 0)) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;

        }
        mpp_trace("eControlRate %d ", pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
        if (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] == OMX_Video_ControlRateConstant) {
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)EncParam);
            EncParam->rc_mode = 1;
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)EncParam);
        }
        pVideoEnc->bFrame_num = 0;
        pVideoEnc->bLast_config_frame = 0;
        pVideoEnc->bSpsPpsHeaderFlag = OMX_FALSE;
        pVideoEnc->bSpsPpsbuf = NULL;
        if (pVideoEnc->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC) {
            pVideoEnc->bSpsPpsbuf = NULL;
            pVideoEnc->bSpsPpsLen = 0;
        } else {
            if (p_vpu_ctx->extradata == NULL) {
                mpp_err("init get extradata fail!");
                pVideoEnc->bSpsPpsbuf = NULL;
                pVideoEnc->bSpsPpsLen = 0;
                goto EXIT;
            } else {
                if ((p_vpu_ctx->extradata != NULL) && p_vpu_ctx->extradata_size > 0 && p_vpu_ctx->extradata_size <= 2048) {
                    pVideoEnc->bSpsPpsbuf = mpp_malloc(OMX_U8, 2048);
                    memcpy(pVideoEnc->bSpsPpsbuf, p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
                    pVideoEnc->bSpsPpsLen = p_vpu_ctx->extradata_size;
                } else {
                    mpp_err("p_vpu_ctx->extradata = %p,p_vpu_ctx->extradata_size = %d", p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
                }
            }
        }
    }
    pVideoEnc->bEncSendEos = OMX_FALSE;
    pVideoEnc->enc_vpumem = NULL;
    pVideoEnc->enc_vpumem = mpp_malloc(VPUMemLinear_t, 1);
    ret = (OMX_ERRORTYPE)VPUMallocLinear(pVideoEnc->enc_vpumem, 
        ((EncParam->width + 15) & 0xfff0) * EncParam->height * 4);
    if (ret) {

        mpp_err("err  %dtemp->phy_addr %x mWidth %d mHeight %d", ret, pVideoEnc->enc_vpumem->phy_addr,
                EncParam->width, EncParam->height);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }


    if (rga_dev_open(&pVideoEnc->rga_ctx)  < 0) {
        mpp_err("open rga device fail!");
    }

    pVideoEnc->bRgb2yuvFlag = OMX_FALSE;
    pVideoEnc->bPixel_format = -1;
#ifdef AVS80
    pVideoEnc->ConfigColorAspects.sAspects.mRange = RangeUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mPrimaries = PrimariesUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mMatrixCoeffs = MatrixUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mTransfer = TransferUnspecified;
#endif
    FP_Enc_DebugSwitchfromPropget(pFpComponent);

    pVideoEnc->vpu_ctx = p_vpu_ctx;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_Enc_GetEncParams(OMX_COMPONENTTYPE *pOMXComponent, EncParameter_t **encParams)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT    *pVideoEnc         = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FunctionIn();

    (*encParams)->height = pVideoEnc->bCurrent_height;
    (*encParams)->width = pVideoEnc->bCurrent_width;
    (*encParams)->bitRate = pFpOutputPort->portDefinition.format.video.nBitrate;
    (*encParams)->framerate = (pFpInputPort->portDefinition.format.video.xFramerate) >> 16;

    if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
        (*encParams)->enableCabac   = 0;
        (*encParams)->cabacInitIdc  = 0;
        (*encParams)->intraPicRate  = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames;
        switch (pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].eProfile) {
        case OMX_VIDEO_AVCProfileBaseline:
            (*encParams)->profileIdc = BASELINE_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            (*encParams)->profileIdc   = MAIN_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            (*encParams)->profileIdc   = HIGHT_PROFILE;
            break;
        default:
            (*encParams)->profileIdc   = BASELINE_PROFILE;
            break;
        }
        switch (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
        case OMX_Video_ControlRateDisable:
            (*encParams)->rc_mode = Video_RC_Mode_Disable;
            break;
        case OMX_Video_ControlRateVariable:
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        case OMX_Video_ControlRateConstant:
            (*encParams)->rc_mode = Video_RC_Mode_CBR;
            break;
        default:
            mpp_err("unknown rate control mode = %d, forced to VBR mode",
                    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        }
        switch ((uint32_t)pFpInputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatAndroidOpaque: {
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            (*encParams)->format = VPU_H264ENC_RGB888;
        }
        break;
        case OMX_COLOR_FormatYUV420Planar: {
            (*encParams)->format = VPU_H264ENC_YUV420_PLANAR;
        }
        break;
        case OMX_COLOR_FormatYUV420SemiPlanar: {
            (*encParams)->format = VPU_H264ENC_YUV420_SEMIPLANAR;
        }
        break;
        default:
            mpp_err("inputPort colorformat is not support format = %d",
                    pFpInputPort->portDefinition.format.video.eColorFormat);
            break;
        }
        ConvertOmxAvcLevelToAvcSpecLevel((int32_t)pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].eLevel, (AVCLevel *) & ((*encParams)->levelIdc));
    } else if (pVideoEnc->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC) {
        (*encParams)->enableCabac   = 0;
        (*encParams)->cabacInitIdc  = 0;
        (*encParams)->intraPicRate  = pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval;

        ConvertOmxHevcProfile2HalHevcProfile(pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].eProfile,
                                             (HEVCEncProfile *) & ((*encParams)->profileIdc));
        ConvertOmxHevcLevel2HalHevcLevel(pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].eLevel,
                                         (HEVCLevel *) & ((*encParams)->levelIdc));
        switch (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
        case OMX_Video_ControlRateDisable:
            (*encParams)->rc_mode = Video_RC_Mode_Disable;
            break;
        case OMX_Video_ControlRateVariable:
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        case OMX_Video_ControlRateConstant:
            (*encParams)->rc_mode = Video_RC_Mode_CBR;
            break;
        default:
            mpp_err("unknown rate control mode = %d, forced to VBR mode",
                    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        }
        switch ((uint32_t)pFpInputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatAndroidOpaque: {
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            (*encParams)->format = VPU_H264ENC_RGB888;
        }
        break;
        case OMX_COLOR_FormatYUV420Planar: {
            (*encParams)->format = VPU_H264ENC_YUV420_PLANAR;
        }
        break;
        case OMX_COLOR_FormatYUV420SemiPlanar: {
            (*encParams)->format = VPU_H264ENC_YUV420_SEMIPLANAR;
        }
        break;
        default:
            mpp_err("inputPort colorformat is not support format = %d",
                    pFpInputPort->portDefinition.format.video.eColorFormat);
            break;
        }
    }

    mpp_log("encode params init settings:\n"
             "width = %d\n"
             "height = %d\n"
             "bitRate = %d\n"
             "framerate = %d\n"
             "format = %d\n"
             "enableCabac = %d,\n"
             "cabacInitIdc = %d,\n"
             "intraPicRate = %d,\n"
             "profileIdc = %d,\n"
             "levelIdc = %d,\n"
             "rc_mode = %d,\n",
             (int)(*encParams)->width,
             (int)(*encParams)->height,
             (int)(*encParams)->bitRate,
             (int)(*encParams)->framerate,
             (int)(*encParams)->format,
             (int)(*encParams)->enableCabac,
             (int)(*encParams)->cabacInitIdc,
             (int)(*encParams)->intraPicRate,
             (int)(*encParams)->profileIdc,
             (int)(*encParams)->levelIdc,
             (int)(*encParams)->rc_mode);

    FunctionOut();
    return ret;
}

OMX_ERRORTYPE FP_Enc_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE              ret           = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc     = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    int i, plane;
    FunctionIn();
    if (pVideoEnc->vpu_ctx) {
        if (pVideoEnc->rkvpu_close_cxt) {
            pVideoEnc->rkvpu_close_cxt(&pVideoEnc->vpu_ctx);
        }
        pVideoEnc->vpu_ctx = NULL;
        if (pVideoEnc->rkapi_hdl) {
            dlclose( pVideoEnc->rkapi_hdl);
            pVideoEnc->rkapi_hdl = NULL;
        }
    }

    if (pVideoEnc->bSpsPpsbuf) {
        mpp_free(pVideoEnc->bSpsPpsbuf);
        pVideoEnc->bSpsPpsbuf = NULL;
    }

    if (pVideoEnc->enc_vpumem) {
        VPUFreeLinear(pVideoEnc->enc_vpumem);
        mpp_free(pVideoEnc->enc_vpumem);
        pVideoEnc->enc_vpumem = NULL;
    }

    if (pVideoEnc->rga_ctx != NULL) {
        rga_dev_close(pVideoEnc->rga_ctx);
        pVideoEnc->rga_ctx = NULL;
    }

    pVideoEnc->bEncSendEos = OMX_FALSE;

    FP_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}


OMX_ERRORTYPE FP_OMX_ComponentConstructor(OMX_HANDLETYPE hComponent, OMX_STRING componentName)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        mpp_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = FP_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        mpp_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = FP_OMX_BaseComponent_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        mpp_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = FP_OMX_Port_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        FP_OMX_BaseComponent_Destructor(pOMXComponent);
        mpp_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = mpp_malloc(FP_OMX_VIDEOENC_COMPONENT, 1);
    if (pVideoEnc == NULL) {
        FP_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    memset(pVideoEnc, 0, sizeof(FP_OMX_VIDEOENC_COMPONENT));

#ifdef USE_ION
    pVideoEnc->hSharedMemory = OSAL_SharedMemory_Open();
    if ( pVideoEnc->hSharedMemory == NULL) {
        mpp_err("OSAL_SharedMemory_Open open fail");
    }
#endif

    pFpComponent->componentName = mpp_malloc(char, MAX_OMX_COMPONENT_NAME_SIZE);
    if (pFpComponent->componentName == NULL) {
        FP_OMX_ComponentDeInit(hComponent);
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    /* Set componentVersion */
    pFpComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pFpComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pFpComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pFpComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pFpComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pFpComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pFpComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pFpComponent->specVersion.s.nStep         = STEP_NUMBER;
    memset(pFpComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);
    pFpComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoEnc;

    pFpComponent->bSaveFlagEOS = OMX_FALSE;
    pFpComponent->bBehaviorEOS = OMX_FALSE;
    pFpComponent->bMultiThreadProcess = OMX_TRUE;
    pFpComponent->codecType = HW_VIDEO_ENC_CODEC;

    pVideoEnc->bFirstFrame = OMX_TRUE;
    pVideoEnc->bFirstInput = OMX_TRUE;
    pVideoEnc->bFirstOutput = OMX_TRUE;
    pVideoEnc->configChange = OMX_FALSE;
    pVideoEnc->bStoreMetaData = OMX_FALSE;
    pVideoEnc->bPrependSpsPpsToIdr = OMX_FALSE;
    pVideoEnc->bRkWFD = OMX_FALSE;
    pVideoEnc->quantization.nQpI = 4; // I frame quantization parameter
    pVideoEnc->quantization.nQpP = 5; // P frame quantization parameter
    pVideoEnc->quantization.nQpB = 5; // B frame quantization parameter
    //add by xlm for use mpp or vpuapi
    pVideoEnc->bIsUseMpp = OMX_FALSE;
    pVideoEnc->bIsNewVpu = OMX_TRUE;

    pVideoEnc->bScale_Mutex = MUTEX_CREATE();
    pVideoEnc->bRecofig_Mutex = MUTEX_CREATE();

    /* Input port */
    pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    pFoilplanetPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_INPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_INPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pFoilplanetPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pFoilplanetPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pFoilplanetPort->portDefinition.format.video.xFramerate = DEFAULT_ENC_FRAME_FRAMERATE;
    pFoilplanetPort->portDefinition.format.video.nBitrate = DEFAULT_ENC_FRAME_BITRATE;
    pFoilplanetPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pFoilplanetPort->portDefinition.format.video.nSliceHeight = 0;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pFoilplanetPort->portDefinition.format.video.cMIMEType =  mpp_malloc(char, MAX_OMX_MIMETYPE_SIZE);
    strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "raw/video");
    pFoilplanetPort->portDefinition.format.video.pNativeRender = 0;
    pFoilplanetPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFoilplanetPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pFoilplanetPort->portDefinition.bEnabled = OMX_TRUE;
    pFoilplanetPort->portWayType = WAY2_PORT;
    pVideoEnc->eControlRate[INPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;
    pFoilplanetPort->bStoreMetaData = OMX_FALSE;

    /* Output port */
    pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    pFoilplanetPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pFoilplanetPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pFoilplanetPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pFoilplanetPort->portDefinition.format.video.xFramerate = DEFAULT_ENC_FRAME_FRAMERATE;
    pFoilplanetPort->portDefinition.format.video.nBitrate = DEFAULT_ENC_FRAME_BITRATE;
    pFoilplanetPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pFoilplanetPort->portDefinition.format.video.nSliceHeight = 0;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pFoilplanetPort->portDefinition.format.video.cMIMEType = mpp_malloc(char, MAX_OMX_MIMETYPE_SIZE);
    memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    pFoilplanetPort->portDefinition.format.video.pNativeRender = 0;
    pFoilplanetPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFoilplanetPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pFoilplanetPort->portDefinition.bEnabled = OMX_TRUE;
    pFoilplanetPort->portWayType = WAY2_PORT;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;


    pOMXComponent->UseBuffer              = &FP_OMX_UseBuffer;
    pOMXComponent->AllocateBuffer         = &FP_OMX_AllocateBuffer;
    pOMXComponent->FreeBuffer             = &FP_OMX_FreeBuffer;
    pOMXComponent->ComponentTunnelRequest = &FP_OMX_ComponentTunnelRequest;
    pOMXComponent->GetParameter           = &FPV_OMX_GetParameter;
    pOMXComponent->SetParameter           = &FPV_OMX_SetParameter;
    pOMXComponent->GetConfig              = &FPV_OMX_GetConfig;
    pOMXComponent->SetConfig              = &FPV_OMX_SetConfig;
    pOMXComponent->GetExtensionIndex      = &FPV_OMX_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum      = &FP_OMX_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit        = &FP_OMX_ComponentDeInit;

    pFpComponent->fp_codec_componentInit      = &FP_Enc_ComponentInit;
    pFpComponent->fp_codec_componentTerminate = &FP_Enc_Terminate;

    pFpComponent->fp_AllocateTunnelBuffer   = &FP_OMX_AllocateTunnelBuffer;
    pFpComponent->fp_FreeTunnelBuffer       = &FP_OMX_FreeTunnelBuffer;
    pFpComponent->fp_BufferProcessCreate    = &FP_OMX_BufferProcess_Create;
    pFpComponent->fp_BufferProcessTerminate = &FP_OMX_BufferProcess_Terminate;
    pFpComponent->fp_BufferFlush            = &FP_OMX_BufferFlush;

    if (!strcmp(componentName, RK_OMX_COMPONENT_H264_ENC)) {
        int i = 0;
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/avc");
        for (i = 0; i < ALL_PORT_NUM; i++) {
            INIT_SET_SIZE_VERSION(&pVideoEnc->AVCComponent[i], OMX_VIDEO_PARAM_AVCTYPE);
            pVideoEnc->AVCComponent[i].nPortIndex = i;
            pVideoEnc->AVCComponent[i].eProfile   = OMX_VIDEO_AVCProfileBaseline;
            pVideoEnc->AVCComponent[i].eLevel     = OMX_VIDEO_AVCLevel31;
            pVideoEnc->AVCComponent[i].nPFrames = 20;
        }
        pVideoEnc->codecId = OMX_VIDEO_CodingAVC;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    } else if (!strcmp(componentName, RK_OMX_COMPONENT_VP8_ENC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp8");
        pVideoEnc->codecId = OMX_VIDEO_CodingVP8;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
    } else if (!strcmp(componentName, RK_OMX_COMPONENT_HEVC_ENC)) {
        int i = 0;
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/hevc");
        for (i = 0; i < ALL_PORT_NUM; i++) {
            INIT_SET_SIZE_VERSION(&pVideoEnc->HEVCComponent[i], OMX_VIDEO_PARAM_HEVCTYPE);
            pVideoEnc->HEVCComponent[i].nPortIndex = i;
            pVideoEnc->HEVCComponent[i].eProfile   = OMX_VIDEO_HEVCProfileMain;
            pVideoEnc->HEVCComponent[i].eLevel     = OMX_VIDEO_HEVCMainTierLevel41;
            pVideoEnc->HEVCComponent[i].nKeyFrameInterval = 20;
        }
        pVideoEnc->codecId = OMX_VIDEO_CodingHEVC;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
    } else {
        // IL client specified an invalid component name
        mpp_err("VPU Component Invalid Component Name\n");
        ret =  OMX_ErrorInvalidComponentName;
        goto EXIT;
    }
    pFpComponent->currentState = OMX_StateLoaded;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    FP_OMX_BASECOMPONENT *pFpComponent = NULL;
    FP_OMX_BASEPORT      *pFoilplanetPort = NULL;
    FP_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    int                    i = 0;

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

    pVideoEnc = (FP_OMX_VIDEOENC_COMPONENT *)pFpComponent->hComponentHandle;
    if (pVideoEnc->fp_enc_out != NULL) {
        fclose(pVideoEnc->fp_enc_out);
    }

    MUTEX_FREE(pVideoEnc->bScale_Mutex);
    MUTEX_FREE(pVideoEnc->bRecofig_Mutex);

#ifdef USE_ION
    if (pVideoEnc->hSharedMemory != NULL) {
        OSAL_SharedMemory_Close(pVideoEnc->hSharedMemory, OMX_FALSE);
        pVideoEnc->hSharedMemory = NULL;
    }
#endif

    mpp_free(pVideoEnc);
    pFpComponent->hComponentHandle = pVideoEnc = NULL;

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    if (pFoilplanetPort->processData.extInfo != NULL) {
        mpp_free(pFoilplanetPort->processData.extInfo);
        pFoilplanetPort->processData.extInfo = NULL;
    }

    for (i = 0; i < ALL_PORT_NUM; i++) {
        pFoilplanetPort = &pFpComponent->pFoilplanetPort[i];
        mpp_free(pFoilplanetPort->portDefinition.format.video.cMIMEType);
        pFoilplanetPort->portDefinition.format.video.cMIMEType = NULL;
    }

    ret = FP_OMX_Port_Destructor(pOMXComponent);

    ret = FP_OMX_BaseComponent_Destructor(hComponent);

EXIT:
    FunctionOut();

    return ret;
}
