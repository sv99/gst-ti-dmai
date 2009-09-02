/*
 * gsttiaudenc.c
 *
 * This file provides the access to the codec APIs for xDM 0.9 Audio Codecs
 *
 * Contributors:
 *     Diego Dompe, RidgeRun
 *     Cristina Murillo, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2009 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Aenc.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"

/* Support for xDM 0.9 */
static gboolean gstti_audenc0_setup_params(GstTIDmaienc *);
static gboolean gstti_audenc0_create(GstTIDmaienc *);
static void gstti_audenc0_destroy(GstTIDmaienc *);
static gboolean gstti_audenc0_process(GstTIDmaienc *, Buffer_Handle,Buffer_Handle);

struct gstti_encoder_ops gstti_audenc0_ops = {
    .xdmversion = "xDM 0.9",
    .codec_type = AUDIO,
    .default_setup_params = gstti_audenc0_setup_params,
    .codec_create = gstti_audenc0_create,
    .codec_destroy = gstti_audenc0_destroy,
    .codec_process = gstti_audenc0_process,
};

/* Declare variable used to categorize GST_LOG output */
/* Debug variable for xDM 0.9 */
GST_DEBUG_CATEGORY_STATIC (gst_tiaudenc0_debug);
#define GST_CAT_DEFAULT gst_tiaudenc0_debug

/******************************************************************************
 * gst_tiaudenc1_setup_params Support for xDM1.0
 *     Setup default codec params
 *****************************************************************************/
static gboolean gstti_audenc0_setup_params(GstTIDmaienc *dmaienc){
    AUDENC_Params *params;
    AUDENC_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc0_debug, "TIAudenc1", 0,
        "DMAI Audio1 Encoder");

    if (!dmaienc->params){
        dmaienc->params = g_malloc(sizeof (AUDENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc(sizeof (AUDENC_DynamicParams));
    }
    *(AUDENC_Params *)dmaienc->params  = Aenc_Params_DEFAULT;
    *(AUDENC_DynamicParams *)dmaienc->dynParams  = Aenc_DynamicParams_DEFAULT;
    params = (AUDENC_Params *)dmaienc->params;
    dynParams = (AUDENC_DynamicParams *)dmaienc->dynParams;

    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    params->maxSampleRate = dynParams->sampleRate = dmaienc->rate;
    switch (dmaienc->channels){
        case (1):
            params->maxNoOfCh = IAUDIO_MONO;
            break;
        case (2):
            params->maxNoOfCh = IAUDIO_STEREO;
            break;
        default:
            GST_ELEMENT_ERROR(dmaienc,STREAM,FORMAT,(NULL),
                ("Unsupported number of channels: %d\n", dmaienc->channels));
            return FALSE;
    }
    dynParams->numChannels = params->maxNoOfCh;
    dynParams->inputBitsPerSample = dmaienc->awidth;
    params->maxBitrate = dynParams->bitRate = 128000;

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc0_create Support for xDM 0.9
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_audenc0_create (GstTIDmaienc *dmaienc)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc0_debug, "TIAudenc", 0,
        "DMAI Audio Encoder");

    GST_DEBUG("opening audio encoder \"%s\"\n", dmaienc->codecName);
    dmaienc->hCodec =
        Aenc_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    (AUDENC_Params *)dmaienc->params,
                    (AUDENC_DynamicParams *)dmaienc->dynParams);

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,ENCODE,(NULL),
            ("failed to create audio encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc0_destroy Support for xDM 0.9
 *     free codec resources
 *****************************************************************************/
static void gstti_audenc0_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Aenc_delete(dmaienc->hCodec);
}


/******************************************************************************
 * gst_tiaudenc0_process Support for xDM 0.9
 ******************************************************************************/
static gboolean gstti_audenc0_process(GstTIDmaienc *dmaienc, Buffer_Handle hSrcBuf,
                    Buffer_Handle hDstBuf){
    Int ret;

    /* Invoke the audio encoder */
    GST_DEBUG("invoking the audio encoder,(%p, %p)\n",
        Buffer_getUserPtr(hSrcBuf),Buffer_getUserPtr(hDstBuf));
    ret = Aenc_process(dmaienc->hCodec, hSrcBuf, hDstBuf);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,ENCODE,(NULL),
            ("failed to encode audio buffer"));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
