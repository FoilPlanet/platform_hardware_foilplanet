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
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <time.h>
#include <sys/time.h>

#include "OMX_Macros.h"
#include "Foilplanet_OMX_Basecomponent.h"

#include "vdec.h"
#include "vdec_control.h"

//#include "vpu_mem_pool.h"
//#include "vpu_api_private_cmd.h"
//#include "vpu_mem.h"

#include "osal_android.h"
#include "osal_event.h"
#include "osal_rga.h"
#include "osal/mpp_thread.h"
#include "osal/mpp_list.h"
#include "osal/mpp_mem.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG     "FP_OMX_VDEC"
#endif

#define VPU_API_SET_IMMEDIATE_OUT       (VPU_API_CMD)0x1000
#define VPU_API_DEC_GET_STREAM_TOTAL    (VPU_API_CMD)0x2000

#define GET_MPPLIST(q)      ((mpp_list *)q)

/** vpu_api_private_cmd.h (rkvpu)
 */
typedef enum VPU_API_PRIVATE_CMD {
    VPU_API_PRIVATE_CMD_NONE        = 0x0,
    VPU_API_PRIVATE_HEVC_NEED_PARSE = 0x1000,
} VPU_API_PRIVATE_CMD;

typedef struct {
    OMX_RK_VIDEO_CODINGTYPE codec_id;
    OMX_VIDEO_CODINGTYPE     omx_id;
} CodeMap;

static const CodeMap kCodeMap[] = {
    { OMX_RK_VIDEO_CodingMPEG2, OMX_VIDEO_CodingMPEG2},
    { OMX_RK_VIDEO_CodingH263,  OMX_VIDEO_CodingH263},
    { OMX_RK_VIDEO_CodingMPEG4, OMX_VIDEO_CodingMPEG4},
    { OMX_RK_VIDEO_CodingVC1,   (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1},
    // { OMX_RK_VIDEO_CodingRV,    OMX_VIDEO_CodingRV},
    { OMX_RK_VIDEO_CodingAVC,   OMX_VIDEO_CodingAVC},
    { OMX_RK_VIDEO_CodingMJPEG, OMX_VIDEO_CodingMJPEG},
    { OMX_RK_VIDEO_CodingFLV1,  (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1},
    { OMX_RK_VIDEO_CodingVP8,   OMX_VIDEO_CodingVP8},
//   { OMX_RK_VIDEO_CodingVP6,   OMX_VIDEO_CodingVP6},
    { OMX_RK_VIDEO_CodingWMV,   OMX_VIDEO_CodingWMV},
//  { OMX_RK_VIDEO_CodingDIVX3, OMX_VIDEO_CodingDIVX3 },
    { OMX_RK_VIDEO_CodingHEVC,   OMX_VIDEO_CodingHEVC},
    { OMX_RK_VIDEO_CodingVP9,   OMX_VIDEO_CodingVP9},
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

static void controlFPS(OMX_BOOL isInput)
{
    static int inFrameCount = 0;
    static int inLastFrameCount = 0;
    static long inLastFpsTimeUs = 0;
    static float inFps = 0;
    static long inDiff = 0;
    static long inNowUs = 0;
    static int outFrameCount = 0;
    static int outLastFrameCount = 0;
    static long outLastFpsTimeUs = 0;
    static float outFps = 0;
    static long outDiff = 0;
    static long outNowUs = 0;

    if (isInput == OMX_TRUE) {
        inFrameCount++;
        if (!(inFrameCount & 0x1F)) {
            struct timeval now;
            gettimeofday(&now, NULL);
            inNowUs = (long)now.tv_sec * 1000000 + (long)now.tv_usec;
            inDiff = inNowUs - inLastFpsTimeUs;
            inFps = ((float)(inFrameCount - inLastFrameCount) * 1.0f) * 1000.0f * 1000.0f / (float)inDiff;
            inLastFpsTimeUs = inNowUs;
            inLastFrameCount = inFrameCount;
            mpp_err("decode input frameCount = %d frameRate = %f HZ", inFrameCount, inFps);
        }
    } else {
        outFrameCount++;
        if (!(outFrameCount & 0x1F)) {
            struct timeval now;
            gettimeofday(&now, NULL);
            outNowUs = (long)now.tv_sec * 1000000 + (long)now.tv_usec;
            outDiff = outNowUs - outLastFpsTimeUs;
            outFps = ((float)(outFrameCount - outLastFrameCount) * 1.0f) * 1000.0f * 1000.0f / (float)outDiff;
            outLastFpsTimeUs = outNowUs;
            outLastFrameCount = outFrameCount;
            mpp_err("decode output frameCount = %d frameRate = %f HZ", outFrameCount, outFps);
        }
    }
    return;
}


void UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    if ((fpOutputPort->portDefinition.format.video.nFrameWidth !=
         fpInputPort->portDefinition.format.video.nFrameWidth) ||
        (fpOutputPort->portDefinition.format.video.nFrameHeight !=
         fpInputPort->portDefinition.format.video.nFrameHeight) ||
        (fpOutputPort->portDefinition.format.video.nStride !=
         fpInputPort->portDefinition.format.video.nStride) ||
        (fpOutputPort->portDefinition.format.video.nSliceHeight !=
         fpInputPort->portDefinition.format.video.nSliceHeight)) {
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

OMX_ERRORTYPE FP_OMX_CheckIsNeedFastmode(
    FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE              ret         = OMX_ErrorNone;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec   = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    VpuCodecContext_t         *p_vpu_ctx   = pVideoDec->vpu_ctx;
    if (pVideoDec->bFastMode == OMX_FALSE
        && pVideoDec->codecId == OMX_VIDEO_CodingHEVC
        && pInputPort->portDefinition.format.video.nFrameWidth > 1920
        && pInputPort->portDefinition.format.video.nFrameHeight > 1080) {
        pVideoDec->bFastMode = OMX_TRUE;
        int fast_mode = 1;
        p_vpu_ctx->control(p_vpu_ctx, VPU_API_USE_FAST_MODE, &fast_mode);
        mpp_log("used fast mode, h265decoder, width = %d, height = %d",
                 pInputPort->portDefinition.format.video.nFrameWidth,
                 pInputPort->portDefinition.format.video.nFrameHeight);
    }
    return ret;
}

OMX_ERRORTYPE FP_OMX_DebugSwitchfromPropget(
    FP_OMX_BASECOMPONENT *pFpComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    FP_OMX_VIDEODEC_COMPONENT  *pVideoDec         = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    char                           value[PROPERTY_VALUE_MAX];
    memset(value, 0, sizeof(value));
    if (property_get("record_omx_dec_in", value, "0") && (atoi(value) > 0)) {
        mpp_log("Start recording stream to /data/video/dec_in.bin");
        if (pVideoDec->fp_in != NULL) {
            fclose(pVideoDec->fp_in);
        }
        pVideoDec->fp_in = fopen("data/video/dec_in.bin", "wb");
    }

    memset(value, 0, sizeof(value));
    if (property_get("dump_omx_fps", value, "0") && (atoi(value) > 0)) {
        mpp_log("Start print framerate when frameCount = 32");
        pVideoDec->bPrintFps = OMX_TRUE;
    }

    memset(value, 0, sizeof(value));
    if (property_get("dump_omx_buf_position", value, "0") && (atoi(value) > 0)) {
        mpp_log("print all buf position");
        pVideoDec->bPrintBufferPosition = OMX_TRUE;
    }

    memset(value, 0, sizeof(value));
    if (property_get("cts_gts.media.gts", value, NULL) && (!strcasecmp(value, "true"))) {
        mpp_log("This is gts media test.");
        pVideoDec->bGtsMediaTest = OMX_TRUE;
    }

    return ret;
}

OMX_ERRORTYPE FP_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret           = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT  *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_BASEPORT       *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT       *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    /* Input port */
    pFpInputPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pFpInputPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pFpInputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pFpInputPort->portDefinition.format.video.nSliceHeight = 0;
    pFpInputPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pFpInputPort->portDefinition.format.video.pNativeRender = 0;
    pFpInputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFpInputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pFpInputPort->portDefinition.bEnabled = OMX_TRUE;
    pFpInputPort->bufferProcessType = BUFFER_COPY;
    pFpInputPort->portWayType = WAY2_PORT;

    /* Output port */
    pFpOutputPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pFpOutputPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pFpOutputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pFpOutputPort->portDefinition.format.video.nSliceHeight = 0;
    pFpOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pFpOutputPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    memset(pFpOutputPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    strcpy(pFpOutputPort->portDefinition.format.video.cMIMEType, "raw/video");
    pFpOutputPort->portDefinition.format.video.pNativeRender = 0;
    pFpOutputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFpOutputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pFpOutputPort->portDefinition.nBufferCountActual = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pFpOutputPort->portDefinition.nBufferCountMin = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pFpOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pFpOutputPort->portDefinition.bEnabled = OMX_TRUE;
    pFpOutputPort->bufferProcessType = (FP_OMX_BUFFERPROCESS_TYPE)(BUFFER_COPY | BUFFER_ANBSHARE);
    pFpOutputPort->portWayType = WAY2_PORT;

    return ret;
}

void FP_Wait_ProcessPause(FP_OMX_BASECOMPONENT *pFpComponent, OMX_U32 nPortIndex)
{
    FP_OMX_BASEPORT *fpOMXInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT *fpOMXOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_BASEPORT *fpOMXPort = NULL;

    FunctionIn();

    fpOMXPort = &pFpComponent->pFoilplanetPort[nPortIndex];

    if (((pFpComponent->currentState == OMX_StatePause) ||
         (pFpComponent->currentState == OMX_StateIdle) ||
         (pFpComponent->transientState == FP_OMX_TransStateLoadedToIdle) ||
         (pFpComponent->transientState == FP_OMX_TransStateExecutingToIdle)) &&
        (pFpComponent->transientState != FP_OMX_TransStateIdleToLoaded) &&
        (!CHECK_PORT_BEING_FLUSHED(fpOMXPort))) {

        OSAL_SignalWait(pFpComponent->pFoilplanetPort[nPortIndex].pauseEvent, DEF_MAX_WAIT_TIME);
        OSAL_SignalReset(pFpComponent->pFoilplanetPort[nPortIndex].pauseEvent);
    }

    FunctionOut();

    return;
}

OMX_BOOL FP_SendInputData(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_BOOL               ret = OMX_FALSE;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *inputUseBuffer = &fpInputPort->way.port2WayDataBuffer.inputDataBuffer;
    VpuCodecContext_t *p_vpu_ctx = pVideoDec->vpu_ctx;
    OMX_S32 i = 0;
    OMX_S32 numInOmxAl = 0;
    OMX_S32 temp_size;
    OMX_S32 maxBufferNum = fpInputPort->portDefinition.nBufferCountActual;
    OMX_S32 dec_ret = 0;
    FunctionIn();

    for (i = 0; i < maxBufferNum; i++) {
        if (fpInputPort->extendBufferHeader[i].bBufferInOMX == OMX_FALSE) {
            numInOmxAl++;
        }
    }

    if (pVideoDec->bPrintBufferPosition) {
        mpp_err("in buffer position: in app and display num = %d", numInOmxAl);
        mpp_err("in buffer position: in omx and vpu num = %d", maxBufferNum - numInOmxAl);
    }

    if (inputUseBuffer->dataValid == OMX_TRUE) {
        VideoPacket_t pkt;
        if (pVideoDec->bFirstFrame == OMX_TRUE) {
            OMX_U8 *extraData = NULL;
            OMX_U32 extraSize = 0;
            OMX_U32 extraFlag = 0;
            OMX_U32 enableDinterlace = 1;
            if (((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA) == OMX_BUFFERFLAG_EXTRADATA)
                || ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG) == OMX_BUFFERFLAG_CODECCONFIG)) {
                if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
                    mpp_log("inputUseBuffer->bufferHeader->pBuffer = %p", inputUseBuffer->bufferHeader->pBuffer);
                    extraData = inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
                    if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
#ifdef USE_ION
                        OMX_U32 trueAddress = OSAL_SharedMemory_HandleToAddress(pVideoDec->hSharedMemory, (OMX_HANDLETYPE)extraData);
                        extraData = (OMX_PTR)((__u64)trueAddress);
#endif
                    }
                    mpp_log("extraData = %p", extraData);
                } else {
                    mpp_log("FP_SendInputData malloc");
                    extraData = mpp_malloc(OMX_U8, inputUseBuffer->dataLen);
                    if (extraData == NULL) {
                        mpp_err("malloc Extra Data fail");
                        ret = OMX_FALSE;
                        goto EXIT;
                    }

                    memcpy(extraData, inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen,
                           inputUseBuffer->dataLen);
                }
                if (pVideoDec->fp_in != NULL) {
                    fwrite(extraData, 1, inputUseBuffer->dataLen, pVideoDec->fp_in);
                    fflush(pVideoDec->fp_in);
                }
                extraSize = inputUseBuffer->dataLen;
                extraFlag = 1;
            }

            mpp_log("decode init");
            //add by xhr
            if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
                mpp_log("set secure_mode");
                OMX_U32 coding;
                OMX_U32 is4kflag = 0;
                /* 4K, goto rkv, 1080P goto vdpu */
                if (p_vpu_ctx->width > 1920 || p_vpu_ctx->height >  1088) {
                    mpp_log("set secure_mode 4K");
                    is4kflag = 1;
                }
                coding = p_vpu_ctx->videoCoding | (is4kflag << 31);
                
                // TODO: VPU_API_ENC_SET_VEPU22_CFG ?
                #define VPU_API_SET_SECURE_CONTEXT (VPU_API_CMD)0x2001
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_SECURE_CONTEXT, &coding);
            }

            p_vpu_ctx->init(p_vpu_ctx, extraData, extraSize);
            // not use iep when thumbNail decode
            if (!(pVideoDec->flags & FP_OMX_VDEC_THUMBNAIL)) {
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENABLE_DEINTERLACE, &enableDinterlace);
            }
            if (pVideoDec->vpumem_handle != NULL) {
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_VPUMEM_CONTEXT, pVideoDec->vpumem_handle);
            }

            if (fpInputPort->portDefinition.format.video.bFlagErrorConcealment) {
                mpp_log("use directly output mode for media");
                RK_U32 flag = 1;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_IMMEDIATE_OUT, (void*)&flag);
            }

            if (p_vpu_ctx->videoCoding == OMX_RK_VIDEO_CodingHEVC) {
                p_vpu_ctx->control(p_vpu_ctx, (VPU_API_CMD)VPU_API_PRIVATE_HEVC_NEED_PARSE, NULL);
            }

            pVideoDec->bFirstFrame = OMX_FALSE;
            if (extraFlag) {
                ret = OMX_TRUE;
                if (extraData  && !pVideoDec->bDRMPlayerMode) {
                    mpp_free(extraData);
                    extraData = NULL;
                    FP_InputBufferReturn(pOMXComponent, inputUseBuffer);
                } else if (extraData && pVideoDec->bDRMPlayerMode) {
                    inputUseBuffer->dataValid = OMX_FALSE;
                    FP_OMX_DATABUFFER * inputInValidBuffer;
                    inputInValidBuffer = mpp_malloc(FP_OMX_DATABUFFER, 1);
                    if (inputInValidBuffer == NULL) {
                        mpp_err("inputInValidBuffer malloc failed!");
                        return OMX_FALSE;
                    }
                    inputInValidBuffer->bufferHeader = inputUseBuffer->bufferHeader;
                    inputInValidBuffer->dataLen = inputUseBuffer->dataLen;
                    inputInValidBuffer->timeStamp = inputUseBuffer->timeStamp;

                    MUTEX_LOCK(fpInputPort->secureBufferMutex);
                    GET_MPPLIST(fpInputPort->securebufferQ)->add_at_tail(inputInValidBuffer, sizeof(FP_OMX_DATABUFFER));
                    MUTEX_UNLOCK(fpInputPort->secureBufferMutex);

                } else {
                    // NOTHING
                }

                goto EXIT;
            }
        }

        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_log("bSaveFlagEOS : OMX_TRUE");
            pFpComponent->bSaveFlagEOS = OMX_TRUE;
            //  if (inputUseBuffer->dataLen != 0)
        }
        memset(&pkt, 0, sizeof(VideoPacket_t));
        pkt.data =  inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
        mpp_log("in sendInputData data = %p", pkt.data);
        if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
#ifdef AVS80
            OMX_U32 trueAddress = OSAL_SharedMemory_HandleToAddress(pVideoDec->hSharedMemory, (OMX_HANDLETYPE)pkt.data);
            pkt.data = (OMX_PTR)((__u64)trueAddress);
#endif
            mpp_log("out sendInputData data = %p", pkt.data);
        }
        pkt.size = inputUseBuffer->dataLen;

        if (pVideoDec->flags & FP_OMX_VDEC_USE_DTS) {
            pkt.pts = VPU_API_NOPTS_VALUE;
            pkt.dts = inputUseBuffer->timeStamp;
        } else {
            pkt.pts = pkt.dts = inputUseBuffer->timeStamp;
        }
        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            mpp_log("send eos");
            pkt.nFlags |= OMX_BUFFERFLAG_EOS;
        }
        mpp_log("pkt.size:%d, pkt.dts:%lld,pkt.pts:%lld,pkt.nFlags:%d",
                  pkt.size, pkt.dts, pkt.pts, pkt.nFlags);
        mpp_log("decode_sendstream pkt.data = %p", pkt.data);
        dec_ret = p_vpu_ctx->decode_sendstream(p_vpu_ctx, &pkt);
        if (dec_ret < 0) {
            mpp_err("decode_sendstream failed , ret = %x", dec_ret);
            /*
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pFpComponent->callbackData, OMX_EventError,
                                                             INPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
            FP_InputBufferReturn(pOMXComponent, inputUseBuffer);
            ret = OMX_TRUE;
            goto EXIT;*/
        }
        if (pkt.size != 0) {
            mpp_err("stream list full wait");
            goto EXIT;
        }
        // mpp_log("decode_sendstream pkt.data = %p",pkt.data);
        if (pVideoDec->bPrintFps == OMX_TRUE) {
            OMX_BOOL isInput = OMX_TRUE;
            controlFPS(isInput);
        }


        if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
            inputUseBuffer->dataValid = OMX_FALSE;
            FP_OMX_DATABUFFER * inputInValidBuffer;
            inputInValidBuffer = mpp_malloc(FP_OMX_DATABUFFER, 1);
            if (inputInValidBuffer == NULL) {
                mpp_err("inputInValidBuffer malloc failed!");
                return OMX_FALSE;
            }
            inputInValidBuffer->bufferHeader = inputUseBuffer->bufferHeader;
            inputInValidBuffer->dataLen = inputUseBuffer->dataLen;
            inputInValidBuffer->timeStamp = inputUseBuffer->timeStamp;
            MUTEX_LOCK(fpInputPort->secureBufferMutex);
            GET_MPPLIST(fpInputPort->securebufferQ)->add_at_tail(inputInValidBuffer, sizeof(FP_OMX_DATABUFFER));
            MUTEX_UNLOCK(fpInputPort->secureBufferMutex);
        } else {
            FP_InputBufferReturn(pOMXComponent, inputUseBuffer);
        }
        if (pFpComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pFpComponent->checkTimeStamp.startTimeStamp = inputUseBuffer->timeStamp;
            pFpComponent->checkTimeStamp.nStartFlags = inputUseBuffer->nFlags;
            pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
            mpp_log("first frame timestamp after seeking %lld us (%.2f secs)",
                      inputUseBuffer->timeStamp, inputUseBuffer->timeStamp / 1E6);
        }
        ret = OMX_TRUE;
    }

EXIT:
    FunctionOut();
    return ret;
}

OMX_BOOL FP_Post_OutputFrame(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_BOOL                  ret = OMX_FALSE;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec     = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pInputPort    = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pOutputPort   = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER         *outputUseBuffer = &pOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    VpuCodecContext_t         *p_vpu_ctx = pVideoDec->vpu_ctx;
    OMX_U32         pOWnBycomponetNum = GET_MPPLIST(pOutputPort->bufferQ)->list_size();
    OMX_S32 maxBufferNum = 0;
    OMX_S32 i = 0, numInOmxAl = 0, limitNum = 8;
    OMX_S32 bufferUnusedInVpu = 0;
    FunctionIn();
    if (p_vpu_ctx == NULL ||
        (pVideoDec->bFirstFrame == OMX_TRUE) ||
        (pVideoDec->bDecSendEOS == OMX_TRUE)) {
        goto EXIT;
    }
    maxBufferNum = pOutputPort->portDefinition.nBufferCountActual;
    for (i = 0; i < maxBufferNum; i++) {
        if (pOutputPort->extendBufferHeader[i].bBufferInOMX == OMX_FALSE) {
            numInOmxAl++;
        }
    }
    if (pVideoDec->bPrintBufferPosition) {
        struct vpu_display_mem_pool *pMem_pool = (struct vpu_display_mem_pool*)pVideoDec->vpumem_handle;
        bufferUnusedInVpu = pMem_pool->get_unused_num(pMem_pool);
        mpp_log("out buffer position: in app and display num = %d", numInOmxAl);
        mpp_log("out buffer position: in omx and vpu num = %d", maxBufferNum - numInOmxAl);
        mpp_log("out buffer position: in component num = %d", pOWnBycomponetNum);
        mpp_log("out buffer position: in vpu unused buffer = %d", bufferUnusedInVpu);
    }
    if (pOutputPort->bufferProcessType == BUFFER_SHARE) {
        OMX_U32 width = 0, height = 0;
        int imageSize = 0;
        OMX_S32 dec_ret = 0;
        DecoderOut_t pOutput;
        VPU_FRAME *pframe = mpp_malloc(VPU_FRAME, 1);
        OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;
        memset(&pOutput, 0, sizeof(DecoderOut_t));
        memset(pframe, 0, sizeof(VPU_FRAME));
        pOutput.data = (unsigned char *)pframe;
        if ((numInOmxAl < limitNum) ||
            (pVideoDec->maxCount > 20)) {
            dec_ret =  p_vpu_ctx->decode_getframe(p_vpu_ctx, &pOutput);
            mpp_log("pOutput.size %d", pOutput.size);
            pVideoDec->maxCount = 0;
        } else {
            pVideoDec->maxCount++;
            mpp_log("pVideoDec 0x%x numInOmxAl %d", pVideoDec, numInOmxAl);
        }
        if (dec_ret < 0) {
            if (dec_ret == VPU_API_EOS_STREAM_REACHED && !pframe->ErrorInfo) {
                outputUseBuffer->dataLen = 0;
                outputUseBuffer->remainDataLen = 0;
                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                outputUseBuffer->timeStamp = 0;
                outputUseBuffer->dataValid = OMX_FALSE;
                ret = OMX_TRUE;
                pVideoDec->bDecSendEOS = OMX_TRUE;
                mpp_log("OMX_BUFFERFLAG_EOS");
            } else {
                mpp_err("OMX_DECODER ERROR");
                pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pFpComponent->callbackData, OMX_EventError,
                                                             OUTPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
            }
            FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
        }
        if (outputUseBuffer->dataValid == OMX_TRUE && (pOWnBycomponetNum > 0)) {
            mpp_log("commit fd to vpu 0x%x\n", outputUseBuffer->bufferHeader);
            OSAL_Fd2VpumemPool(pFpComponent, outputUseBuffer->bufferHeader);
            FP_ResetDataBuffer(outputUseBuffer);
        }
        if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
            int ret = 0;
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_DEC_GET_STREAM_TOTAL, &ret);
            //mpp_log("delete packet status = %d", ret);
            if (ret == 0) {
                MUTEX_LOCK(pInputPort->secureBufferMutex);
                FP_OMX_DATABUFFER *securebuffer = NULL;

                GET_MPPLIST(pInputPort->securebufferQ)->del_at_head(&securebuffer, sizeof(FP_OMX_DATABUFFER));

                if (securebuffer != NULL) {
#ifdef USE_ION
                    OMX_U8 *data;
                    OMX_U32 trueAddress = OSAL_SharedMemory_HandleToAddress(pVideoDec->hSharedMemory, (OMX_HANDLETYPE)securebuffer->bufferHeader->pBuffer);
                    data = (OMX_PTR)((__u64)trueAddress);
                    mpp_log("output secure buffer:%p", data);
#endif
                    FP_InputBufferReturn(pOMXComponent, securebuffer);
                    mpp_free(securebuffer);
                }
                MUTEX_UNLOCK(pInputPort->secureBufferMutex);
            }
        }
        /*
         *when decode frame (width > 8192 || Height > 4096), mpp not to check it
         *do not check here, ACodec will alloc large 4K size memory
         *cause lower memory fault
        */
        if (pframe->DisplayWidth > 8192 ||  pframe->DisplayHeight > 4096) {
            pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pFpComponent->callbackData,
                                                         OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            if (pframe->vpumem.phy_addr > 0) {
                VPUMemLink(&pframe->vpumem);
                VPUFreeLinear(&pframe->vpumem);
            }
            ret = OMX_FALSE;
            goto EXIT;
        }

        if ((pOutput.size > 0) && (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
            OMX_COLOR_FORMATTYPE eColorFormat = OSAL_CheckFormat(pFpComponent, pframe);
            if ((pInputPort->portDefinition.format.video.nFrameWidth != pframe->DisplayWidth) ||
                (pInputPort->portDefinition.format.video.nFrameHeight != pframe->DisplayHeight)
                || (pInputPort->portDefinition.format.video.nSliceHeight != pframe->FrameHeight)
                || (pInputPort->portDefinition.format.video.nStride != (OMX_S32)pframe->FrameWidth)
                || pOutputPort->portDefinition.format.video.eColorFormat != eColorFormat) {
                mpp_log("video.nFrameWidth %d video.nFrameHeight %d nSliceHeight %d",
                          pInputPort->portDefinition.format.video.nFrameWidth,
                          pInputPort->portDefinition.format.video.nFrameHeight,
                          pInputPort->portDefinition.format.video.nSliceHeight);

                mpp_log("video.nFrameWidth %d video.nFrameHeight %d pframe->FrameHeight %d",
                          pframe->DisplayWidth,
                          pframe->DisplayHeight, pframe->FrameHeight);

                pOutputPort->newCropRectangle.nWidth = pframe->DisplayWidth;
                pOutputPort->newCropRectangle.nHeight = pframe->DisplayHeight;
                pOutputPort->newPortDefinition.format.video.eColorFormat = eColorFormat;
                pOutputPort->newPortDefinition.nBufferCountActual = pOutputPort->portDefinition.nBufferCountActual;
                pOutputPort->newPortDefinition.nBufferCountMin = pOutputPort->portDefinition.nBufferCountMin;
                pInputPort->newPortDefinition.format.video.nFrameWidth = pframe->DisplayWidth;
                pInputPort->newPortDefinition.format.video.nFrameHeight = pframe->DisplayHeight;

                pInputPort->newPortDefinition.format.video.nStride         = pframe->FrameWidth;
                pInputPort->newPortDefinition.format.video.nSliceHeight    = pframe->FrameHeight;
                FP_ResolutionUpdate(pOMXComponent);
                pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pFpComponent->callbackData, OMX_EventPortSettingsChanged,
                                                             OUTPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
                if (pframe->vpumem.phy_addr > 0) {
                    VPUMemLink(&pframe->vpumem);
                    VPUFreeLinear(&pframe->vpumem);
                }
                mpp_free(pframe);
                OSAL_ResetVpumemPool(pFpComponent);
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_INFO_CHANGE, NULL);
                pVideoDec->bInfoChange = OMX_TRUE;
                goto EXIT;
            }

            if (pVideoDec->bPrintFps == OMX_TRUE) {
                OMX_BOOL isInput = OMX_FALSE;
                controlFPS(isInput);
            }

            if (pframe->ErrorInfo && (pVideoDec->bGtsMediaTest == OMX_FALSE) && (pVideoDec->bDRMPlayerMode == OMX_FALSE)) {   //drop frame when this frame mark error from dec
                mpp_err("this frame is Error frame!,pOutput.timeUs = %lld", pOutput.timeUs);
                if (pframe->vpumem.phy_addr > 0) {
                    VPUMemLink(&pframe->vpumem);
                    VPUFreeLinear(&pframe->vpumem);
                }
                goto EXIT;
            }

            bufferHeader = OSAL_Fd2OmxBufferHeader(pOutputPort, VPUMemGetFD(&pframe->vpumem), pframe);
            if (bufferHeader != NULL) {
                if (pVideoDec->bStoreMetaData == OMX_TRUE) {
                    bufferHeader->nFilledLen = bufferHeader->nAllocLen;
                    mpp_log("nfill len %d", bufferHeader->nFilledLen);
                } else {
                    bufferHeader->nFilledLen = pframe->DisplayHeight * pframe->DisplayWidth * 3 / 2;
                }
                bufferHeader->nOffset    = 0;
                if ((VPU_API_ERR)pOutput.nFlags == VPU_API_EOS_STREAM_REACHED) {
                    bufferHeader->nFlags |= OMX_BUFFERFLAG_EOS;
                    pVideoDec->bDecSendEOS = OMX_TRUE;
                } else {
                    bufferHeader->nFlags     = 0;
                }
                bufferHeader->nTimeStamp = pOutput.timeUs;
                mpp_log("FP_OutputBufferReturn %lld", pOutput.timeUs);
            } else {
                if (pframe->vpumem.phy_addr > 0) {
                    VPUMemLink(&pframe->vpumem);
                    VPUFreeLinear(&pframe->vpumem);
                }
                mpp_free(pframe);
                goto EXIT;
            }

            if ((bufferHeader->nFilledLen > 0) ||
                ((bufferHeader->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
                mpp_log("FP_OutputBufferReturn");
                FP_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
            }

            ret = OMX_TRUE;
        } else if (CHECK_PORT_BEING_FLUSHED(pOutputPort)) {
            if (pOutput.size && (pframe->vpumem.phy_addr > 0)) {
                VPUMemLink(&pframe->vpumem);
                VPUFreeLinear(&pframe->vpumem);
                mpp_free(pframe);
            }
            outputUseBuffer->dataLen = 0;
            outputUseBuffer->remainDataLen = 0;
            outputUseBuffer->nFlags = 0;
            outputUseBuffer->timeStamp = 0;
            ret = OMX_TRUE;
            FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
        } else {
            //mpp_err("output buffer is smaller than decoded data size Out Length");
            //pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
            //                                        pFpComponent->callbackData,
            //                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            if (pframe != NULL) {
                mpp_free(pframe);
                pframe = NULL;
            }
            ret = OMX_FALSE;
        }
    } else {
        if (outputUseBuffer->dataValid == OMX_TRUE) {
            OMX_U32 width = 0, height = 0;
            int imageSize = 0;
            int ret = 0;
            DecoderOut_t pOutput;
            VPU_FRAME pframe;
            memset(&pOutput, 0, sizeof(DecoderOut_t));
            memset(&pframe, 0, sizeof(VPU_FRAME));
            pOutput.data = (unsigned char *)&pframe;
            ret =  p_vpu_ctx->decode_getframe(p_vpu_ctx, &pOutput);
            if (ret < 0) {
                if (ret == VPU_API_EOS_STREAM_REACHED && !pframe.ErrorInfo) {
                    outputUseBuffer->dataLen = 0;
                    outputUseBuffer->remainDataLen = 0;
                    outputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                    outputUseBuffer->timeStamp = 0;
                    outputUseBuffer->dataValid = OMX_FALSE;
                    ret = OMX_TRUE;
                    pVideoDec->bDecSendEOS = OMX_TRUE;
                    mpp_err("OMX_BUFFERFLAG_EOS");
                } else {
                    mpp_err("OMX_DECODER ERROR");
                    pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                 pFpComponent->callbackData, OMX_EventError,
                                                                 OUTPUT_PORT_INDEX,
                                                                 OMX_IndexParamPortDefinition, NULL);
                }
                FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            }

            /*
             *when decode frame (width > 8192 || Height > 4096), mpp not to check it.
             *do not check here, ACodec will alloc large 4K size memory.
             *cause lower memory fault
            */
            if (pframe.DisplayWidth > 8192 ||  pframe.DisplayHeight > 4096) {
                pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pFpComponent->callbackData,
                                                             OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                if (pframe.vpumem.phy_addr > 0) {
                    VPUMemLink(&pframe.vpumem);
                    VPUFreeLinear(&pframe.vpumem);
                }
                ret = OMX_FALSE;
                goto EXIT;
            }


            if ((pOutput.size > 0) && (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
                if (pInputPort->portDefinition.format.video.nFrameWidth != pframe.DisplayWidth ||
                    pInputPort->portDefinition.format.video.nFrameHeight != pframe.DisplayHeight) {

                    pOutputPort->newCropRectangle.nWidth = pframe.DisplayWidth;
                    pOutputPort->newCropRectangle.nHeight = pframe.DisplayHeight;
                    pOutputPort->newPortDefinition.nBufferCountActual = pOutputPort->portDefinition.nBufferCountActual;
                    pOutputPort->newPortDefinition.nBufferCountMin = pOutputPort->portDefinition.nBufferCountMin;
                    pInputPort->newPortDefinition.format.video.nFrameWidth = pframe.DisplayWidth;
                    pInputPort->newPortDefinition.format.video.nFrameHeight = pframe.DisplayHeight;
                    pInputPort->newPortDefinition.format.video.nStride         = pframe.DisplayWidth;
                    pInputPort->newPortDefinition.format.video.nSliceHeight    = pframe.DisplayHeight;

                    FP_ResolutionUpdate(pOMXComponent);
                    pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                 pFpComponent->callbackData, OMX_EventPortSettingsChanged,
                                                                 OUTPUT_PORT_INDEX,
                                                                 OMX_IndexParamPortDefinition, NULL);
                    if (pframe.vpumem.phy_addr > 0) {
                        VPUMemLink(&pframe.vpumem);
                        VPUFreeLinear(&pframe.vpumem);
                    }
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_INFO_CHANGE, NULL);
                    goto EXIT;

                }

                if (!pframe.vpumem.phy_addr) { /*in mpp process may be notify a null frame for info change*/
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_SET_INFO_CHANGE, NULL);
                    goto EXIT;
                }

                FP_Frame2Outbuf(pOMXComponent, outputUseBuffer->bufferHeader, &pframe);
                outputUseBuffer->remainDataLen = pframe.DisplayHeight * pframe.DisplayWidth * 3 / 2;
                outputUseBuffer->timeStamp = pOutput.timeUs;
                if (VPU_API_EOS_STREAM_REACHED == (VPU_API_ERR)pOutput.nFlags) {
                    outputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                    pVideoDec->bDecSendEOS = OMX_TRUE;
                    mpp_err("OMX_BUFFERFLAG_EOS");
                }
                if ((outputUseBuffer->remainDataLen > 0) ||
                    ((outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) ||
                    (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
                    mpp_log("FP_OutputBufferReturn");
                    FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                }
                ret = OMX_TRUE;
            } else if (CHECK_PORT_BEING_FLUSHED(pOutputPort)) {
                if (pOutput.size) {
                    VPUMemLink(&pframe.vpumem);
                    VPUFreeLinear(&pframe.vpumem);
                }
                outputUseBuffer->dataLen = 0;
                outputUseBuffer->remainDataLen = 0;
                outputUseBuffer->nFlags = 0;
                outputUseBuffer->timeStamp = 0;
                ret = OMX_TRUE;
                FP_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            } else {
                //mpp_err("output buffer is smaller than decoded data size Out Length");
                //pFpComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                //                                        pFpComponent->callbackData,
                //                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                ret = OMX_FALSE;
            }
        } else {
            ret = OMX_FALSE;
        }
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
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *srcInputUseBuffer = &fpInputPort->way.port2WayDataBuffer.inputDataBuffer;
    OMX_BOOL               bCheckInputData = OMX_FALSE;
    OMX_BOOL               bValidCodecData = OMX_FALSE;

    FunctionIn();

    while (!pVideoDec->bExitBufferProcessThread) {
        usleep(0);  // release cpu-circle
        FP_Wait_ProcessPause(pFpComponent, INPUT_PORT_INDEX);
        mpp_log("FP_Check_BufferProcess_State in");
        while ((FP_Check_BufferProcess_State(pFpComponent, INPUT_PORT_INDEX)) &&
               (!pVideoDec->bExitBufferProcessThread)) {

            mpp_log("FP_OMX_InputBufferProcess in");

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

                    if (pVideoDec->fp_in != NULL) {
                        fwrite(srcInputUseBuffer->bufferHeader->pBuffer + srcInputUseBuffer->usedDataLen, 1, srcInputUseBuffer->dataLen, pVideoDec->fp_in);
                        fflush(pVideoDec->fp_in);
                    }
                }

                if (srcInputUseBuffer->dataValid == OMX_TRUE) {
                    if (FP_SendInputData((OMX_COMPONENTTYPE *)hComponent) != OMX_TRUE) {
                        mpp_log("stream list is full");
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
                pVideoDec->bExitBufferProcessThread = OMX_TRUE;
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
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT      *fpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    FP_OMX_DATABUFFER    *dstOutputUseBuffer = &fpOutputPort->way.port2WayDataBuffer.outputDataBuffer;

    FunctionIn();

    while (!pVideoDec->bExitBufferProcessThread) {
        usleep(0);
        FP_Wait_ProcessPause(pFpComponent, OUTPUT_PORT_INDEX);

        while ((FP_Check_BufferProcess_State(pFpComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoDec->bExitBufferProcessThread)) {

            if (CHECK_PORT_BEING_FLUSHED(fpOutputPort))
                break;

            MUTEX_LOCK(dstOutputUseBuffer->bufferMutex);
            if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                (!CHECK_PORT_BEING_FLUSHED(fpOutputPort))) {

                mpp_log("FP_OutputBufferGetQueue");
                ret = FP_OutputBufferGetQueue(pFpComponent);
                if (ret != OMX_ErrorNone) {
                    MUTEX_UNLOCK(dstOutputUseBuffer->bufferMutex);
                    break;
                }
            }

            if (dstOutputUseBuffer->dataValid == OMX_TRUE) {
                if (FP_Post_OutputFrame(pOMXComponent) != OMX_TRUE) {
                    usleep(3 * 1000);
                }
            }

            /* reset outputData */
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
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE    *pOMXComponent = NULL;
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

OMX_ERRORTYPE FP_OMX_BufferProcess_Create(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE               ret         = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec    = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

    FunctionIn();

    pVideoDec->bExitBufferProcessThread = OMX_FALSE;

    pVideoDec->hOutputThread = new MppThread((MppThreadFunc)FP_OMX_OutputProcessThread, pOMXComponent);

    pVideoDec->hInputThread  = new MppThread((MppThreadFunc)FP_OMX_InputProcessThread, pOMXComponent); 

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_OMX_BufferProcess_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT *pFpComponent = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    OMX_S32                countValue = 0;
    unsigned int           i = 0;

    FunctionIn();

    pVideoDec->bExitBufferProcessThread = OMX_TRUE;

    OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].bufferSemID);

    OSAL_SignalSet(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].pauseEvent);

    delete (MppThread *)pVideoDec->hInputThread;
    pVideoDec->hInputThread = NULL;

    OSAL_Get_SemaphoreCount(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        OSAL_SemaphorePost(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].bufferSemID);


    OSAL_SignalSet(pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX].pauseEvent);

    OSAL_SignalSet(pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX].pauseEvent);
    
    delete (MppThread *)(pVideoDec->hOutputThread);
    pVideoDec->hOutputThread = NULL;

    pFpComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pFpComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE omx_open_vpudec_context(FP_OMX_VIDEODEC_COMPONENT *pVideoDec)
{
    pVideoDec->rkapi_hdl = dlopen("libvpu.so", RTLD_LAZY);
    pVideoDec->bOld_api = OMX_FALSE;
    if (pVideoDec->rkapi_hdl == NULL) {
        return OMX_ErrorHardware;
    }
    pVideoDec->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoDec->rkapi_hdl, "vpu_open_context");
    if (pVideoDec->rkvpu_open_cxt == NULL) {
        dlclose(pVideoDec->rkapi_hdl);
        pVideoDec->rkapi_hdl = NULL;
        mpp_log("used old version lib");
        pVideoDec->rkapi_hdl = dlopen("librk_vpuapi.so", RTLD_LAZY);
        if (pVideoDec->rkapi_hdl == NULL) {
            mpp_err("dll open fail librk_vpuapi.so");
            return OMX_ErrorHardware;
        }
        pVideoDec->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoDec->rkapi_hdl, "vpu_open_context");

        if (pVideoDec->rkvpu_open_cxt == NULL) {
            mpp_err("dlsym vpu_open_context fail");
            dlclose( pVideoDec->rkapi_hdl);
            return OMX_ErrorHardware;
        }
        pVideoDec->bOld_api = OMX_TRUE;
    }
    pVideoDec->rkvpu_close_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoDec->rkapi_hdl, "vpu_close_context");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FP_Dec_ComponentInit(OMX_COMPONENTTYPE *pOMXComponent)
{

    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec    =  (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    OMX_RK_VIDEO_CODINGTYPE codecId = OMX_RK_VIDEO_CodingUnused;
    FP_OMX_BASEPORT           *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    VpuCodecContext_t *p_vpu_ctx = mpp_malloc(VpuCodecContext_t, 1);
    if (pFpComponent->rkversion != NULL) {
        mpp_err("omx decoder info : %s", pFpComponent->rkversion);
    }
    if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
        mpp_log("drm player mode is true, force to mpp");
        property_set("use_mpp_mode", "1");
    }
    memset((void*)p_vpu_ctx, 0, sizeof(VpuCodecContext_t));
    if (omx_open_vpudec_context(pVideoDec)) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    {
        int32_t kNumMapEntries = sizeof(kCodeMap) / sizeof(kCodeMap[0]);
        int i = 0;
        for (i = 0; i < kNumMapEntries; i++) {
            if (kCodeMap[i].omx_id == pVideoDec->codecId) {
                codecId = kCodeMap[i].codec_id;
                break;
            }
        }
    }

    if (pVideoDec->bOld_api == OMX_FALSE) {
        p_vpu_ctx->width = pFpInputPort->portDefinition.format.video.nFrameWidth;
        p_vpu_ctx->height = pFpInputPort->portDefinition.format.video.nFrameHeight;
        p_vpu_ctx->codecType = CODEC_DECODER;

        p_vpu_ctx->videoCoding = codecId;
    } else {
        mpp_free(p_vpu_ctx);
        p_vpu_ctx = NULL;
    }

    if ( pVideoDec->rkvpu_open_cxt != NULL) {
        pVideoDec->rkvpu_open_cxt(&p_vpu_ctx);
    }

    if (p_vpu_ctx == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    p_vpu_ctx->enableparsing = 1;
    p_vpu_ctx->extradata_size = 0;
    p_vpu_ctx->extradata = NULL;
    p_vpu_ctx->width = pFpInputPort->portDefinition.format.video.nFrameWidth;
    p_vpu_ctx->height = pFpInputPort->portDefinition.format.video.nFrameHeight;
    p_vpu_ctx->codecType = CODEC_DECODER;


    p_vpu_ctx->videoCoding = codecId;
    pVideoDec->vpu_ctx = p_vpu_ctx;

    pVideoDec->bFirstFrame = OMX_TRUE;
    pVideoDec->maxCount = 0;
    pVideoDec->bInfoChange = OMX_FALSE;

    if (rga_dev_open(&pVideoDec->rga_ctx)  < 0) {
        mpp_err("open rga device fail!");
    }

    if (pVideoDec->bDRMPlayerMode == OMX_FALSE) {
        if (FP_OMX_CheckIsNeedFastmode(pFpComponent) != OMX_ErrorNone) {
            mpp_err("check fast mode failed!");
        }
    }
    /*
     ** if current stream is Div3, tell VPU_API of on2 decoder to
     ** config hardware as Div3.
    */
    /*  if (pVideoDec->flags & FP_OMX_VDEC_IS_DIV3) {
          p_vpu_ctx->videoCoding = OMX_RK_VIDEO_CodingDIVX3;
      }*/
    if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
        pVideoDec->bIsHevc = 1;
    }
    if (p_vpu_ctx->width > 1920 && p_vpu_ctx->height > 1088) {
        OSAL_PowerControl(pFpComponent, 3840, 2160, pVideoDec->bIsHevc,
                                   pFpInputPort->portDefinition.format.video.xFramerate,
                                   OMX_TRUE,
                                   8);
        pVideoDec->bIsPowerControl = OMX_TRUE;
    }

    FP_OMX_DebugSwitchfromPropget(pFpComponent);

    if (p_vpu_ctx->width > 1920 && p_vpu_ctx->height > 1080) {
        //add for kodi
        property_set("sys.gpu.frames_num_of_sectionKD", "4");
        property_set("sys.gpu.frames_num_to_skip_KD", "3");
        pVideoDec->b4K_flags = OMX_TRUE;
    }

#ifdef WRITR_FILE
    pVideoDec->fp_out = fopen("data/video/dec_out.yuv", "wb");
#endif

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE FP_Dec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    FP_OMX_BASECOMPONENT      *pFpComponent  = (FP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    FP_OMX_VIDEODEC_COMPONENT    *pVideoDec         = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;
    FP_OMX_BASEPORT           *pFpInputPort  = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    FP_OMX_BASEPORT           *pFpOutputPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];

    int i, plane;
    FunctionIn();
    if (pVideoDec && pVideoDec->vpu_ctx) {
        if (pVideoDec->rkvpu_close_cxt) {
            pVideoDec->rkvpu_close_cxt(&pVideoDec->vpu_ctx);
        }
        pVideoDec->vpu_ctx = NULL;
        if (pVideoDec->rkapi_hdl) {
            dlclose(pVideoDec->rkapi_hdl);
            pVideoDec->rkapi_hdl = NULL;
        }
    }

    if (pVideoDec->rga_ctx != NULL) {
        rga_dev_close(pVideoDec->rga_ctx);
        pVideoDec->rga_ctx = NULL;
    }
#if 1
    OSAL_Closevpumempool(pFpComponent);
#endif
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
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;

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

    pVideoDec = mpp_malloc(FP_OMX_VIDEODEC_COMPONENT, 1);
    if (pVideoDec == NULL) {
        FP_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    memset(pVideoDec, 0, sizeof(FP_OMX_VIDEODEC_COMPONENT));

#ifdef USE_ION
    pVideoDec->hSharedMemory = OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        mpp_err("OSAL_SharedMemory_Open open fail");
    }
#endif

    pFpComponent->componentName = (OMX_STRING)malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pFpComponent->componentName == NULL) {
        FP_OMX_ComponentDeInit(hComponent);
        ret = OMX_ErrorInsufficientResources;
        mpp_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    memset(pFpComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);
    pVideoDec->bReconfigDPB = OMX_FALSE;
    pFpComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoDec;

    pFpComponent->bSaveFlagEOS = OMX_FALSE;
    pFpComponent->nRkFlags = 0;
    pFpComponent->bBehaviorEOS = OMX_FALSE;
    pVideoDec->bDecSendEOS = OMX_FALSE;
    pVideoDec->bPvr_Flag = OMX_FALSE;
    pVideoDec->bFastMode = OMX_FALSE;
    pVideoDec->bPrintFps = OMX_FALSE;
    pVideoDec->bPrintBufferPosition = OMX_FALSE;
    pVideoDec->bGtsMediaTest = OMX_FALSE;
    pVideoDec->bGtsExoTest = OMX_FALSE;
    pVideoDec->fp_in = NULL;
    pVideoDec->b4K_flags = OMX_FALSE;
    pVideoDec->power_fd = -1;
    pVideoDec->bIsPowerControl = OMX_FALSE;
    pVideoDec->bIsHevc = 0;
    pVideoDec->bIs10bit = OMX_FALSE;
    pFpComponent->bMultiThreadProcess = OMX_TRUE;
    pFpComponent->codecType = HW_VIDEO_DEC_CODEC;

    pVideoDec->bFirstFrame = OMX_TRUE;

    pVideoDec->vpumem_handle = NULL;

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
    /* Input port */

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    pFoilplanetPort->portDefinition.nBufferCountActual = MAX_VIDEO_INPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferCountMin = MAX_VIDEO_INPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferSize = 0;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pFoilplanetPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pFoilplanetPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pFoilplanetPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pFoilplanetPort->portDefinition.format.video.nSliceHeight = 0;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pFoilplanetPort->portDefinition.format.video.cMIMEType =  mpp_malloc(char, MAX_OMX_MIMETYPE_SIZE);
    memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    pFoilplanetPort->portDefinition.format.video.pNativeRender = 0;
    pFoilplanetPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFoilplanetPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pFoilplanetPort->portDefinition.bEnabled = OMX_TRUE;
    pFoilplanetPort->portWayType = WAY2_PORT;

    /* Output port */
    pFoilplanetPort = &pFpComponent->pFoilplanetPort[OUTPUT_PORT_INDEX];
    pFoilplanetPort->portDefinition.nBufferCountActual = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferCountMin = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pFoilplanetPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pFoilplanetPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pFoilplanetPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pFoilplanetPort->portDefinition.format.video.nSliceHeight = 0;
    pFoilplanetPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pFoilplanetPort->portDefinition.format.video.cMIMEType = mpp_malloc(char, MAX_OMX_MIMETYPE_SIZE);
    strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "raw/video");
    pFoilplanetPort->portDefinition.format.video.pNativeRender = 0;
    pFoilplanetPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pFoilplanetPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pFoilplanetPort->portDefinition.bEnabled = OMX_TRUE;
    pFoilplanetPort->portWayType = WAY2_PORT;
    pFoilplanetPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pFoilplanetPort->bufferProcessType = (FP_OMX_BUFFERPROCESS_TYPE)(BUFFER_COPY | BUFFER_ANBSHARE);

    pFoilplanetPort->processData.extInfo = mpp_malloc(DECODE_CODEC_EXTRA_BUFFERINFO, 1);
    memset(((char *)pFoilplanetPort->processData.extInfo), 0, sizeof(DECODE_CODEC_EXTRA_BUFFERINFO));
    {
        int i = 0;
        DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo = NULL;
        pBufferInfo = (DECODE_CODEC_EXTRA_BUFFERINFO *)(pFoilplanetPort->processData.extInfo);
    }
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

    pFpComponent->fp_codec_componentInit      = &FP_Dec_ComponentInit;
    pFpComponent->fp_codec_componentTerminate = &FP_Dec_Terminate;

    pFpComponent->fp_AllocateTunnelBuffer   = &FP_OMX_AllocateTunnelBuffer;
    pFpComponent->fp_FreeTunnelBuffer       = &FP_OMX_FreeTunnelBuffer;
    pFpComponent->fp_BufferProcessCreate    = &FP_OMX_BufferProcess_Create;
    pFpComponent->fp_BufferProcessTerminate = &FP_OMX_BufferProcess_Terminate;
    pFpComponent->fp_BufferFlush            = &FP_OMX_BufferFlush;

    pFoilplanetPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    if (!strcmp(componentName, FP_OMX_COMPONENT_H264_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/avc");
        pVideoDec->codecId = OMX_VIDEO_CodingAVC;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    } else if (!strcmp(componentName, FP_OMX_COMPONENT_H264_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor h264 secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/avc");
        pVideoDec->codecId = OMX_VIDEO_CodingAVC;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    } else if (!strcmp(componentName, FP_OMX_COMPONENT_MPEG4_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/mp4v-es");
        pVideoDec->codecId = OMX_VIDEO_CodingMPEG4;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat =  OMX_VIDEO_CodingMPEG4;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_MPEG4_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor mpeg4 secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/mp4v-es");
        pVideoDec->codecId = OMX_VIDEO_CodingMPEG4;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat =  OMX_VIDEO_CodingMPEG4;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_H263_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/3gpp");
        pVideoDec->codecId = OMX_VIDEO_CodingH263;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_FLV_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/flv");
        pVideoDec->codecId = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_MPEG2_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/mpeg2");
        pVideoDec->codecId = OMX_VIDEO_CodingMPEG2;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_MPEG2_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor mpeg2 secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/mpeg2");
        pVideoDec->codecId = OMX_VIDEO_CodingMPEG2;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_RMVB_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/vnd.rn-realvideo");
        pVideoDec->codecId = OMX_VIDEO_CodingRV;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat =  OMX_VIDEO_CodingRV;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VP8_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp8");
        pVideoDec->codecId = OMX_VIDEO_CodingVP8;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = pVideoDec->codecId = OMX_VIDEO_CodingVP8;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VC1_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/vc1");
        pVideoDec->codecId = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_WMV3_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-ms-wmv");
        pVideoDec->codecId = OMX_VIDEO_CodingWMV;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VP6_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/vp6");
        pVideoDec->codecId = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_HEVC_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor hevc.secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/hevc");
        pVideoDec->codecId = OMX_VIDEO_CodingHEVC;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
#ifndef LOW_VRESION
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
#else
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_OLD_CodingHEVC;
#endif
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_HEVC_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/hevc");
        pVideoDec->codecId = OMX_VIDEO_CodingHEVC;
#ifndef LOW_VRESION
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
#else
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_OLD_CodingHEVC;
#endif
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_MJPEG_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/mjpeg");
        pVideoDec->codecId = OMX_VIDEO_CodingMJPEG;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VP9_DEC)) {
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp9");
        pVideoDec->codecId = OMX_VIDEO_CodingVP9;
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VP9_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor VP9 secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp9");
        pVideoDec->codecId = OMX_VIDEO_CodingVP9;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
    } else if (!strcmp(componentName, FP_OMX_COMPONENT_VP8_DRM_DEC)) {
        mpp_err("FP_OMX_ComponentConstructor VP8 secure");
        memset(pFoilplanetPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        strcpy(pFoilplanetPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp8");
        pVideoDec->codecId = OMX_VIDEO_CodingVP8;
#ifdef HAVE_L1_SVP_MODE
        pVideoDec->bDRMPlayerMode = OMX_TRUE;
#endif
        pFoilplanetPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
    } else {
        // IL client specified an invalid component name
        mpp_err("VPU Component Invalid Component Name\n");
        ret =  OMX_ErrorInvalidComponentName;
        goto EXIT;
    }
    {
        int gpu_fd = -1;
        gpu_fd = open("/dev/pvrsrvkm", O_RDWR, 0);
        if (gpu_fd > 0) {
            pVideoDec->bPvr_Flag = OMX_TRUE;
            close(gpu_fd);
        }
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
    FP_OMX_BASEPORT      *pInputPort = NULL;
    FP_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
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

    pVideoDec = (FP_OMX_VIDEODEC_COMPONENT *)pFpComponent->hComponentHandle;

#ifdef USE_ION
    if (pVideoDec->hSharedMemory != NULL) {
        OSAL_SharedMemory_Close(pVideoDec->hSharedMemory, pVideoDec->bDRMPlayerMode);
        pVideoDec->hSharedMemory = NULL;
    }
#endif

//    OSAL_RefANB_Terminate(pVideoDec->hRefHandle);
    if (pVideoDec->fp_in != NULL) {
        fclose(pVideoDec->fp_in);
    }
    if (pVideoDec->b4K_flags == OMX_TRUE) {
        //add for kodi
        property_set("sys.gpu.frames_num_of_sectionKD", "0");
        property_set("sys.gpu.frames_num_to_skip_KD", "0");
        pVideoDec->b4K_flags = OMX_FALSE;
    }
    pInputPort = &pFpComponent->pFoilplanetPort[INPUT_PORT_INDEX];
    if (pVideoDec->bIsPowerControl == OMX_TRUE) {
        if (pVideoDec->bIs10bit) {
            OSAL_PowerControl(pFpComponent, 3840, 2160, pVideoDec->bIsHevc,
                                       pInputPort->portDefinition.format.video.xFramerate,
                                       OMX_FALSE,
                                       10);
        } else {
            OSAL_PowerControl(pFpComponent, 3840, 2160, pVideoDec->bIsHevc,
                                       pInputPort->portDefinition.format.video.xFramerate,
                                       OMX_FALSE,
                                       8);
        }
        pVideoDec->bIsPowerControl = OMX_FALSE;
    }

    if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
        mpp_log("drm player mode is true, force to mpp");
        property_set("use_mpp_mode", "0");
    }

    mpp_free(pVideoDec);
    pFpComponent->hComponentHandle = pVideoDec = NULL;

    if (pFpComponent->componentName != NULL) {
        mpp_free(pFpComponent->componentName);
        pFpComponent->componentName = NULL;
    }

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
