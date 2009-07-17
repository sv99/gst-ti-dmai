/*
 * gsttividresize.h
 *
 * This file enclares the "TIVidresize" element, which resizes the video stream
 * using hardware resizer module.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __GST_TIVIDRESIZE_H__
#define __GST_TIVIDRESIZE_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Resize.h>



G_BEGIN_DECLS

/* Standard macros for maniuplating TIVidresize objects */
#define GST_TYPE_TIVIDRESIZE \
  (gst_tividresize_get_type())
#define GST_TIVIDRESIZE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDRESIZE,GstTIVidresize))
#define GST_TIVIDRESIZE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDRESIZE,GstTIVidresizeClass))
#define GST_IS_TIVIDRESIZE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDRESIZE))
#define GST_IS_TIVIDRESIZE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDRESIZE))

typedef struct _GstTIVidresize      GstTIVidresize;
typedef struct _GstTIVidresizeClass GstTIVidresizeClass;

/* _GstTIVidresize object */
struct _GstTIVidresize
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* Element property */
  gboolean          contiguousInputFrame;

  /* Element state */
  gint              srcWidth;
  gint              srcHeight;
  gint              dstWidth;
  gint              dstHeight;
  Resize_Handle     hResize;
  gint              colorSpace;
  gint              outBufSize;

  /* Buffer Managment */
  Buffer_Handle     hRszInBuf;
  Buffer_Handle     hRszOutBuf;
  BufTab_Handle     hOutBufTab;
  
};

/* _GstTIVidresizeClass object */
struct _GstTIVidresizeClass 
{
  GstBaseTransformClass parent_class;
};

/* External function enclarations */
GType gst_tividresize_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDRESIZE_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
