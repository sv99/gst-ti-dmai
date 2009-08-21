/*
 * gsttidmaienc.c
 *
 * This file defines the a generic encoder element based on DMAI
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Code Refactoring by:
 *     Diego Dompe, RidgeRun
 *
 * Contributor:
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

/*
 * TODO LIST
 *
 *  * Add codec_data handling
 *  * Add pad-alloc functionality
 *  * Reduce minimal input buffer requirements to 1 frame size and
 *    implement heuristics to break down the input tab into smaller chunks.
 *  * Allow custom properties for the class.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/xdais/dm/xdm.h>
#include <ti/xdais/dm/ivideo.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaienc_debug);
#define GST_CAT_DEFAULT gst_tidmaienc_debug

/* Element property identifiers */
enum
{
    PROP_0,
    PROP_ENGINE_NAME,     /* engineName     (string)  */
    PROP_CODEC_NAME,      /* codecName      (string)  */
    PROP_SIZE_OUTPUT_BUF, /* sizeOutputBuf  (int)     */
};

#define GST_TIDMAIENC_PARAMS_QDATA g_quark_from_static_string("dmaienc-params")

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tidmaienc_base_init(gpointer gclass);
static void
 gst_tidmaienc_class_init(GstTIDmaiencClass *g_class);
static void
 gst_tidmaienc_init(GstTIDmaienc *object, GstTIDmaiencClass *g_class);
static void
 gst_tidmaienc_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void
 gst_tidmaienc_get_property (GObject *object, guint prop_id, GValue *value,
    GParamSpec *pspec);
static gboolean
 gst_tidmaienc_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tidmaienc_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tidmaienc_chain(GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn
 gst_tidmaienc_change_state(GstElement *element, GstStateChange transition);
static gboolean
 gst_tidmaienc_init_encoder(GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_exit_encoder(GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_configure_codec (GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_deconfigure_codec (GstTIDmaienc *dmaienc);
static GstFlowReturn
 encode(GstTIDmaienc *dmaienc,GstBuffer * buf);

/*
 * Register all the required encoders
 * Receives a NULL terminated array of encoder instances.
 */
gboolean register_dmai_encoders(GstPlugin * plugin, GstTIDmaiencData *encoder){
    GTypeInfo typeinfo = {
           sizeof(GstTIDmaiencClass),
           (GBaseInitFunc)gst_tidmaienc_base_init,
           NULL,
           (GClassInitFunc)gst_tidmaienc_class_init,
           NULL,
           NULL,
           sizeof(GstTIDmaienc),
           0,
           (GInstanceInitFunc) gst_tidmaienc_init
       };
    GType type;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tidmaienc_debug, "TIDmaienc", 0,
        "DMAI VISA Encoder");

    while (encoder->streamtype != NULL) {
        gchar *type_name;

        type_name = g_strdup_printf ("dmaienc_%s", encoder->streamtype);

        /* Check if it exists */
        if (g_type_from_name (type_name)) {
            g_free (type_name);
            g_warning("Not creating type %s, since it exists already",type_name);
            goto next;
        }

        type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, 0);
        g_type_set_qdata (type, GST_TIDMAIENC_PARAMS_QDATA, (gpointer) encoder);

        if (!gst_element_register(plugin, type_name, GST_RANK_PRIMARY,type)) {
              g_warning ("Failed to register %s", type_name);
              g_free (type_name);
              return FALSE;
            }
        g_free(type_name);

next:
        encoder++;
    }

    GST_DEBUG("DMAI encoders registered\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaienc_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tidmaienc_base_init(gpointer gclass)
{
    GstTIDmaiencData *encoder;
    static GstElementDetails details;
    gchar *codec_type, *codec_name;

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    encoder = (GstTIDmaiencData *)
     g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);
    g_assert (encoder != NULL);
    g_assert (encoder->streamtype != NULL);
    g_assert (encoder->srcTemplateCaps != NULL);
    g_assert (encoder->sinkTemplateCaps != NULL);
    g_assert (encoder->eops != NULL);
    g_assert (encoder->eops->codec_type != 0);

    switch (encoder->eops->codec_type){
    case VIDEO:
        codec_type = g_strdup("Video");
        break;
    case AUDIO:
        codec_type = g_strdup("Audio");
        break;
    case IMAGE:
        codec_type = g_strdup("Image");
        break;
    default:
        g_warning("Unkown encoder codec type");
        return;
    }

    codec_name = g_ascii_strup(encoder->streamtype,strlen(encoder->streamtype));
    details.longname = g_strdup_printf ("DMAI %s %s Encoder",
                            encoder->eops->xdmversion,
                            codec_name);
    details.klass = g_strdup_printf ("Codec/Encoder/%s",codec_type);
    details.description = g_strdup_printf ("DMAI %s encoder",codec_name);
      details.author = "Don Darling; Texas Instruments, Inc., "
                       "Diego Dompe; RidgeRun Engineering ";

    g_free(codec_type);
    g_free(codec_name);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (encoder->srcTemplateCaps));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (encoder->sinkTemplateCaps));
    gst_element_class_set_details(element_class, &details);

}

/******************************************************************************
 * gst_tidmaienc_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIDmaienc class.
 ******************************************************************************/
static void gst_tidmaienc_class_init(GstTIDmaiencClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;
    GstTIDmaiencData *encoder;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIENC_PARAMS_QDATA);
    g_assert (encoder != NULL);
    g_assert (encoder->codecName != NULL);
    g_assert (encoder->engineName != NULL);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->set_property = gst_tidmaienc_set_property;
    gobject_class->get_property = gst_tidmaienc_get_property;

    gstelement_class->change_state = gst_tidmaienc_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", encoder->engineName,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of codec",
            encoder->codecName, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SIZE_OUTPUT_BUF,
        g_param_spec_int("sizeOutputBuf",
            "Size of Ouput Buffer",
            "Size of the output buffer to allocate for encoded data",
            0, G_MAXINT32, 0, G_PARAM_WRITABLE));
}

/******************************************************************************
 * gst_tidmaienc_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tidmaienc_init(GstTIDmaienc *dmaienc, GstTIDmaiencClass *gclass)
{
    GstTIDmaiencData *encoder;

    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    /* Instantiate raw sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaienc->sinkpad =
        gst_pad_new_from_static_template(encoder->sinkTemplateCaps, "sink");
    gst_pad_set_setcaps_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_set_sink_caps));
    gst_pad_set_event_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_sink_event));
    gst_pad_set_chain_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_chain));
    gst_pad_fixate_caps(dmaienc->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->sinkpad))));

    /* Instantiate encoded source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaienc->srcpad =
        gst_pad_new_from_static_template(encoder->srcTemplateCaps, "src");
    gst_pad_fixate_caps(dmaienc->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad))));

    /* Add pads to TIDmaienc element */
    gst_element_add_pad(GST_ELEMENT(dmaienc), dmaienc->sinkpad);
    gst_element_add_pad(GST_ELEMENT(dmaienc), dmaienc->srcpad);

    /* Initialize TIDmaienc state */
    dmaienc->engineName         = g_strdup(encoder->engineName);
    dmaienc->codecName          = g_strdup(encoder->codecName);

    dmaienc->hEngine            = NULL;
    dmaienc->hCodec             = NULL;

    dmaienc->adapter            = NULL;

    dmaienc->head               = 0;
    dmaienc->headWrap           = 0;
    dmaienc->tail               = 0;

    dmaienc->outBuf             = NULL;
    dmaienc->inBuf              = NULL;

    dmaienc->require_configure = TRUE;

    /* Initialize TIDmaienc video state */

    dmaienc->framerateNum       = 0;
    dmaienc->framerateDen       = 0;
    dmaienc->height	        = 0;
    dmaienc->width	        = 0;

    /*Initialize TIDmaienc audio state */

    dmaienc->channels           = 0;
    dmaienc->depth              = 0;
    dmaienc->awidth             = 0;
    dmaienc->rate               = 0;
}


/******************************************************************************
 * gst_tidmaienc_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tidmaienc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;

    GST_LOG("begin set_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        if (dmaienc->engineName) {
            g_free((gpointer)dmaienc->engineName);
        }
        dmaienc->engineName = g_strdup(g_value_get_string(value));
        GST_LOG("setting \"engineName\" to \"%s\"\n", dmaienc->engineName);
        break;
    case PROP_CODEC_NAME:
        if (dmaienc->codecName) {
            g_free((gpointer)dmaienc->codecName);
        }
        dmaienc->codecName =  g_strdup(g_value_get_string(value));
        GST_LOG("setting \"codecName\" to \"%s\"\n", dmaienc->codecName);
        break;
    case PROP_SIZE_OUTPUT_BUF:
        dmaienc->outBufSize = g_value_get_int(value);
        GST_LOG("setting \"outBufSize\" to \"%d\"\n",
            dmaienc->outBufSize);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tidmaienc_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tidmaienc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;

    GST_LOG("begin get_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        g_value_set_string(value, dmaienc->engineName);
        break;
    case PROP_CODEC_NAME:
        g_value_set_string(value, dmaienc->codecName);
        break;
    case PROP_SIZE_OUTPUT_BUF:
        g_value_set_int(value,dmaienc->outBufSize);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tidmaienc_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tidmaienc_change_state(GstElement *element,
    GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIDmaienc          *dmaienc = (GstTIDmaienc *)element;

    GST_DEBUG("begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
    default:
        break;
    }

    /* Pass state changes to base class */
    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    /* Handle ramp-down state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init encoder */
        GST_DEBUG("GST_STATE_CHANGE_NULL_TO_READY");
        if (!gst_tidmaienc_init_encoder(dmaienc)) {
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");
        /* Shut down encoder */
        if (!gst_tidmaienc_exit_encoder(dmaienc)) {
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    GST_DEBUG("end change_state\n");
    return ret;
}


/******************************************************************************
 * gst_tidmaienc_init_encoder
 *     Initialize or re-initializes the stream
 ******************************************************************************/
static gboolean gst_tidmaienc_init_encoder(GstTIDmaienc *dmaienc)
{
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
        g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("begin init_encoder\n");

    /* Make sure we know what codec we're using */
    if (!dmaienc->engineName) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Engine name not specified"));
        return FALSE;
    }

    if (!dmaienc->codecName) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Codec name not specified"));
        return FALSE;
    }

    /* Open the codec engine */
    GST_DEBUG("opening codec engine \"%s\"\n", dmaienc->engineName);
    dmaienc->hEngine = Engine_open((Char *) dmaienc->engineName, NULL, NULL);

    if (dmaienc->hEngine == NULL) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("failed to open codec engine \"%s\"", dmaienc->engineName));
        return FALSE;
    }

    dmaienc->adapter = gst_adapter_new();
    if (!dmaienc->adapter){
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create adapter"));
        gst_tidmaienc_exit_encoder(dmaienc);
        return FALSE;
    }

    /* Initialize conditional for making threads wait on conditions */
    pthread_cond_init(&dmaienc->waitOnOutBuf,NULL);
    pthread_mutex_init(&dmaienc->outBufMutex,NULL);

    /* Status variables */
    dmaienc->head = 0;
    dmaienc->tail = 0;

    GST_DEBUG("end init_encoder\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_exit_encoder
 *    Shut down any running video encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tidmaienc_exit_encoder(GstTIDmaienc *dmaienc)
{
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("begin exit_encoder\n");

    /* Release the codec */
    gst_tidmaienc_deconfigure_codec(dmaienc);

    if (dmaienc->adapter){
        gst_adapter_clear(dmaienc->adapter);
        gst_object_unref(dmaienc->adapter);
        dmaienc->adapter = NULL;
    }

    if (dmaienc->hEngine) {
        GST_DEBUG("closing codec engine\n");
        Engine_close(dmaienc->hEngine);
        dmaienc->hEngine = NULL;
    }

    GST_DEBUG("end exit_encoder\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaienc_configure_codec
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tidmaienc_configure_codec (GstTIDmaienc  *dmaienc)
{
    Buffer_Attrs           Attrs     = Buffer_Attrs_DEFAULT;
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("Init\n");

    Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    if (dmaienc->outBufSize == 0) {
        dmaienc->outBufSize = dmaienc->inBufSize * 3;
    }
    dmaienc->headWrap = dmaienc->outBufSize;

    /* Create codec output buffers */
    if (encoder->eops->codec_type == VIDEO) {
        GST_DEBUG("creating output buffer \n");

        dmaienc->outBuf = Buffer_create(dmaienc->outBufSize, &Attrs);
    } else {
//TODO
        dmaienc->outBufSize = 0; // Audio case?
    }

    if (dmaienc->outBuf == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }
    GST_DEBUG("Output buffer handler: %p\n",dmaienc->outBuf);

    /* Initialize the rest of the codec */
    return encoder->eops->codec_create(dmaienc);
}



/******************************************************************************
 * gst_tidmaienc_deconfigure_codec
 *     free codec engine resources
 *****************************************************************************/
static gboolean gst_tidmaienc_deconfigure_codec (GstTIDmaienc  *dmaienc)
{
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    dmaienc->require_configure = TRUE;

    if (dmaienc->hCodec) {
        GST_LOG("closing video encoder\n");
        encoder->eops->codec_destroy(dmaienc);
        dmaienc->hCodec = NULL;
    }

    /* Wait for free all downstream buffers */
    pthread_mutex_lock(&dmaienc->outBufMutex);
    while (dmaienc->head != dmaienc->tail){
        GST_LOG("Waiting for downstream buffers to be freed\n");
        pthread_cond_wait(&dmaienc->waitOnOutBuf,&dmaienc->outBufMutex);
    }
    pthread_mutex_unlock(&dmaienc->outBufMutex);

    if (dmaienc->outBuf) {
        GST_DEBUG("freeing output buffer, %p\n",dmaienc->outBuf);
        Buffer_delete(dmaienc->outBuf);
        dmaienc->outBuf = NULL;
    }

    if (dmaienc->inBuf){
        GST_DEBUG("freeing input buffer, %p\n",dmaienc->inBuf);
        Buffer_delete(dmaienc->inBuf);
        dmaienc->inBuf = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tidmaienc_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIDmaienc *dmaienc;
    GstStructure *capStruct;
    const gchar  *mime;
    char * str = NULL;
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    dmaienc =(GstTIDmaienc *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
            &framerateDen)) {
            dmaienc->framerateNum = framerateNum;
            dmaienc->framerateDen = framerateDen;
        }

        if (!gst_structure_get_int(capStruct, "height", &dmaienc->height)) {
            dmaienc->height = 0;
        }

        if (!gst_structure_get_int(capStruct, "width", &dmaienc->width)) {
            dmaienc->width = 0;
        }

        caps = gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad)));
        capStruct = gst_caps_get_structure(caps, 0);
        gst_structure_set(capStruct,"height",G_TYPE_INT,dmaienc->height,
                                    "width",G_TYPE_INT,dmaienc->width,
                                    "framerate", GST_TYPE_FRACTION,
                                        dmaienc->framerateNum,dmaienc->framerateDen,
                                    (char *)NULL);
#if PLATFORM == dm6467
        dmaienc->colorSpace = ColorSpace_YUV422PSEMI;
#else
        dmaienc->colorSpace = ColorSpace_UYVY;
#endif

        dmaienc->inBufSize = BufferGfx_calcLineLength(dmaienc->width,
            dmaienc->colorSpace) * dmaienc->height;

    /* Generic Audio Properties */
    } else if(!strncmp(mime, "audio/", 6)){

        if (!gst_structure_get_int(capStruct, "channels", &dmaienc->channels)){
            dmaienc->channels = 0;
        }

        if (!gst_structure_get_int(capStruct, "width", &dmaienc->awidth)){
            dmaienc->awidth = 0;
        }

        if (!gst_structure_get_int(capStruct, "depth", &dmaienc->depth)){
            dmaienc->depth = 0;
        }

        if (!gst_structure_get_int(capStruct, "rate", &dmaienc->rate)){
            dmaienc->rate = 0;
        }

        caps = gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad)));

        /* gst_pad_get_pad_template_caps: gets the capabilities of 
         * dmaienc->srcpad, then creates a copy and makes it writable 
         */
        capStruct = gst_caps_get_structure(caps, 0);

        gst_structure_set(capStruct,"channels",G_TYPE_INT,dmaienc->channels,
                                    "rate",G_TYPE_INT,dmaienc->rate,
                                    (char *)NULL);

        dmaienc->inBufSize = 0;
    }

    else { //Add support for images

    }

    GST_DEBUG("Setting source caps: '%s'", (str = gst_caps_to_string(caps)));
    g_free(str);
    gst_pad_set_caps(dmaienc->srcpad, caps);
    gst_caps_unref(caps);

    gst_tidmaienc_deconfigure_codec(dmaienc);

    gst_object_unref(dmaienc);

    GST_DEBUG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_sink_event
 *     Perform event processing.
 ******************************************************************************/
static gboolean gst_tidmaienc_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIDmaienc *dmaienc;
    gboolean      ret = FALSE;
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    dmaienc =(GstTIDmaienc *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_FLUSH_START:
        /* Flush the adapter */
        gst_adapter_clear(dmaienc->adapter);
        ret = TRUE;
        goto done;
    default:
        ret = gst_pad_push_event(dmaienc->srcpad, event);
    }

done:
    gst_object_unref(dmaienc);
    return ret;
}

void release_cb(gpointer data, GstTIDmaiBufferTransport *buf){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)data;

    GST_LOG("Release pointers: %p, %p, %p, %d",
        buf,
        Buffer_getUserPtr(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)),
        Buffer_getUserPtr(dmaienc->outBuf),
        dmaienc->tail);

    if (Buffer_getUserPtr(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)) !=
        Buffer_getUserPtr(dmaienc->outBuf) + dmaienc->tail){
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("unexpected behavior freeing buffer that is not on the tail"));
        return;
    }

    dmaienc->tail += GST_BUFFER_SIZE(buf);
    if (dmaienc->tail >= dmaienc->headWrap){
        dmaienc->headWrap = dmaienc->outBufSize;
        dmaienc->tail = 0;
    }
}

gint outSpace(GstTIDmaienc *dmaienc){
    if (dmaienc->head == dmaienc->tail){
        return dmaienc->outBufSize - dmaienc->head;
    } else if (dmaienc->head > dmaienc->tail){
        gint size = dmaienc->outBufSize - dmaienc->head;
        if (dmaienc->inBufSize > size){
            GST_LOG("Wrapping the head");
            dmaienc->headWrap = dmaienc->head;
            dmaienc->head = 0;
            size = dmaienc->tail - dmaienc->head;
        }
        return size;
    } else {
        return dmaienc->tail - dmaienc->head;
    }
}

Buffer_Handle encode_buffer_get_free(GstTIDmaienc *dmaienc){
    Buffer_Attrs  Attrs  = Buffer_Attrs_DEFAULT;
    Buffer_Handle hBuf;

    Attrs.reference = TRUE;
    /* Wait until enough data has been processed downstream
     * This is an heuristic
     */
    pthread_mutex_lock(&dmaienc->outBufMutex);
    while (outSpace(dmaienc) < dmaienc->inBufSize){
        GST_LOG("Failed to get free buffer, waiting for space\n");
        pthread_cond_wait(&dmaienc->waitOnOutBuf,&dmaienc->outBufMutex);
    }
    pthread_mutex_unlock(&dmaienc->outBufMutex);

    hBuf = Buffer_create(dmaienc->inBufSize,&Attrs);
    GST_LOG("Outbuf at %p, head at %d",Buffer_getUserPtr(dmaienc->outBuf),dmaienc->head);
    Buffer_setUserPtr(hBuf,Buffer_getUserPtr(dmaienc->outBuf) + dmaienc->head);
    Buffer_setNumBytesUsed(hBuf,dmaienc->inBufSize);

    return hBuf;
}

/* Return a dmai buffer from the passed gstreamer buffer */
Buffer_Handle get_raw_buffer(GstTIDmaienc *dmaienc, GstBuffer *buf){
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)){
        switch (encoder->eops->codec_type) {
            case VIDEO:
                if (Buffer_getType(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf))
                    == Buffer_Type_GRAPHICS){
                    /* Easy: we got a gfx buffer from upstream */
                    return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
                } else {
                    /* Still easy: got a DMAI transport, just not of gfx type... */
                    Buffer_Handle hBuf;
                    BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;

                    gfxAttrs.bAttrs.reference   = TRUE;
                    gfxAttrs.dim.width          = dmaienc->width;
                    gfxAttrs.dim.height         = dmaienc->height;
                    gfxAttrs.colorSpace         = dmaienc->colorSpace;
                    gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(dmaienc->width,
                                                    dmaienc->colorSpace);

                    hBuf = Buffer_create(dmaienc->inBufSize, &gfxAttrs.bAttrs);
                    Buffer_setUserPtr(hBuf,
                        Buffer_getUserPtr(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)));
                    Buffer_setNumBytesUsed(hBuf,dmaienc->inBufSize);

                    return hBuf;
                }
                break;
            default:
                return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
        }
    } else {
        BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;
        Buffer_Attrs Attrs    = Buffer_Attrs_DEFAULT;
        Buffer_Attrs *attrs;

        switch (encoder->eops->codec_type) {
            case VIDEO:
                /* Slow path: Copy the data into gfx buffer */

                gfxAttrs.dim.width          = dmaienc->width;
                gfxAttrs.dim.height         = dmaienc->height;
                gfxAttrs.colorSpace         = dmaienc->colorSpace;
                gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(dmaienc->width,
                                                dmaienc->colorSpace);

                attrs = &gfxAttrs.bAttrs;
                break;
            default:
                attrs= &Attrs;
        }
        /* Allocate a Buffer tab and copy the data there */
        if (!dmaienc->inBuf){
            dmaienc->inBuf = Buffer_create(dmaienc->inBufSize,attrs);

            if (dmaienc->inBuf == NULL) {
                GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to create input buffers"));
                return NULL;
            }

            GST_DEBUG("Input buffer handler: %p\n",dmaienc->inBuf);
        }

        memcpy(Buffer_getUserPtr(dmaienc->inBuf),GST_BUFFER_DATA(buf),
                dmaienc->inBufSize);
        Buffer_setNumBytesUsed(dmaienc->inBuf,dmaienc->inBufSize);

        return dmaienc->inBuf;
    }
}

/******************************************************************************
 * encode
 *  This function encodes a frame and push the buffer downstream
 ******************************************************************************/
static GstFlowReturn encode(GstTIDmaienc *dmaienc,GstBuffer * rawData){
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;
    Buffer_Handle  hDstBuf,hSrcBuf;
    GstBuffer     *outBuf;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    /* Obtain a free output buffer for the decoded data */
    hSrcBuf = get_raw_buffer(dmaienc,rawData);
    hDstBuf = encode_buffer_get_free(dmaienc);

    if (!hSrcBuf || !hDstBuf){
        goto failure;
    }

    if (!encoder->eops->codec_process(dmaienc,hSrcBuf,hDstBuf)){
        goto failure;
    }

    dmaienc->head += Buffer_getNumBytesUsed(hDstBuf);

    /* Create a DMAI transport buffer object to carry a DMAI buffer to
     * the source pad.  The transport buffer knows how to release the
     * buffer for re-use in this element when the source pad calls
     * gst_buffer_unref().
         */
    outBuf = gst_tidmaibuffertransport_new(hDstBuf,
        &dmaienc->waitOnOutBuf,&dmaienc->outBufMutex);
    gst_tidmaibuffertransport_set_release_callback(
        (GstTIDmaiBufferTransport *)outBuf,release_cb,dmaienc);
    gst_buffer_copy_metadata(outBuf,rawData,
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);
    gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
        Buffer_getNumBytesUsed(hDstBuf));
    gst_buffer_set_caps(outBuf, GST_PAD_CAPS(dmaienc->srcpad));

    /* DMAI set the buffer type on the input buffer, since only this one
     * is a GFX buffer
     */
    if (gstti_bufferGFX_getFrameType(hSrcBuf) == IVIDEO_I_FRAME){
        GST_BUFFER_FLAG_UNSET(outBuf, GST_BUFFER_FLAG_DELTA_UNIT);
    } else {
        GST_BUFFER_FLAG_SET(outBuf, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    gst_buffer_unref(rawData);
    rawData = NULL;

    if (gst_pad_push(dmaienc->srcpad, outBuf) != GST_FLOW_OK) {
        GST_DEBUG("push to source pad failed\n");
    }

    return GST_FLOW_OK;

failure:
    if (rawData != NULL)
        gst_buffer_unref(rawData);

    return GST_FLOW_UNEXPECTED;
}


/******************************************************************************
 * gst_tidmaienc_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, and pass it to the parser, who is responsible to either
 *    buffer them until it has a full frame. If the parser returns a full frame
 *    we push a gsttidmaibuffer to the encoder function.
 ******************************************************************************/
static GstFlowReturn gst_tidmaienc_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)GST_OBJECT_PARENT(pad);
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    if (dmaienc->inBufSize == 0){
        dmaienc->inBufSize = GST_BUFFER_SIZE(buf);
    }

    if (dmaienc->require_configure){
        dmaienc->require_configure = FALSE;
        if (!gst_tidmaienc_configure_codec(dmaienc)) {
            return GST_FLOW_UNEXPECTED;
        }
    }

    if (GST_BUFFER_SIZE(buf) > dmaienc->inBufSize){
        GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
           ("Failed to encode buffer, received input buffer with size bigger than allocated memory"));
        gst_buffer_unref(buf);
        return GST_FLOW_UNEXPECTED;
    }

    if (!GST_IS_TIDMAIBUFFERTRANSPORT(buf) ||
        Buffer_getType(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf))
          != Buffer_Type_GRAPHICS){
        gst_adapter_push(dmaienc->adapter,buf);
        if (gst_adapter_available(dmaienc->adapter) >= dmaienc->inBufSize){
            buf = gst_adapter_take_buffer(dmaienc->adapter,dmaienc->inBufSize);
        } else {
            buf = NULL;
        }
    } else {
        GST_INFO("Using accelerated buffer\n");
    }

    if (buf){
        if (encode(dmaienc, buf) != GST_FLOW_OK) {
           GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
               ("Failed to encode buffer"));
           gst_buffer_unref(buf);
           return GST_FLOW_UNEXPECTED;
       }
    }

    return GST_FLOW_OK;
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
