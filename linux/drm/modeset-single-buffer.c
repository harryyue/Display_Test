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
	uint8_t *vaddr;
	uint32_t fb_id;
};

struct buffer_object buf;

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {};
	struct drm_mode_map_dumb map = {};

	/* create a dumb-buffer, the pixel format is XRGB888 */
	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	/* bind the dumb-buffer to an FB object */
	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			bo->handle, &bo->fb_id);

	/* map the dumb-buffer to
	 * userspace */
	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);

	/* initialize the
	 * dumb-buffer with
	 * white-color */
	memset(bo->vaddr, 0xff, bo->size);

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
	printf("fbs_count:%d\n",res->count_fbs);
	for (i=0; i<res->count_fbs; i++)
		printf("fb_id[%d]=%d\n", i, res->fbs[i]);

	printf("crtcs_count:%d\n",res->count_crtcs);
	for (i=0; i<res->count_crtcs; i++)
		printf("crtc_id[%d]=%d\n", i, res->crtcs[i]);

	printf("connectors_count:%d\n",res->count_connectors);
	for (i=0; i<res->count_connectors; i++)
		printf("conn_id[%d]=%d\n", i, res->connectors[i]);

	printf("encoders_count:%d\n",res->count_encoders);
	for (i=0; i<res->count_crtcs; i++)
		printf("encoder_id[%d]=%d\n", i, res->encoders[i]);
	printf("===================================================================================================\n");

	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

	conn = drmModeGetConnector(fd, conn_id);
	buf.width = conn->modes[0].hdisplay;
	buf.height = conn->modes[0].vdisplay;

	modeset_create_fb(fd, &buf);

	drmModeSetCrtc(fd, crtc_id, buf.fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);

	getchar();

	memset(buf.vaddr, 0x77, buf.size);
	getchar();
	memset(buf.vaddr, 0x00, buf.size);
	getchar();

	modeset_destroy_fb(fd, &buf);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);

	close(fd);

	return 0;

}
