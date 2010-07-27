/*
 * gsttidmaivideosink.h:
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *     derived from fakesink
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


#ifndef __GST_TIDMAIVIDEOSINK_H__
#define __GST_TIDMAIVIDEOSINK_H__

#include <gst/gst.h>
#include <string.h>
#include <ctype.h>

//#include <gst/base/gstbasesink.h>
#include <gst/video/gstvideosink.h>
#include <ti/sdo/dmai/Ccv.h>
#include <ti/sdo/dmai/Display.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Framecopy.h>
#include <ti/sdo/dmai/Resize.h>

#include "gsttidmaibuffertransport.h"

G_BEGIN_DECLS

//CEM Do we need to define this if we are using DMAI?
#define V4L2 "v4l2"
#define FBDEV "fbdev"

#define CLEAN 0
#define DIRTY 1

#define GST_TYPE_TIDMAIVIDEOSINK \
  (gst_tidmaivideosink_get_type())
#define GST_TIDMAIVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIDMAIVIDEOSINK,GstTIDmaiVideoSink))
#define GST_TIDMAIVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIDMAIVIDEOSINK,GstTIDmaiVideoSinkClass))
#define GST_IS_TIDMAIVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIDMAIVIDEOSINK))
#define GST_IS_TIDMAIVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIDMAIVIDEOSINK))
#define GST_TIDMAIVIDEOSINK_CAST(obj) ((GstTIDmaiVideoSink *)obj)

typedef struct _VideoStd_Attrs VideoStd_Attrs;
struct _VideoStd_Attrs {
  VideoStd_Type     videostd;
  Int32             width;
  Int32             height;
  int               framerate;
};

typedef struct _GstTIDmaiVideoSink GstTIDmaiVideoSink;
typedef struct _GstTIDmaiVideoSinkClass GstTIDmaiVideoSinkClass;

/**
 * GstDmaiVideoSink:
 *
 * The opaque #GstDmaiVideoSink data structure.
 */
struct _GstTIDmaiVideoSink {
  GstVideoSink      videosink;

  /* User input video attributes */
  gchar         *displayStd;
  gchar         *displayDevice;
  gchar         *videoStd;
  gchar         *videoOutput;
  gint          framerate;
  gint          numBufs;
  gint          rotation;
  gint          xPosition;
  gint          yPosition;
  gint          xCentering;
  gint          yCentering;
  gboolean      resizer;
  gboolean      autoselect;
  gint          numDispBuf; 
  gchar         *cleanBufCtrl;

  Display_Handle    hDisplay;
  Display_Attrs     dAttrs;
  Framecopy_Handle  hFc;
  Cpu_Device        cpu_dev;
  Buffer_Handle     tempDmaiBuf;
  gint              numBuffers;
  GstBuffer         **allocatedBuffers;
  Buffer_Handle     *unusedBuffers;
  GstBuffer         *lastAllocatedBuffer;
  GstBuffer         *prerolledBuffer;
  gint              numAllocatedBuffers;
  gint              numUnusedBuffers;
  gboolean          dmaiElementUpstream;
  gboolean          zeromemcpy;

  gboolean      capsAreSet;
  gint          width;
  gint          height;
  ColorSpace_Type colorSpace;
  /* prevVideoStd is used as part of the autoselect functionality.  If the
   * selected videoStd is not supported by the device then we look for
   * the next videoStd starting from the previous one.
   */
  int           prevVideoStd;

  /* iattrs are the video attributes of the input.
   * oattrs are the video attributes of the display/output.
   */
  VideoStd_Attrs    iattrs;
  VideoStd_Attrs    oattrs;

  /* Hardware accelerated copy */
  gboolean      accelFrameCopy;
};

struct _GstTIDmaiVideoSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_tidmaivideosink_get_type (void);

G_END_DECLS

#endif /* __GST_TIDMAIVIDEOSINK_H__ */

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
