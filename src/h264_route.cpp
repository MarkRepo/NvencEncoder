#ifndef _cplusplus
#define _cplusplus
#endif

#include "h264_route.h"
#include "NvEncoder.h"
#include "h264_log.h"

static CNvEncoder gNvEncoder;
FILE* rgba = NULL;
FILE* h264 = NULL;
char rgbafilename[] = "/var/log/rgb";
char h264filename[] = "/var/log/h264";
//extern volatile int a;

#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024

int h264_init(const int argc, ...)
{
#if 1
	if( (rgba = fopen(rgbafilename, "w")) == NULL){
		NvDebugLog("foepn rgbafile error");
		return -1;
	}
	if( (h264 = fopen(h264filename, "w")) == NULL){
		NvDebugLog("fopen h264file error.");
		return -1;
	}
#endif
	H264_LogInitFile(gLogFileName);
	return  gNvEncoder.EncoderInit(argc);
}

int h264_route(const pixman_image_t* ppi, int argc, ...)
{
	static int  call_cnt = 0;
	//static unsigned long long  lasttime = 0;
	call_cnt++;
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	uint8_t *pData;
	int 	curWidth = 0, curHeight = 0, oldWidth = 0, oldHeight = 0, stride = 0;
	unsigned long long eStart = 0, eEnd = 0, Freq = 0;
	NvQueryPerformanceCounter(&eStart);
	NvQueryPerformanceFrequency(&Freq);
	pixman_image_t * ncppi = const_cast<pixman_image_t*>(ppi);
	pData     = (uint8_t*)pixman_image_get_data(ncppi);
	curWidth  = pixman_image_get_width(ncppi);
	curHeight = pixman_image_get_height(ncppi);
	stride	  = pixman_image_get_stride(ncppi);
	if(!pData){
		//NvDebugLog("pixman_image_get_data is NULL, curWidth:%d, curHeight:%d", curWidth, curHeight);
		return 0;
	}

	if(stride < 0){
		pData += stride*(curHeight-1);
	}
	
	//NvDebugLog("curWidth: %d, curHeight: %d, stride: %d", curWidth, curHeight, stride);
	oldWidth  = gNvEncoder.m_pNvHWEncoder->m_stCreateEncodeParams.encodeWidth;
	oldHeight = gNvEncoder.m_pNvHWEncoder->m_stCreateEncodeParams.encodeHeight;
	if(curWidth != oldWidth || curHeight != oldHeight){
		//reconfig
#if 0
		NvEncPictureCommand encPicCommand;
        memset(&encPicCommand, 0, sizeof(NvEncPictureCommand));
		encPicCommand.bResolutionChangePending = true;
		encPicCommand.newWidth = curWidth;
		encPicCommand.newHeight = curHeight;
        nvStatus = gNvEncoder.m_pNvHWEncoder->NvEncReconfigureEncoder(&encPicCommand);
        if (nvStatus != NV_ENC_SUCCESS)
        {
        	NvDebugLog("Call NvEncReconfigEncoder failed, nvStatus: %d", nvStatus);
         	return -1;
        }
#endif
		return 0;
	}
	
#if 0
	if(call_cnt % 30 == 0){
		fwrite((uint8_t *)pData, curWidth*curHeight*4, 1, rgba);
	}
#endif
	nvStatus = gNvEncoder.EncodeFrame(pData, false, curWidth, curHeight);
	if(nvStatus != NV_ENC_SUCCESS){
		NvDebugLog("EncoderFrame failed, nvStatus: %d", nvStatus);
		return -1;
	}
	NvQueryPerformanceCounter(&eEnd);
#if 0
	double elapstime = (double)(eEnd-eStart);
	double elapstime2 = (double)((eStart-lasttime)*1000.0)/Freq;
	lasttime = eStart;
	//NvDebugLog("EncodeFrame idx: %d,FrameComeTime: %6.2fms, elapstime: %6.2fms.", call_cnt, elapstime2,(elapstime*1000.0)/Freq);
	printf("elapstime: %6.2fms\n", (elapstime*1000.0)/Freq);
#endif
	return 0;
}

int h264_release()
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	int width = 0, height = 0;
	width  = gNvEncoder.m_pNvHWEncoder->m_stCreateEncodeParams.encodeWidth;
	height = gNvEncoder.m_pNvHWEncoder->m_stCreateEncodeParams.encodeHeight;
	nvStatus = gNvEncoder.EncodeFrame(NULL, true, width, height);
	if(nvStatus != NV_ENC_SUCCESS)
		return -1;
	if ( (nvStatus = gNvEncoder.Deinitialize()) != NV_ENC_SUCCESS)
		return -1;
	H264_LogCloseFile();
	return 0;
}

int h264_init_zc(const int max_number_surface)
{
#if 0
	if( (rgba = fopen(rgbafilename, "w")) == NULL){
		NvDebugLog("foepn rgbafile error");
		return -1;
	}

	if( (h264 = fopen(h264filename, "w")) == NULL){
		NvDebugLog("fopen h264file error.");
		return -1;
	}
#endif
	H264_LogInitFile(gLogFileName);
	return  gNvEncoder.EncoderInitZC(max_number_surface);
}

void* h264_create_mem_zc(const int stride, const int height, const int id)
{
	NvDebugLog("create mem zc stride: %d, height: %d, id: %d", stride, height, id);
	//CUresult result = CUDA_SUCCESS;
	//NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	if(id >= gNvEncoder.m_stEncodeBufferPtrCnt || id < 0)
	{
		NvDebugLog("Invalid id: %d", id);
		return NULL;
	}
	
	if(gNvEncoder.m_stEncodeBufferPtrArr[id]){
		NvDebugLog("this ID EncodeBuffer exit, start destroy resource.");
		h264_destroy_mem_zc(id);
	}
	
	CCudaAutoLock cuLock(gNvEncoder.m_pDevice);
	gNvEncoder.m_stEncodeBufferPtrArr[id] = (EncodeBufferPtr)malloc(sizeof(EncodeBuffer));
	EncodeBufferPtr ebPtr = gNvEncoder.m_stEncodeBufferPtrArr[id];
	if(!ebPtr)
	{
		NvDebugLog("malloc EncodeBuffer error.");
		return NULL;
	}
	
	int wstride = (stride > 0) ? stride : (-1*stride);
	ebPtr->stInputBfr.surfaceId = id;
	ebPtr->stInputBfr.dwWidth  = wstride / 4;
	ebPtr->stInputBfr.dwHeight = height;
	ebPtr->stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;

	__cur(cuMemHostAlloc((void**)&ebPtr->stInputBfr.pRGBHostPtr, wstride*height, CU_MEMHOSTALLOC_DEVICEMAP |CU_MEMHOSTALLOC_WRITECOMBINED), NULL);
	__cur(cuMemHostGetDevicePointer(&ebPtr->stInputBfr.pNV12devPtr, ebPtr->stInputBfr.pRGBHostPtr, 0), NULL);
	__nvr(gNvEncoder.m_pNvHWEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)ebPtr->stInputBfr.pNV12devPtr, 
															wstride*height, 1, wstride, &ebPtr->stInputBfr.nvRegisteredResource), NULL);
	__nvr(gNvEncoder.m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &ebPtr->stOutputBfr.hBitstreamBuffer), NULL);
	
    ebPtr->stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;
    ebPtr->stOutputBfr.hOutputEvent = NULL;
	NvDebugLog("create mem success, pRGBHostPtr: %p", ebPtr->stInputBfr.pRGBHostPtr);
	return (void*)ebPtr->stInputBfr.pRGBHostPtr;
}


int   h264_destroy_mem_zc(const int id)
{
	NvDebugLog("destroy id: %d mem zc, currentUseId: %d", id, gNvEncoder.m_currentUseId);
	if( id < 0 || id >= gNvEncoder.m_stEncodeBufferPtrCnt){
		NvDebugLog("Invalid id: %d", id);
		return -1;
	}

	EncodeBufferPtr ebPtr = gNvEncoder.m_stEncodeBufferPtrArr[id];
	if(ebPtr == NULL){
		NvDebugLog("this id's EncoderBufferPtr is NULL, return -1");
		return -1;
	}

	if(id == gNvEncoder.m_currentUseId)
		gNvEncoder.m_currentUseId = -1;

	//NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	//CUresult	result 	 = CUDA_SUCCESS;
	CCudaAutoLock cuLock(gNvEncoder.m_pDevice);
	
	__nv(gNvEncoder.m_pNvHWEncoder->NvEncUnregisterResource(ebPtr->stInputBfr.nvRegisteredResource));
	
	if(ebPtr->stInputBfr.pRGBHostPtr){
		__cu(cuMemFreeHost(ebPtr->stInputBfr.pRGBHostPtr));
		ebPtr->stInputBfr.pRGBHostPtr = NULL;
	}
	 __nv(gNvEncoder.m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(ebPtr->stOutputBfr.hBitstreamBuffer));
	 ebPtr->stOutputBfr.hBitstreamBuffer = NULL;
	
	free(ebPtr);
	gNvEncoder.m_stEncodeBufferPtrArr[id] = NULL;

	return 0;
	
}

int h264_route_zc(const int id)
{
	static unsigned long long  lasttime = 0;
	unsigned long long eStart = 0, eEnd = 0, Freq = 0;
	NvQueryPerformanceCounter(&eStart);
	NvQueryPerformanceFrequency(&Freq);
	NvDebugLog("h264 route zc, id: %d, currentUseId: %d", id, gNvEncoder.m_currentUseId);
	if(id >= gNvEncoder.m_stEncodeBufferPtrCnt || id < 0)
	{
		NvDebugLog("Invalid id: %d", id);
		return -1;
	}

	EncodeBufferPtr ebPtr = gNvEncoder.m_stEncodeBufferPtrArr[id];
	if(ebPtr == NULL){
		NvDebugLog("m_stEncodeBufferPtrArr[id] is NULL.");
		return -1;
	}
	
	if(	id != gNvEncoder.m_currentUseId){
		if(ebPtr->stInputBfr.dwWidth == gNvEncoder.m_pNvHWEncoder->m_uCurWidth && 
		   ebPtr->stInputBfr.dwHeight == gNvEncoder.m_pNvHWEncoder->m_uCurHeight){
		    NvDebugLog("id is not equal currentId, width and height not change. just make currentUseId equal id.");
			gNvEncoder.m_currentUseId = id;
		}
		else{
#if 0
			NvEncPictureCommand encPicCommand;
	        memset(&encPicCommand, 0, sizeof(NvEncPictureCommand));
			encPicCommand.bResolutionChangePending = true;
			encPicCommand.newWidth = ebPtr->stInputBfr.dwWidth;
			encPicCommand.newHeight = ebPtr->stInputBfr.dwHeight;
			NvDebugLog("NvEncReconfigureEncoder, width:%d, height:%d", ebPtr->stInputBfr.dwWidth, ebPtr->stInputBfr.dwHeight);
	        __nv( gNvEncoder.m_pNvHWEncoder->NvEncReconfigureEncoder(&encPicCommand));
			gNvEncoder.m_currentUseId = id;
#endif
			return 0;
		}
	}
	
#if 0
	static int i = 0;
	if(i<200){
		fwrite((uint8_t *)ebPtr->stInputBfr.pRGBHostPtr, 1920*1080*4, 1, rgba);
		i++;
	}
#endif
	if(gNvEncoder.m_pNvHWEncoder->m_connfd > 0)
		__nv(gNvEncoder.EncodeFrameZC(id));
	NvDebugLog("route success.");

	NvQueryPerformanceCounter(&eEnd);
#if 1
	double elapstime = (double)((eEnd-eStart)*1000.0)/Freq;
	double elapstime2 = (double)((eStart-lasttime)*1000.0)/Freq;
	lasttime = eStart;
	NvLog("LastFrameTime: %6.2fms, elapstime: %6.2fms.", elapstime2, elapstime);
	//printf("elapstime: %6.2fms\n", (elapstime*1000.0)/Freq);
#endif
	return 0;
}

int h264_release_zc()
{
	__nv( gNvEncoder.m_pNvHWEncoder->NvEncFlushEncoderQueue(gNvEncoder.m_stEOSOutputBfr.hOutputEvent));
	__nv( gNvEncoder.DeinitializeZC());
	H264_LogCloseFile();
	return 0;
}

int nvenc_init(const int max_number_surface,const H264_INPUT_BUFFER_TYPE buffer_type)
{
#if 0
	char rgb_file_path[128] = {0};
	char h264_file_path[128] = {0};
	snprintf(rgb_file_path, 127, "/ovpdatastore/rgb_%d.rgb32", getpid());
	snprintf(h264_file_path, 127, "/ovpdatastore/h264_%d.h264", getpid());
	if( (rgba = fopen(rgb_file_path, "w")) == NULL){
		NvDebugLog("foepn rgbafile error");
		return -1;
	}
	if( (h264 = fopen(h264_file_path, "w")) == NULL){
		NvDebugLog("fopen h264file error.");
		return -1;
	}
#endif

	H264_LogInitFile(gLogFileName);
	gNvEncoder.m_currentBufferType = buffer_type;
	return gNvEncoder.EncoderInitZC(max_number_surface);
}

void* nvenc_create_mem(const int stride,const int height,const int id)
{
	uint32_t lockedPitch = 0;
	NvDebugLog("create mem zc stride: %d, height: %d, id: %d", stride, height, id);
	//CUresult result = CUDA_SUCCESS;
	//NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	if(id >= gNvEncoder.m_stEncodeBufferPtrCnt || id < 0)
	{
		NvDebugLog("Invalid id: %d", id);
		return NULL;
	}
	
	if(gNvEncoder.m_stEncodeBufferPtrArr[id]){
		NvDebugLog("this ID EncodeBuffer exit, start destroy resource.");
		h264_destroy_mem_zc(id);
	}
	
	CCudaAutoLock cuLock(gNvEncoder.m_pDevice);
	gNvEncoder.m_stEncodeBufferPtrArr[id] = (EncodeBufferPtr)malloc(sizeof(EncodeBuffer));
	EncodeBufferPtr ebPtr = gNvEncoder.m_stEncodeBufferPtrArr[id];
	if(!ebPtr)
	{
		NvDebugLog("malloc EncodeBuffer error.");
		return NULL;
	}
	
	int wstride = (stride > 0) ? stride : (-1*stride);
	ebPtr->stInputBfr.surfaceId = id;
	ebPtr->stInputBfr.dwWidth  = wstride / 4;
	ebPtr->stInputBfr.dwHeight = height;
	ebPtr->stInputBfr.pRGBHostPtr = NULL;
	ebPtr->stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
	switch(gNvEncoder.m_currentBufferType)
	{
		case NVENC_INPUT_BUFFER:
#if 0
			ebPtr->stInputBfr.pRGBHostPtr = (uint8_t*)malloc(wstride*height);
			if(!(ebPtr->stInputBfr.pRGBHostPtr)){
				NvDebugLog("pRgbHostPtr malloc error.");
				return NULL;
			}
			__nvr(gNvEncoder.m_pNvHWEncoder->NvEncCreateInputBuffer(wstride/4, height, &ebPtr->stInputBfr.hInputSurface, ebPtr->stInputBfr.bufferFmt), NULL);
#endif
			__nvr(gNvEncoder.m_pNvHWEncoder->NvEncCreateInputBuffer(wstride/4, height, &ebPtr->stInputBfr.hInputSurface, ebPtr->stInputBfr.bufferFmt), NULL);
			__nvr(gNvEncoder.m_pNvHWEncoder->NvEncLockInputBuffer(ebPtr->stInputBfr.hInputSurface, (void**)&ebPtr->stInputBfr.pRGBHostPtr, &lockedPitch),NULL);
			//NvLog("pRGBHostPtr: %p", ebPtr->stInputBfr.pRGBHostPtr);
			break;
		case CUDA_DEVICE_BUFFER:
			break;
		case CUDA_HOST_BUFFER:
			__cur(cuMemHostAlloc((void**)&ebPtr->stInputBfr.pRGBHostPtr, wstride*height, CU_MEMHOSTALLOC_DEVICEMAP |CU_MEMHOSTALLOC_WRITECOMBINED), NULL);
			__cur(cuMemHostGetDevicePointer(&ebPtr->stInputBfr.pNV12devPtr, ebPtr->stInputBfr.pRGBHostPtr, 0), NULL);
			__nvr(gNvEncoder.m_pNvHWEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)ebPtr->stInputBfr.pNV12devPtr, 
															wstride*height, 1, wstride, &ebPtr->stInputBfr.nvRegisteredResource), NULL);
			break;
		default:
			break;
	}
	
	__nvr(gNvEncoder.m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &ebPtr->stOutputBfr.hBitstreamBuffer), NULL);
    ebPtr->stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;
    ebPtr->stOutputBfr.hOutputEvent = NULL;
	NvDebugLog("create mem success, pRGBHostPtr: %p", ebPtr->stInputBfr.pRGBHostPtr);
	return (void*)ebPtr->stInputBfr.pRGBHostPtr;
}

int   nvenc_destroy_mem(const int id)
{
	NvDebugLog("destroy id: %d mem zc, currentUseId: %d", id, gNvEncoder.m_currentUseId);
	if( id < 0 || id >= gNvEncoder.m_stEncodeBufferPtrCnt){
		NvDebugLog("Invalid id: %d", id);
		return -1;
	}

	EncodeBufferPtr ebPtr = gNvEncoder.m_stEncodeBufferPtrArr[id];
	if(ebPtr == NULL){
		NvDebugLog("this id's EncoderBufferPtr is NULL, return -1");
		return -1;
	}

	if(id == gNvEncoder.m_currentUseId)
		gNvEncoder.m_currentUseId = -1;

	//NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	//CUresult	result 	 = CUDA_SUCCESS;
	CCudaAutoLock cuLock(gNvEncoder.m_pDevice);

	switch(gNvEncoder.m_currentBufferType)
	{
		case NVENC_INPUT_BUFFER:
			__nv(gNvEncoder.m_pNvHWEncoder->NvEncDestroyInputBuffer(ebPtr->stInputBfr.hInputSurface));
			//free(ebPtr->stInputBfr.pRGBHostPtr);
			break;
		case CUDA_DEVICE_BUFFER:
			break;
		case CUDA_HOST_BUFFER:
			__nv(gNvEncoder.m_pNvHWEncoder->NvEncUnregisterResource(ebPtr->stInputBfr.nvRegisteredResource));
			__cu(cuMemFreeHost(ebPtr->stInputBfr.pRGBHostPtr));
			break;
		default:
			break;
	}

	 __nv(gNvEncoder.m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(ebPtr->stOutputBfr.hBitstreamBuffer));
	 ebPtr->stOutputBfr.hBitstreamBuffer = NULL;
	
	free(ebPtr);
	gNvEncoder.m_stEncodeBufferPtrArr[id] = NULL;

	return 0;
	
}


int nvenc_route(const int id)
{
	return h264_route_zc(id);
}

int nvenc_release()
{
	__nv( gNvEncoder.m_pNvHWEncoder->NvEncFlushEncoderQueue(gNvEncoder.m_stEOSOutputBfr.hOutputEvent));
	__nv( gNvEncoder.DeinitializeZC());
	H264_LogCloseFile();
	fclose(rgba);
	fclose(h264);
	return 0;
}



