/*
 * gstticodecplugin.c
 *
 * This file defines the main entry point of the DMAI Plugins for GStreamer.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 * Contributor:
 *      Diego Dompe, RidgeRun Engineering
 *      Cristina Murillo, RidgeRun
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

#include <sys/resource.h>
#include <stdlib.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/CERuntime.h>
#include <ti/sdo/dmai/Dmai.h>

#include "gstticommonutils.h"
#include "gsttidmaidec.h"
#include "gsttidmaienc.h"
#include "gsttidmaivideosink.h"
#include "gsttisupport_h264.h"
#include "gsttisupport_mpeg4.h"
#include "gsttisupport_aac.h"
#include "gsttisupport_mp3.h"
#include "gsttisupport_wma.h"
#include "gsttidmairesizer.h"
#include "gsttidmaiaccel.h"

/* Audio caps */
static GstStaticCaps gstti_pcm_caps = GST_STATIC_CAPS(
    "audio/x-raw-int, "
    "   width = (int) 16, "
    "   depth = (int) 16, "
    "   endianness = (int) BYTE_ORDER, "
    "   channels = (int) { 1, 8 }, "
    "   rate = (int) [ 8000, 96000 ]"
);

/* Video caps */
#if PLATFORM == dm6467
static GstStaticCaps gstti_uyvy_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* Y8C8 - YUV422 semi planar */
    "   format=(fourcc)Y8C8, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]"
);
#else
static GstStaticCaps gstti_uyvy_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* UYVY */
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);
#endif

extern struct gstti_decoder_ops gstti_viddec2_ops;
extern struct gstti_decoder_ops gstti_viddec0_ops;
extern struct gstti_encoder_ops gstti_videnc1_ops;
extern struct gstti_encoder_ops gstti_videnc0_ops;

extern struct gstti_encoder_ops gstti_audenc0_ops;
extern struct gstti_encoder_ops gstti_audenc1_ops;


#if PLATFORM == dm357
# ifndef DECODEENGINE
#  define DECODEENGINE "hmjcp"
# endif
# ifndef ENCODEENGINE
#  define ENCODEENGINE "hmjcp"
# endif
#else
# ifndef DECODEENGINE
#  define DECODEENGINE "decode"
# endif
# ifndef ENCODEENGINE
#  define ENCODEENGINE "encode"
# endif
#endif

/* Video decoders */

GstTIDmaidecData decoders[] = {
#ifdef ENABLE_H264DEC_XDM2
    {
        .streamtype = "h264",
        .sinkCaps = &gstti_h264_caps,
        .srcCaps = &gstti_uyvy_caps,
        .engineName = DECODEENGINE,
        .codecName = "h264dec",
        .dops = &gstti_viddec2_ops,
        .parser = &gstti_h264_parser,
    },
#elif defined(ENABLE_H264DEC_XDM0)
    {
        .streamtype = "h264",
        .sinkCaps = &gstt_h264_caps,
        .srcCaps = &gstti_uyvy_caps,
        .engineName = DECODEENGINE,
        .codecName = "h264dec",
        .dops = &gstti_viddec0_ops,
        .parser = &gstti_h264_parser,
    },
#endif
#ifdef ENABLE_MPEG4DEC_XDM2
    {
        .streamtype = "mpeg4",
        .sinkCaps = &gstti_mpeg4_sink_caps,
        .srcCaps = &gstti_uyvy_caps,
        .engineName = DECODEENGINE,
        .codecName = "mpeg4dec",
        .dops = &gstti_viddec2_ops,
        .parser = &gstti_mpeg4_parser,
    },
#elif defined(ENABLE_MPEG4DEC_XDM0)
    {
        .streamtype = "mpeg4",
        .sinkCaps = &gstti_mpeg4_sink_caps,
        .srcCaps = &gstti_uyvy_caps,
        .engineName = DECODEENGINE,
        .codecName = "mpeg4dec",
        .dops = &gstti_viddec0_ops,
        .parser = &gstti_mpeg4_parser,
    },
#endif

/* Audio decoders */

/*
#ifdef ENABLE_AACDEC_XDM1
    {
        .streamtype = "aac",
        .sinkCaps = &gstti_aac_caps,
        .srcCaps = &gstti_pcm_caps,
        .engineName = DECODEENGINE,
        .codecName = "aachedec",
        .dops = &gstti_auddec1_ops,
        .parser = &gstti_aac_parser,
    },
#elif defined(ENABLE_AAC4DEC_XDM0)
    {
        .streamtype = "aac",
        .sinkCaps = &gstti_aac_caps,
        .srcCaps = &gstti_pcm_caps,
        .engineName = DECODEENGINE,
        .codecName = "aachedec",
        .dops = &gstti_auddec0_ops,
        .parser = &gstti_aac_parser,
    },
#endif
#endif*/

    { .streamtype = NULL },

    /* Dummy entry to avoid build errors when no element
       is enabled using the src or sink caps
    */
    { .streamtype = NULL,
      .srcCaps = &gstti_uyvy_caps,
      .sinkCaps = &gstti_uyvy_caps,
    },
};


/* Video encoders */

GstTIDmaiencData encoders[] = {
#ifdef ENABLE_H264ENC_XDM1
    {
        .streamtype = "h264",
        .sinkCaps = &gstti_uyvy_caps,
        .srcCaps = &gstti_h264_caps,
        .engineName = ENCODEENGINE,
        .codecName = "h264enc",
        .eops = &gstti_videnc1_ops,
    },
#elif defined(ENABLE_H264ENC_XDM0)
    {
        .streamtype = "h264",
        .sinkCaps = &gstti_uyvy_caps,
        .srcCaps = &gstti_h264_caps,
        .engineName = ENCODEENGINE,
        .codecName = "h264enc",
        .eops = &gstti_videnc0_ops,
    },
#endif
#ifdef ENABLE_MPEG4ENC_XDM1
    {
        .streamtype = "mpeg4",
        .sinkCaps = &gstti_uyvy_caps,
        .srcCaps = &gstti_mpeg4_src_caps,
        .engineName = ENCODEENGINE,
        .codecName = "mpeg4enc",
        .eops = &gstti_videnc1_ops,
    },
#elif defined(ENABLE_MPEG4ENC_XDM0)
    {
        .streamtype = "mpeg4",
        .sinkCaps = &gstti_uyvy_caps,
        .srcCaps = &gstti_mpeg4_src_caps,
        .engineName = ENCODEENGINE,
        .codecName = "mpeg4enc",
        .eops = &gstti_videnc0_ops,
    },
#endif

/* Audio encoders */

#ifdef ENABLE_AACENC_XDM1
    {
        .streamtype = "aac",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_aac_caps,
        .engineName = ENCODEENGINE,
        .codecName = "aacenc",
        .eops = &gstti_audenc1_ops,
    },
#elif defined(ENABLE_AACENC_XDM0)
    {
        .streamtype = "aac",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_aac_caps,
        .engineName = ENCODEENGINE,
        .codecName = "aacenc",
        .eops = &gstti_audenc0_ops,
    },
#endif
#ifdef ENABLE_WMAENC_XDM1
    {
        .streamtype = "wma",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_wma_caps,
        .engineName = ENCODEENGINE,
        .codecName = "wmaenc",
        .eops = &gstti_audenc1_ops,
    },
#elif defined(ENABLE_WMAENC_XDM0)
    {
        .streamtype = "wma",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_wma_caps,
        .engineName = ENCODEENGINE,
        .codecName = "wmaenc",
        .eops = &gstti_audenc0_ops,
    },
#endif
#ifdef ENABLE_MP3ENC_XDM1
    {
        .streamtype = "mp3",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_mp3_caps,
        .engineName = ENCODEENGINE,
        .codecName = "mp3enc",
        .eops = &gstti_audenc1_ops,
    },
#elif defined(ENABLE_MP3ENC_XDM0)
    {
        .streamtype = "mp3",
        .sinkCaps = &gstti_pcm_caps,
        .srcCaps = &gstti_mp3_caps,
        .engineName = ENCODEENGINE,
        .codecName = "mp3enc",
        .eops = &gstti_audenc0_ops,
    },
#endif
    { .streamtype = NULL },
    /* Dummy entry to avoid build errors when no element
       is enabled using the src or sink caps
    */
    { .streamtype = NULL,
      .srcCaps = &gstti_pcm_caps,
      .sinkCaps = &gstti_pcm_caps,
    },
};

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
TICodecPlugin_init (GstPlugin * TICodecPlugin)
{
    /* Initialize the codec engine run time */
    CERuntime_init();

    /* Initialize DMAI */
    Dmai_init();

    if (!register_dmai_decoders(TICodecPlugin,decoders)){
        g_warning("Failed to register one decoder, aborting");
        return FALSE;
    }

    if (!register_dmai_encoders(TICodecPlugin,encoders)){
        g_warning("Failed to register one encoder, aborting");
        return FALSE;
    }

    if (!gst_element_register(TICodecPlugin, "dmaiaccel",
        GST_RANK_PRIMARY,GST_TYPE_TIDMAIACCEL))
        return FALSE;

#ifdef ENABLE_VIDEOSINK
    if (!gst_element_register(TICodecPlugin, "TIDmaiVideoSink",
        GST_RANK_PRIMARY,GST_TYPE_TIDMAIVIDEOSINK))
        return FALSE;
#endif

#ifdef ENABLE_DMAI_RESIZER
    if (!gst_element_register(TICodecPlugin, "dmairesizer",
        GST_RANK_PRIMARY,GST_TYPE_DMAI_RESIZER))
        return FALSE;
#endif

    return TRUE;
}

/* gstreamer looks for this structure to register TICodecPlugins */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "TICodecPlugin",
    "Plugin for TI xDM-Based Codecs",
    TICodecPlugin_init,
    VERSION,
    "LGPL",
    "TI / RidgeRun",
    "http://www.ti.com/, http://www.ridgerun.com"
)


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
