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

#ifndef _FOILPLANET_OMX_CORE_INC_
#define _FOILPLANET_OMX_CORE_INC_

static const omx_core_cb_type dec_core[] = {
    {
        "OMX.fp.video_decoder.avc",
        "video_decoder.avc"
    },

    {
        "OMX.fp.video_decoder.m4v",
        "video_decoder.mpeg4"
    },

    {
        "OMX.fp.video_decoder.h263",
        "video_decoder.h263"
    },

    {
        "OMX.fp.video_decoder.flv1",
        "video_decoder.flv1"
    },

    {
        "OMX.fp.video_decoder.m2v",
        "video_decoder.mpeg2"
    },
#ifndef AVS80
    {
        "OMX.fp.video_decoder.rv",
        "video_decoder.rv"
    },
#endif

#ifdef SUPPORT_VP6
    {
        "OMX.fp.video_decoder.vp6",
        "video_decoder.vp6"
    },
#endif

    {
        "OMX.fp.video_decoder.vp8",
        "video_decoder.vp8"
    },
#ifdef SUPPORT_VP9
    {
        "OMX.fp.video_decoder.vp9",
        "video_decoder.vp9"
    },
#endif

    {
        "OMX.fp.video_decoder.vc1",
        "video_decoder.vc1"
    },

    {
        "OMX.fp.video_decoder.wmv3",
        "video_decoder.wmv3"
    },
#ifdef SUPPORT_HEVC
    {
        "OMX.fp.video_decoder.hevc",
        "video_decoder.hevc"
    },
#endif
    {
        "OMX.fp.video_decoder.mjpeg",
        "video_decoder.mjpeg"
    },
#ifdef HAVE_L1_SVP_MODE
    {
        "OMX.fp.video_decoder.avc.secure",
        "video_decoder.avc"
    },

    {
        "OMX.fp.video_decoder.hevc.secure",
        "video_decoder.hevc"
    },

    {
        "OMX.fp.video_decoder.m2v.secure",
        "video_decoder.mpeg2"
    },

    {
        "OMX.fp.video_decoder.m4v.secure",
        "video_decoder.mpeg4"
    },

    {
        "OMX.fp.video_decoder.vp8.secure",
        "video_decoder.vp8"
    },

    {
        "OMX.fp.video_decoder.vp9.secure",
        "video_decoder.vp9"
    },
#endif
};

const unsigned int SIZE_OF_DEC_CORE = sizeof(dec_core) / sizeof(dec_core[0]);

#endif  // _FOILPLANET_OMX_CORE_INC_
