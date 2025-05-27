#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint32_t *vaddr;
	uint32_t fb_id;
};

struct buffer_object buf[2];

static int modeset_create_fb(int fd, struct buffer_object *bo, uint32_t color)
{
	struct drm_mode_create_dumb create = {};
	struct drm_mode_map_dumb map = {};
	uint32_t i;

	/* create a dumb-buffer, the pixel format is XRGB888 */
	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	/* bind the dumb-buffer to an FB object */
	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch, bo->handle, &bo->fb_id);

	/* map the dumb-buffer to userspace */
	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
	printf("%s: W:%d, H:%d, size:%d, pitch:%d, vaddr:%p\n", __func__, bo->width, bo->height, bo->size, bo->pitch, bo->vaddr);

	/* initialize the dumb-buffer */
	for(i=0; i<(bo->size/4); i++)
	{
		if (i < 8)
			printf("i:%d, bo->size:%d\n", i, bo->size/4);
		bo->vaddr[i] = color;
	}

	return 0;

}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);
	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

}

int main(int argc, char **argv)
{
	int fd;
	int i;
	drmModeConnector *conn;
	drmModeRes *res;
	uint32_t conn_id;
	uint32_t crtc_id;

	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

	res = drmModeGetResources(fd);

	printf("========================== drmModeGetResources() ==================================================\n");
	printf("min_width:%d, max_width:%d, min_height:%d, max_height:%d\n",res->min_width, res->max_width, res->min_height, res->max_height);
	printf("---------------------------------------------------------------------------------------------------\n");
	printf("fbs_count:%d\n",res->count_fbs);
	for (i=0; i<res->count_fbs; i++)
		printf("fb_id[%d]=%d\n", i, res->fbs[i]);
	printf("---------------------------------------------------------------------------------------------------\n");

	printf("crtcs_count:%d\n",res->count_crtcs);
	for (i=0; i<res->count_crtcs; i++)
		printf("crtc_id[%d]=%d\n", i, res->crtcs[i]);
	printf("---------------------------------------------------------------------------------------------------\n");

	printf("connectors_count:%d\n",res->count_connectors);
	for (i=0; i<res->count_connectors; i++)
		printf("conn_id[%d]=%d\n", i, res->connectors[i]);
	printf("---------------------------------------------------------------------------------------------------\n");

	printf("encoders_count:%d\n",res->count_encoders);
	for (i=0; i<res->count_crtcs; i++)
		printf("encoder_id[%d]=%d\n", i, res->encoders[i]);

	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];
	printf("===================================================================================================\n");

	conn = drmModeGetConnector(fd, conn_id);
	printf("========================== drmModeGetConnector() ==================================================\n");
	printf("conn_id:%d, encoder_id:%d, connection_status:%d, connector_type:%d, connector_type_id:%d\n",
			conn->connector_id, conn->encoder_id, conn->connection, conn->connector_type, conn->connector_type_id);
	printf("HW_Width:%d,HW_Height:%d, subpixel:%d\n",conn->mmWidth, conn->mmHeight, conn->subpixel);
	printf("---------------------------------------------------------------------------------------------------\n");
	printf("modes_count:%d\n",conn->count_modes);
	for (i=0; i<conn->count_modes; i++)
		printf("%i> conn_mode_name:%s\n", i, conn->modes[i].name);
	printf("---------------------------------------------------------------------------------------------------\n");

	printf("encoders_count:%d\n",conn->count_encoders);
	for (i=0; i<conn->count_encoders; i++)
		printf("%i> encoder_id:%d\n", i, conn->encoders[i]);
	printf("---------------------------------------------------------------------------------------------------\n");

	printf("prop_count:%d\n",conn->count_props);
	for (i=0; i<conn->count_props; i++)
		printf("%i> prop:%d, value:%ld\n", i, conn->props[i], conn->prop_values[i]);
	printf("---------------------------------------------------------------------------------------------------\n");

	buf[0].width = conn->modes[0].hdisplay;
	buf[0].height = conn->modes[0].vdisplay;
	buf[1].width = conn->modes[0].hdisplay;
	buf[1].height = conn->modes[0].vdisplay;
	buf[2].width = conn->modes[0].hdisplay;
	buf[2].height = conn->modes[0].vdisplay;

	printf("===================================================================================================\n");

	modeset_create_fb(fd, &buf[0], 0xffff0000); /* ARGB, A:0xff, R:0xff, G:0x00, B:0x00 */
	modeset_create_fb(fd, &buf[1], 0xff0000ff); /* ARGB, A:0xff, R:0x00, G:0x00, B:0xff */
	modeset_create_fb(fd, &buf[2], 0xff00ff00); /* ARGB, A:0xff, R:0x00, G:0xff, B:0x00 */

	printf("Show Red Color\n");
	drmModeSetCrtc(fd, crtc_id, buf[0].fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);
	getchar();

	printf("Show Blue Color\n");
	drmModeSetCrtc(fd, crtc_id, buf[1].fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);
	getchar();

	printf("Show Green Color\n");
	drmModeSetCrtc(fd, crtc_id, buf[2].fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);
	getchar();

	printf("> Exit!!!\n");
	modeset_destroy_fb(fd, &buf[0]);
	modeset_destroy_fb(fd, &buf[1]);
	modeset_destroy_fb(fd, &buf[2]);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);

	close(fd);

	return 0;

}
