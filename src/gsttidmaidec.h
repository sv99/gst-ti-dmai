/*
 * gsttidmaidec.h
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 * Contributor:
 *     Diego Dompe, RidgeRun
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

#ifndef __GST_TIDMAIDEC_H__
#define __GST_TIDMAIDEC_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstticommonutils.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>

G_BEGIN_DECLS

#define GST_TIDMAIDEC_PARAMS_QDATA g_quark_from_static_string("dmaidec-params")

/* Constants */
#define gst_tidmaidec_CODEC_FREE 0x2

typedef struct _GstTIDmaidec      GstTIDmaidec;
typedef struct _GstTIDmaidecData  GstTIDmaidecData;
typedef struct _GstTIDmaidecClass GstTIDmaidecClass;
typedef struct _MetaBufTab MetaBufTab;

#include "gsttiparsers.h"

/* structure to store the buffer information */
struct _MetaBufTab
{
    GstBuffer buffer;
    gboolean is_dummy;
};

/* _GstTIDmaidec object */
struct _GstTIDmaidec
{
    /* gStreamer infrastructure */
    GstElement          element;
    GstPad              *sinkpad;
    GstPad              *srcpad;

    /* Element properties */
    const gchar*        engineName;
    const gchar*        codecName;

    /* Element state */
    Engine_Handle    	hEngine;
    gpointer         	hCodec;
    gpointer            *params;
    gpointer            *dynParams;

    /* Output thread */
    GList               *outList;

    /* Blocking Conditions to Throttle I/O */
    pthread_mutex_t     bufTabMutex;
    pthread_cond_t      bufTabCond;
    gint16              outputUseMask;

    /* Video Information */
    gint                framerateNum;
    gint                framerateDen;
    GstClockTime        frameDuration;
    gint                height;
    gint                width;
    gint                pitch;
    gint                par_d;
    gint                par_n;
    gint                allocatedHeight;
    gint                allocatedWidth;
    ColorSpace_Type     colorSpace;

    /* Audio Information */
    gint                channels;
    gint                rate;
    gint                depth;

    /* Event information */
    gint64              segment_start;
    gint64              segment_stop;
    GstClockTime        current_timestamp;
    GstClockTime        sample_duration;
    gboolean            qos;
    gint                qos_value;
    gint                skip_frames, skip_done; /* QOS skip to next I Frame */

    /* Buffer management */
    Buffer_Handle       circBuf;
#ifdef GLIB_2_31_AND_UP
    GMutex              circMutex;
    GMutex              circMetaMutex;
#else
    GMutex              *circMutex;
    GMutex              *circMetaMutex;
#endif

    GList               *circMeta;

    gint                head;
    gint                tail;
    gint                marker;
    gint                end;
    UInt32              numInputBufs;
    UInt32              numOutputBufs;
    BufTab_Handle       hOutBufTab;
    gint                outBufSize;
    gint                inBufSize;
    MetaBufTab          *metaBufTab;
#ifdef GLIB_2_31_AND_UP
    GMutex              metaTabMutex;
#else
    GMutex              *metaTabMutex;
#endif
    GstBuffer           *allocated_buffer;
    gboolean            downstreamBuffers;
    gint                downstreamWidth;
    gboolean            require_configure;
    gboolean            src_pad_caps_fixed;

    /* Parser structures */
    void                *parser_private;
    gboolean            parser_started;

    /* Flags */
    gboolean            flushing;
    gboolean            generate_timestamps;

    /* Private Data */
    void                *stream_private;
};

/* _GstTIDmaidecClass object */
struct _GstTIDmaidecClass
{
    GstElementClass         parent_class;
    GstPadTemplate   *srcTemplateCaps, *sinkTemplateCaps;
    /* Custom Codec Data */
    struct codec_custom_data    *codec_data;
};

/* Decoder operations */
struct gstti_decoder_ops {
    const gchar             *xdmversion;
    enum dmai_codec_type    codec_type;
    /* Functions to provide custom properties */
    void                    (*install_properties)(GObjectClass *);
    void                    (*set_property)
                                (GObject *,guint,const GValue *,GParamSpec *);
    void                    (*get_property)(GObject *,guint,GValue *, GParamSpec *);
    /* Functions to manipulate codecs */
    gboolean                (* default_setup_params)(GstTIDmaidec *);
    void                    (* set_codec_caps)(GstTIDmaidec *);
    gboolean                (* codec_create) (GstTIDmaidec *);
    void                    (* set_outBufTab) (GstTIDmaidec *,BufTab_Handle);
    void                    (* codec_destroy) (GstTIDmaidec *);
    gboolean                (* codec_process)
                                (GstTIDmaidec *, GstBuffer *,
                                 Buffer_Handle, gboolean /* flushing */);
    Buffer_Handle           (* codec_get_data) (GstTIDmaidec *);
    /* Advanced functions for video decoders */
    void                    (* codec_flush) (GstTIDmaidec *);
    Buffer_Handle           (* codec_get_free_buffers)(GstTIDmaidec *);
    /* Get the minimal input buffer sizes */
    gint                    (* get_in_buffer_size)(GstTIDmaidec *);
    gint                    (* get_out_buffer_size)(GstTIDmaidec *);
    gint16                  outputUseMask;
};

struct gstti_stream_decoder_ops {
    /*
     * (optional) It copies buffers into the circular input buffer, and
     * may be used to interleave data (like on h264) 
     */
    int             (* custom_memcpy)(GstTIDmaidec *, void *, int, GstBuffer *);
    /* Functions to provide custom properties */
    void (*setup)(GstTIDmaidec *dmaidec);
    void (*install_properties)(GObjectClass *);
    void (*set_property)
             (GObject *,guint,const GValue *,GParamSpec *);
    void (*get_property)(GObject *,guint,GValue *, GParamSpec *);
};

/* Data definition for each instance of decoder */
struct _GstTIDmaidecData
{
    const gchar                 *streamtype;
    GstStaticCaps               *srcCaps, *sinkCaps;
    const gchar                 *engineName;
    const gchar                 *codecName;
    struct gstti_decoder_ops    *dops;
    struct gstti_parser_ops     *parser;
    struct gstti_stream_decoder_ops *stream_ops;
};

/* Function to initialize the decoders */
gboolean register_dmai_decoder(GstPlugin *plugin, GstTIDmaidecData *decoder);

G_END_DECLS

#endif /* __GST_TIDMAIDEC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
