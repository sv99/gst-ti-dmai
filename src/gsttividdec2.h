/*
 * gsttividdec2.h
 *
 * This file declares the "TIViddec2" element, which decodes an xDM 1.2 video
 * stream.
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

#ifndef __GST_TIVIDDEC2_H__
#define __GST_TIVIDDEC2_H__

#include <pthread.h>

#include <gst/gst.h>
#include "gsttiparsers.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Vdec2.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIViddec2 objects */
#define GST_TYPE_TIVIDDEC2 \
  (gst_tividdec2_get_type())
#define GST_TIVIDDEC2(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDDEC2,GstTIViddec2))
#define GST_TIVIDDEC2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDDEC2,GstTIViddec2Class))
#define GST_IS_TIVIDDEC2(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDDEC2))
#define GST_IS_TIVIDDEC2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDDEC2))

typedef struct _GstTIViddec2      GstTIViddec2;
typedef struct _GstTIViddec2Class GstTIViddec2Class;

/* _GstTIViddec2 object */
struct _GstTIViddec2
{
  /* gStreamer infrastructure */
  GstElement     element;
  GstPad        *sinkpad;
  GstPad        *srcpad;
  GstCaps       *outCaps;

  /* Element properties */
  const gchar*   engineName;
  const gchar*   codecName;
  gboolean       displayBuffer;
  gboolean       genTimeStamps;

  /* Element state */
  Engine_Handle    	hEngine;
  Vdec2_Handle     	hVd;
  gboolean         	eos;
  gboolean         	flushing;
  gboolean		   	paused;
  pthread_mutex_t  	threadStatusMutex;
  UInt32           	threadStatus;

  /* Decode thread */
  pthread_t          decodeThread;
  Fifo_Handle        hInFifo;
  struct parser_ops  *parser;
  void				 *codec_private;
  Rendezvous_Handle	 waitOnDecodeThread;

  /* Blocking Conditions to Throttle I/O */
  Rendezvous_Handle  waitOnFifoFlush;
  Rendezvous_Handle  waitOnInBufTab;
  Rendezvous_Handle	 waitOnOutBufTab;

  /* Framerate (Num/Den) */
  gint               framerateNum;
  gint               framerateDen;
  gint               height;
  gint               width;

  /* Buffer management */
  UInt32           numInputBufs;
  BufTab_Handle    hInBufTab;
  UInt32           numOutputBufs;
  BufTab_Handle    hOutBufTab;

  /* Quicktime h264 header  */
  struct h264_parser_private h264_data;
};

/* _GstTIViddec2Class object */
struct _GstTIViddec2Class
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tividdec2_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDDEC2_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
