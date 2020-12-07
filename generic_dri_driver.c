/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <xf86drm.h>

#include "dri.h"
#include "drv_priv.h"
#include "helpers.h"
#include "util.h"
#include <errno.h>

static uint32_t generic_dri_resolve_format(struct driver *drv, uint32_t format, uint64_t use_flags)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/* Camera subsystem requires NV12. */
		if (use_flags & (BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE))
			return DRM_FORMAT_NV12;
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_NV12;
	default:
		return format;
	}
}

static const uint32_t scanout_render_formats[] = { DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888,
						   DRM_FORMAT_ABGR8888, DRM_FORMAT_XBGR8888,
						   DRM_FORMAT_BGR888,	DRM_FORMAT_BGR565 };

static const uint32_t texture_only_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_NV21,
						 DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

static int replace_char(char *str, char orig, char rep)
{
	char *ix = str;
	int n = 0;
	while ((ix = strchr(ix, orig)) != NULL) {
		*ix++ = rep;
		n++;
	}
	return n;
}

static int generic_dri_driver_init(struct driver *drv)
{
	struct dri_driver *dri;

	drmVersionPtr drm_version;
	drm_version = drmGetVersion(drv->fd);

	if (!drm_version)
		return -ENOMEM;

	dri = calloc(1, sizeof(struct dri_driver));
	if (!dri)
		return -ENOMEM;

	drv->priv = dri;

	char dri_pathname[64];

	sprintf(dri_pathname, STRINGIZE(DRI_DRIVER_DIR) "/%s_dri.so", drm_version->name);

	char drv_suffix[32];
	strcpy(drv_suffix, drm_version->name);
	drmFreeVersion(drm_version);

	replace_char(drv_suffix, '-', '_');

	if (dri_init(drv, dri_pathname, drv_suffix)) {
		drv_log("dri_init failed for (%s) , (%s)", dri_pathname, drv_suffix);
		free(dri);
		drv->priv = NULL;
		return -ENODEV;
	}

	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK | BO_USE_SCANOUT);

	drv_add_combinations(drv, texture_only_formats, ARRAY_SIZE(texture_only_formats),
			     &LINEAR_METADATA, BO_USE_TEXTURE_MASK);

	drv_modify_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
			       BO_USE_HW_VIDEO_ENCODER | BO_USE_HW_VIDEO_DECODER |
				   BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);
	drv_modify_combination(drv, DRM_FORMAT_NV21, &LINEAR_METADATA, BO_USE_HW_VIDEO_ENCODER);

	return drv_modify_linear_combinations(drv);
}

const struct backend backend_dri_generic = {
	.name = "dri_generic",
	.init = generic_dri_driver_init,
	.bo_create = dri_bo_create,
	.bo_destroy = dri_bo_destroy,
	.bo_import = dri_bo_import,
	.bo_map = dri_bo_map,
	.bo_unmap = dri_bo_unmap,
	.bo_get_plane_fd = dri_bo_get_plane_fd,
	.resolve_format = generic_dri_resolve_format,
};
