/*
 * gsttidmaiaccel.c
 *
 * This file defines the "dmaiaccel" element, which converts gst buffers into
 * dmai transport buffers if possible, otherwise it just memcpy the data into
 * dmai transport buffers.
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2009 RidgeRun, http://www.ridgerun.com/
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
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/ce/osal/Memory.h>

#include "gsttidmaiaccel.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaiaccel_debug);
#define GST_CAT_DEFAULT gst_tidmaiaccel_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

/* Static Function Declarations */
static void
 gst_tidmaiaccel_base_init(gpointer g_class);
static void
 gst_tidmaiaccel_class_init(GstTIDmaiaccelClass *g_class);
static GstFlowReturn gst_tidmaiaccel_transform (GstBaseTransform *trans,
    GstBuffer *inBuf, GstBuffer *outBuf);
static gboolean gst_tidmaiaccel_get_unit_size (GstBaseTransform *trans,
    GstCaps *caps, guint *size);
static gboolean gst_tidmaiaccel_set_caps (GstBaseTransform *trans,
    GstCaps *in, GstCaps *out);
static GstFlowReturn gst_tidmaiaccel_prepare_output_buffer (GstBaseTransform
    *trans, GstBuffer *inBuf, gint size, GstCaps *caps, GstBuffer **outBuf);

/******************************************************************************
 * gst_tidmaiaccel_init
 *****************************************************************************/
static void gst_tidmaiaccel_init (GstTIDmaiaccel *dmaiaccel)
{
    gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (dmaiaccel), TRUE);
    dmaiaccel->disabled = FALSE;
    dmaiaccel->colorSpace = ColorSpace_NOTSET;
    dmaiaccel->width = 0;
    dmaiaccel->height = 0;
}


/******************************************************************************
 * gst_tidmaiaccel_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tidmaiaccel_get_type(void)
{
    static GType object_type = 0;

    GST_LOG("Begin\n");
    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIDmaiaccelClass),
            gst_tidmaiaccel_base_init,
            NULL,
            (GClassInitFunc)gst_tidmaiaccel_class_init,
            NULL,
            NULL,
            sizeof(GstTIDmaiaccel),
            0,
            (GInstanceInitFunc) gst_tidmaiaccel_init
        };

        object_type = g_type_register_static(GST_TYPE_BASE_TRANSFORM,
                          "GstTIDmaiaccel", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tidmaiaccel_debug, "TIDmaiaccel", 0,
            "TI Dmai buffer accelerator");

        GST_LOG("initialized get_type\n");

    }

    GST_LOG("Finish\n");
    return object_type;
};


/******************************************************************************
 * gst_tidmaiaccel_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tidmaiaccel_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI Dmai Buffer accelerator",
        "Misc",
        "If the input buffer is contigous on memory, it optimize it to avoid memory copies",
        "Diego Dompe; RidgeRun"
    };
    GST_LOG("Begin\n");

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
    GST_LOG("Finish\n");
}


/******************************************************************************
 * gst_tidmaiaccel_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIDmaiaccel class.
 ******************************************************************************/
static void gst_tidmaiaccel_class_init(GstTIDmaiaccelClass *klass)
{
    GstBaseTransformClass   *trans_class;

    trans_class      = (GstBaseTransformClass *) klass;

    GST_LOG("Begin\n");
    trans_class->transform = GST_DEBUG_FUNCPTR(gst_tidmaiaccel_transform);
    trans_class->set_caps  = GST_DEBUG_FUNCPTR(gst_tidmaiaccel_set_caps);
    trans_class->prepare_output_buffer =
        GST_DEBUG_FUNCPTR(gst_tidmaiaccel_prepare_output_buffer);
    trans_class->get_unit_size =
        GST_DEBUG_FUNCPTR(gst_tidmaiaccel_get_unit_size);

    GST_LOG("Finish\n");
}


/******************************************************************************
 * gst_tidmaiaccel_set_caps
 *****************************************************************************/
static gboolean gst_tidmaiaccel_set_caps (GstBaseTransform *trans,
    GstCaps *in, GstCaps *out)
{
    GstTIDmaiaccel *dmaiaccel = GST_TIDMAIACCEL(trans);
    GstStructure    *structure;
    guint32         fourcc;

    GST_LOG("begin set caps\n");

    structure = gst_caps_get_structure(in, 0);

    if (!gst_structure_get_int(structure, "width", &dmaiaccel->width)) {
        GST_ERROR("Failed to get width \n");
    }

    if (!gst_structure_get_int(structure, "height", &dmaiaccel->height)) {
        GST_ERROR("Failed to get height \n");
    }

    if (!gst_structure_get_fourcc(structure, "format", &fourcc)) {
        GST_ERROR("failed to get fourcc from cap\n");
    }

    switch (fourcc) {
    case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
        dmaiaccel->colorSpace = ColorSpace_UYVY;
        break;
    case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
        dmaiaccel->colorSpace = ColorSpace_YUV422PSEMI;
        break;
    }

    dmaiaccel->lineLength = BufferGfx_calcLineLength(dmaiaccel->width,
                                dmaiaccel->colorSpace);

    GST_LOG("end set caps\n");
    return TRUE;
}


/*****************************************************************************
 * gst_tidmaiaccel_prepare_output_buffer
 *    Function is used to allocate output buffer
 *****************************************************************************/
static GstFlowReturn gst_tidmaiaccel_prepare_output_buffer (GstBaseTransform
    *trans, GstBuffer *inBuf, gint size, GstCaps *caps, GstBuffer **outBuf)
{
    GstTIDmaiaccel *dmaiaccel = GST_TIDMAIACCEL(trans);
    Buffer_Handle   hOutBuf;
    Bool isContiguous = FALSE;

    if (!dmaiaccel->disabled){
        Memory_getBufferPhysicalAddress(
                    GST_BUFFER_DATA(inBuf),
                    GST_BUFFER_SIZE(inBuf),
                    &isContiguous);
    }

    if (isContiguous && dmaiaccel->width){
        GST_DEBUG("Is contiguous video buffer");

        /* This is a contiguous buffer, create a dmai buffer transport */
        BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;

        gfxAttrs.bAttrs.reference   = TRUE;
        gfxAttrs.dim.width          = dmaiaccel->width;
        gfxAttrs.dim.height         = dmaiaccel->height;
        gfxAttrs.colorSpace         = dmaiaccel->colorSpace;
        gfxAttrs.dim.lineLength     = dmaiaccel->lineLength;

        hOutBuf = Buffer_create(GST_BUFFER_SIZE(inBuf), &gfxAttrs.bAttrs);
        Buffer_setUserPtr(hOutBuf, (Int8*)GST_BUFFER_DATA(inBuf));
        Buffer_setNumBytesUsed(hOutBuf, GST_BUFFER_SIZE(inBuf));
        *outBuf = gst_tidmaibuffertransport_new(hOutBuf, NULL);
        gst_buffer_set_data(*outBuf, (guint8*) Buffer_getUserPtr(hOutBuf),
            Buffer_getSize(hOutBuf));
        gst_buffer_copy_metadata(*outBuf,inBuf,GST_BUFFER_COPY_ALL);
        gst_buffer_set_caps(*outBuf, GST_PAD_CAPS(trans->srcpad));
        return GST_FLOW_OK;
    } else {
        GST_DEBUG("Copying into contiguous video buffer");
        /* This is a contiguous buffer, create a dmai buffer transport */

        if (!dmaiaccel->disabled){
            /* Initialize our buffer tab */
            Rendezvous_Attrs  rzvAttrs  = Rendezvous_Attrs_DEFAULT;
            BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;

            gfxAttrs.dim.width          = dmaiaccel->width;
            gfxAttrs.dim.height         = dmaiaccel->height;
            gfxAttrs.colorSpace         = dmaiaccel->colorSpace;
            gfxAttrs.dim.lineLength     = dmaiaccel->lineLength;

            dmaiaccel->hOutBufTab =
                        BufTab_create(2, GST_BUFFER_SIZE(inBuf),
                            BufferGfx_getBufferAttrs(&gfxAttrs));
            dmaiaccel->waitOnOutBufTab = Rendezvous_create(2, &rzvAttrs);
            if (dmaiaccel->hOutBufTab == NULL) {
                GST_ELEMENT_ERROR(dmaiaccel,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to create output buffer tab"));
                return GST_FLOW_ERROR;
            }
            dmaiaccel->disabled = TRUE;
        }

        hOutBuf = BufTab_getFreeBuf(dmaiaccel->hOutBufTab);
        if (hOutBuf == NULL) {
            GST_INFO("Failed to get free buffer, waiting on bufTab\n");
            Rendezvous_meet(dmaiaccel->waitOnOutBufTab);

            hOutBuf = BufTab_getFreeBuf(dmaiaccel->hOutBufTab);

            if (hOutBuf == NULL) {
                GST_ELEMENT_ERROR(dmaiaccel,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to get a free contiguous buffer from BufTab"));
                return GST_FLOW_ERROR;
            }
        }

        memcpy(Buffer_getUserPtr(hOutBuf),GST_BUFFER_DATA(inBuf),
            GST_BUFFER_SIZE(inBuf));
        Buffer_setNumBytesUsed(hOutBuf, GST_BUFFER_SIZE(inBuf));
        *outBuf = gst_tidmaibuffertransport_new(hOutBuf, dmaiaccel->waitOnOutBufTab);
        gst_buffer_set_data(*outBuf, (guint8*) Buffer_getUserPtr(hOutBuf),
            Buffer_getSize(hOutBuf));
        gst_buffer_copy_metadata(*outBuf,inBuf,GST_BUFFER_COPY_ALL);
        gst_buffer_set_caps(*outBuf, GST_PAD_CAPS(trans->srcpad));

        return GST_FLOW_OK;
    }
}


/******************************************************************************
 * gst_tidmaiaccel_get_unit_size
 *   get the size in bytes of one unit for the given caps
 *****************************************************************************/
static gboolean gst_tidmaiaccel_get_unit_size (GstBaseTransform *trans,
    GstCaps *caps, guint *size)
{
    *size = 1;

    return TRUE;
}

/******************************************************************************
 * gst_tidmaiaccel_transform
 *    Transforms one incoming buffer to one outgoing buffer.
 *****************************************************************************/
static GstFlowReturn gst_tidmaiaccel_transform (GstBaseTransform *trans,
    GstBuffer *inBuf, GstBuffer *outBuf)
{
    /* Do nothing, all the magic is done on the prepare buffer */

    return GST_FLOW_OK;
}

#if 0
if (dmaidec->hOutBufTab) {
    GST_DEBUG("freeing output buffers\n");
    BufTab_delete(dmaidec->hOutBufTab);
    dmaidec->hOutBufTab = NULL;
}
#endif

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif