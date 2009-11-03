/*
 * gsttisupport_aac.h
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Contributor:
 *     Diego Dompe, RidgeRun
 *     Cristina Murillo, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2009 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 * whether express or implied; without even the implied warranty of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GSTTI_SUPPORT_AAC_H__
#define __GSTTI_SUPPORT_AAC_H__

#include <gst/gst.h>

/* Caps for aac */
extern GstStaticCaps gstti_aac_sink_caps;
extern GstStaticCaps gstti_aac_src_caps;

/* AAC Parser */
struct gstti_aac_parser_private {
    gboolean    flushing;
    gboolean    framed;
};

extern struct gstti_parser_ops gstti_aac_parser;

#define MAIN_PROFILE 1
#define LC_PROFILE 2
#define SSR_PROFILE 3
#define LTP_PROFILE 4                                                                                                    
#define HEAAC_PROFILE 5

/* Define maximum number of bytes in ADIF header */
#define MAX_AAC_HEADER_LENGTH 20

/* Write id field in ADIF header */
#define ADIF_SET_ID(header, value) \
    memcpy(GST_BUFFER_DATA(header), value, 4);

/* Clear copyright_id_present field in ADIF header */
#define ADIF_CLEAR_COPYRIGHT_ID_PRESENT(header) \
    GST_BUFFER_DATA(header)[4] &=  ~0x1;

/* Write profile value in ADIF header */
#define ADIF_SET_PROFILE(header, value) \
    GST_BUFFER_DATA(header)[10] |= (value & 0x2); \
    GST_BUFFER_DATA(header)[11] |= ((value & 0x1) << 7);

/* Write sampling frequency index value in ADIF header */
#define ADIF_SET_SAMPLING_FREQUENCY_INDEX(header, value) \
    GST_BUFFER_DATA(header)[11] |= (value << 3);    

/* Write front_channel_element value in ADIF header */
#define ADIF_SET_FRONT_CHANNEL_ELEMENT(header, value) \
    GST_BUFFER_DATA(header)[11] |= (value >> 1); \
    GST_BUFFER_DATA(header)[12] |= ((value & 0x1) << 7);
    
/* Write comment field value in ADIF header */
#define ADIF_SET_COMMENT_FIELD(header, value) \
    GST_BUFFER_DATA(header)[16] |= value;

#endif /* __GSTTI_SUPPORT_AAC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
