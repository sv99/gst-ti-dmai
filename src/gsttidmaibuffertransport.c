/*
 * gsttidmaibuffertransport.c
 *
 * This file defines the "TIDmaiBufferTransport" buffer object, which is used
 * to encapsulate an existing DMAI buffer object inside of a gStreamer
 * buffer so it can be passed along the gStreamer pipeline.
 *
 * Specifically, this object provides a finalize function that will release
 * a DMAI buffer properly when gst_buffer_unref() is called.  If the specified
 * DMAI buffer is part of a BufTab, it will be released for re-use.
 * DMAI buffers no part of a BufTab will be deleted when no longer referenced.
 *
 * Downstream elements may use the GST_IS_TIDMAIBUFFERTRANSPORT() macro to
 * check to see if a gStreamer buffer encapsulates a DMAI buffer.  When passed
 * an element of this type, elements can take advantage of the fact that the
 * buffer is contiguously allocated in memory.  Also, if the element is using
 * DMAI it can access the DMAI buffer directly via the
 * GST_TIDMAIBUFFERTRANSPORT_DMAIBUF() macro.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
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

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>

#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaibuffertransport_debug);
#define GST_CAT_DEFAULT gst_tidmaibuffertransport_debug

/* Declare a global pointer to our buffer base class */
static GstBufferClass *parent_class;

/* Static Function Declarations */
static void gst_tidmaibuffertransport_init(GTypeInstance *instance,
                gpointer g_class);
static void gst_tidmaibuffertransport_class_init(gpointer g_class,
                gpointer class_data);
static void gst_tidmaibuffertransport_finalize(GstTIDmaiBufferTransport *nbuf);


/******************************************************************************
 * gst_tidmaibuffertransport_get_type
 *    Defines function pointers for initialization routines for this buffer.
 ******************************************************************************/
GType gst_tidmaibuffertransport_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstBufferClass),
            NULL,
            NULL,
            gst_tidmaibuffertransport_class_init,
            NULL,
            NULL,
            sizeof(GstTIDmaiBufferTransport),
            0,
            gst_tidmaibuffertransport_init,
            NULL
        };

        object_type =
            g_type_register_static(GST_TYPE_BUFFER, "GstTIDmaiBufferTransport",
                &object_info, (GTypeFlags) 0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tidmaibuffertransport_debug,
                                "TIDmaiBufferTransport", 0,
                                "TI DMAI Buffer Transport");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
}


/******************************************************************************
 * gst_tidmaibuffertransport_class_init
 *    Initializes the GstTIDmaiBufferTransport class.
 ******************************************************************************/
static void gst_tidmaibuffertransport_class_init(gpointer g_class,
                gpointer class_data)
{
    GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS(g_class);

    GST_LOG("begin class_init\n");
    parent_class = (GstBufferClass *) g_type_class_peek_parent(g_class);

    mo_class->finalize =
        (GstMiniObjectFinalizeFunction) gst_tidmaibuffertransport_finalize;

    GST_LOG("end class_init\n");
}


/******************************************************************************
 * gst_tidmaibuffertransport_init
 *    Initializes a new transport buffer instance.
 ******************************************************************************/
static void gst_tidmaibuffertransport_init(GTypeInstance *instance,
                gpointer g_class)
{
    GstTIDmaiBufferTransport *buf = GST_TIDMAIBUFFERTRANSPORT(instance);

    buf->release_cb = NULL;
    buf->cb_data = NULL;
}


/******************************************************************************
 * gst_tidmaibuffertransport_finalize
 *    Release a DMAI buffer transport object
 ******************************************************************************/
static void gst_tidmaibuffertransport_finalize(GstTIDmaiBufferTransport *cbuf)
{
    GST_LOG("begin finalize\n");

    if (cbuf->release_cb){
        GST_LOG("Calling release function");
        cbuf->release_cb(cbuf->cb_data,cbuf);
    }

    /* If the DMAI buffer is part of a BufTab, free it for re-use.  Otherwise,
     * destroy the buffer.
     */
    if (Buffer_getBufTab(cbuf->dmaiBuffer) != NULL) {
        Buffer_freeUseMask(cbuf->dmaiBuffer,
            gst_tidmaibuffertransport_GST_FREE);

        GST_DEBUG("clearing useMask bit so buffer %p can be reused: new mask %dh\n",
            cbuf->dmaiBuffer,Buffer_getUseMask(cbuf->dmaiBuffer));
        /* If rendezvous handle is set then wake-up caller */
        if (cbuf->hRv) {
            if (Buffer_getUseMask(cbuf->dmaiBuffer) == 0) {
                Rendezvous_forceAndReset(cbuf->hRv);
            } else {
                GST_DEBUG("Not broadcasting buffer free at finalize, since use mask is not clean yet");
            }
        }
    } else {
        GST_LOG("calling Buffer_delete()\n");
        Buffer_delete(cbuf->dmaiBuffer);
    }

    GST_LOG("end finalize\n");
}


/******************************************************************************
 * gst_tidmaibuffertransport_new
 *    Create a new DMAI buffer transport object.
 *
 * Note: If rendenzvous handle is set then Rendenzvous_force will be called for
 *       this handle during finalize method.
 ******************************************************************************/
GstBuffer *gst_tidmaibuffertransport_new(Buffer_Handle hBuf,
    Rendezvous_Handle hRv)
{
    GstTIDmaiBufferTransport *buf;

    GST_LOG("begin new\n");

    buf = (GstTIDmaiBufferTransport*)gst_mini_object_new(
                                         GST_TYPE_TIDMAIBUFFERTRANSPORT);

    g_return_val_if_fail(buf != NULL, NULL);

    GST_BUFFER_SIZE(buf) = Buffer_getSize(hBuf);
    GST_BUFFER_DATA(buf) = (Void*)Buffer_getUserPtr(hBuf);

    if (GST_BUFFER_DATA(buf) == NULL) {
        gst_mini_object_unref(GST_MINI_OBJECT(buf));
        return NULL;
    }

    buf->dmaiBuffer = hBuf;
    buf->hRv = hRv;
    GST_LOG("end new\n");

    return GST_BUFFER(buf);
}

/******************************************************************************
 * gst_tidmaibuffertransport_set_release_callback
 *    Set a release callback function
 ******************************************************************************/

void gst_tidmaibuffertransport_set_release_callback
    (GstTIDmaiBufferTransport *buf,
	void (*release_cb)(gpointer,GstTIDmaiBufferTransport *buf),
	gpointer data){
    buf->release_cb = release_cb;
    buf->cb_data = data;
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
