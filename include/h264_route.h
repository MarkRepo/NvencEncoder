#ifndef _H264_ROUTE_H_
#define _H264_ROUTE_H_

#include <pixman.h>
#include <stdint.h>

typedef enum {
	INVALID_BUFFER_TYPE = 0,
	NVENC_INPUT_BUFFER = 1,
	CUDA_DEVICE_BUFFER = 2,
	CUDA_HOST_BUFFER = 3,
}H264_INPUT_BUFFER_TYPE;

#ifdef _cplusplus
extern "C" 
{
#endif

extern int h264_init(const int argc, ...);
extern int h264_route(const pixman_image_t* ppi, const int argc, ...);
extern int h264_release();

// zero-copy interface
extern int   h264_init_zc(const int max_number_surface);
extern void* h264_create_mem_zc(const int stride, const int height, const int id);
extern int   h264_destroy_mem_zc(const int id);
extern int   h264_route_zc(const int id);
extern int   h264_release_zc();

//unify interface
extern int    nvenc_init(const int max_number_surface, const H264_INPUT_BUFFER_TYPE buffer_type);
extern void*  nvenc_create_mem(const int stride, const int height, const int id);
extern int	  nvenc_destroy_mem(const int id);
extern int 	  nvenc_route(const int id);
extern int    nvenc_release();

#ifdef _cplusplus
}
#endif

#endif

