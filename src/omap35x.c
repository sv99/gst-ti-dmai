/*
 * omap35x.c
 *
 * This file provides filtered capabilities for the codecs used with the
 * default codec combo for omap35x
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
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

#include  "gstticommonutils.h"
#include "ti_encoders.h"
#include "caps.h"

struct codec_custom_data_entry codec_custom_data[] = {
    { .codec_name = "mpeg4enc",
      .data = {
        .sinkCaps = &gstti_D1_uyvy_caps,
        .srcCaps = &gstti_D1_mpeg4_src_caps,
        .setup_params = ti_mpeg4enc_params,
        .install_properties = ti_mpeg4enc_install_properties,
        .set_property = ti_mpeg4enc_set_property,
        .get_property = ti_mpeg4enc_get_property,
      },
    },
    { .codec_name = NULL },
};

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
