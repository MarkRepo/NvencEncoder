////////////////////////////////////////////////////////////////////////////
//
// Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
//
// Please refer to the NVIDIA end user license agreement (EULA) associated
// with this source code for terms and conditions that govern your use of
// this software. Any use, reproduction, disclosure, or distribution of
// this software and related documentation outside the terms of the EULA
// is strictly prohibited.
//
////////////////////////////////////////////////////////////////////////////

#include "nvCPUOPSys.h"
#include "nvEncodeAPI.h"
#include "nvUtils.h"
#include "NvEncoder.h"
#include "h264_log.h"
#include <new>
#include <unistd.h>

#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024
extern FILE* rgba;

void convertBGRApitchtoARGB(const void* pFrameBGRA, uint8_t* pInputSurface, int width, int height, int srcStride, int dstStride)
{
	//int y, x;
	//static int k = 0;
	//for(y=0; y<height; y++){
	memcpy(pInputSurface, pFrameBGRA, height*width*4);
		/*for(x=0; x<width; x++){
			pInputSurface[dstStride*y+4*x]   = pFrameBGRA[width*y+x].rgbRed;
			pInputSurface[dstStride*y+4*x+1] = pFrameBGRA[width*y+x].rgbGreen;
			pInputSurface[dstStride*y+4*x+2] = pFrameBGRA[width*y+x].rgbBlue;
			pInputSurface[dstStride*y+4*x+3] = pFrameBGRA[width*y+x].rgbAlpha;
		}*/
	//}
	/*
	if(k == 0){
		for( y=0; y<height*width/16; y++){
	  		printf("%02x ", pInputSurface[y]);
		}
		printf("\n");
	}
	k=k+1;*/
	
}

void convertBGRApitchtoABGR(const void* pFrameBGRA, unsigned char* pInputSurface, int width, int height, int srcStride, int dstStride)
{

	memcpy(pInputSurface, pFrameBGRA, height*width*4);
/*
	int y, x;
	for(y=0; y<height; y++){
		for(x=0; x<width; x++){
			pInputSurface[dstStride*y+4*x]   = pFrameBGRA[width*y+x].rgbBlue;
			pInputSurface[dstStride*y+4*x+1] = pFrameBGRA[width*y+x].rgbGreen;
			pInputSurface[dstStride*y+4*x+2] = pFrameBGRA[width*y+x].rgbRed;
			pInputSurface[dstStride*y+4*x+3] = pFrameBGRA[width*y+x].rgbAlpha;
		}
	}
	*/
}

void convertYUVpitchtoNV12( unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
                            unsigned char *nv12_luma, unsigned char *nv12_chroma,
                            int width, int height , int srcStride, int dstStride)
{
    int y;
    int x;
    if (srcStride == 0)
        srcStride = width;
    if (dstStride == 0)
        dstStride = width;

    for ( y = 0 ; y < height ; y++)
    {
        memcpy( nv12_luma + (dstStride*y), yuv_luma + (srcStride*y) , width );
    }

    for ( y = 0 ; y < height/2 ; y++)
    {
        for ( x= 0 ; x < width; x=x+2)
        {
            nv12_chroma[(y*dstStride) + x] =    yuv_cb[((srcStride/2)*y) + (x >>1)];
            nv12_chroma[(y*dstStride) +(x+1)] = yuv_cr[((srcStride/2)*y) + (x >>1)];
        }
    }
}

void convertYUV10pitchtoP010PL(unsigned short *yuv_luma, unsigned short *yuv_cb, unsigned short *yuv_cr,
    unsigned short *nv12_luma, unsigned short *nv12_chroma, int width, int height, int srcStride, int dstStride)
{
    int x, y;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            nv12_luma[(y*dstStride / 2) + x] = yuv_luma[(srcStride*y) + x] << 6;
        }
    }

    for (y = 0; y < height / 2; y++)
    {
        for (x = 0; x < width; x = x + 2)
        {
            nv12_chroma[(y*dstStride / 2) + x] = yuv_cb[((srcStride / 2)*y) + (x >> 1)] << 6;
            nv12_chroma[(y*dstStride / 2) + (x + 1)] = yuv_cr[((srcStride / 2)*y) + (x >> 1)] << 6;
        }
    }
}

void convertYUVpitchtoYUV444(unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
    unsigned char *surf_luma, unsigned char *surf_cb, unsigned char *surf_cr, int width, int height, int srcStride, int dstStride)
{
    int h;

    for (h = 0; h < height; h++)
    {
        memcpy(surf_luma + dstStride * h, yuv_luma + srcStride * h, width);
        memcpy(surf_cb + dstStride * h, yuv_cb + srcStride * h, width);
        memcpy(surf_cr + dstStride * h, yuv_cr + srcStride * h, width);
    }
}

void convertYUV10pitchtoYUV444(unsigned short *yuv_luma, unsigned short *yuv_cb, unsigned short *yuv_cr,
    unsigned short *surf_luma, unsigned short *surf_cb, unsigned short *surf_cr,
    int width, int height, int srcStride, int dstStride)
{
    int x, y;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            surf_luma[(y*dstStride / 2) + x] = yuv_luma[(srcStride*y) + x] << 6;
            surf_cb[(y*dstStride / 2) + x] = yuv_cb[(srcStride*y) + x] << 6;
            surf_cr[(y*dstStride / 2) + x] = yuv_cr[(srcStride*y) + x] << 6;
        }
    }
}

CNvEncoder::CNvEncoder()
{
    m_pNvHWEncoder = new CNvHWEncoder;
    m_pDevice = NULL;
#if defined (NV_WINDOWS)
    m_pD3D = NULL;
#endif
   // m_cuContext = NULL;

    m_uEncodeBufferCount = 0;
    memset(&m_stEncoderInput, 0, sizeof(m_stEncoderInput));
    memset(&m_stEOSOutputBfr, 0, sizeof(m_stEOSOutputBfr));
    memset(&m_stMVBuffer, 0, sizeof(m_stMVBuffer));
    //memset(&m_stEncodeBuffer, 0, sizeof(m_stEncodeBuffer));
    m_stEncodeBufferPtrArr = NULL;
	m_stEncodeBufferPtrCnt = 0;
	m_currentUseId = -1;
	m_currentBufferType = INVALID_BUFFER_TYPE;
	m_numFramesEncoded = 0;
}

CNvEncoder::~CNvEncoder()
{
    if (m_pNvHWEncoder)
    {
        delete m_pNvHWEncoder;
        m_pNvHWEncoder = NULL;
    }
	for(int i = 0; i< m_stEncodeBufferPtrCnt; i++){
		if(m_stEncodeBufferPtrArr[i] != NULL)
			free(m_stEncodeBufferPtrArr[i]);
	}
	free(m_stEncodeBufferPtrArr);
}

NVENCSTATUS CNvEncoder::InitCuda(uint32_t deviceID)
{
    CUresult cuResult;
    CUdevice device;
    CUcontext cuContextCurr;
    int  deviceCount = 0;
    int  SMminor = 0, SMmajor = 0;

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    typedef HMODULE CUDADRIVER;
#else
    typedef void *CUDADRIVER;
#endif
    CUDADRIVER hHandleDriver = 0;
    cuResult = cuInit(0, __CUDA_API_VERSION, hHandleDriver);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuInit error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    cuResult = cuDeviceGetCount(&deviceCount);
	//printf("deviceCount : %d\n", deviceCount);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuDeviceGetCount error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    // If dev is negative value, we clamp to 0
    if ((int)deviceID < 0)
        deviceID = 0;

    if (deviceID >(unsigned int)deviceCount - 1)
    {
        NvDebugLog("Invalid Device Id = %d", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    cuResult = cuDeviceGet(&device, deviceID);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuDeviceGet error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    cuResult = cuDeviceComputeCapability(&SMmajor, &SMminor, deviceID);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuDeviceComputeCapability error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    if (((SMmajor << 4) + SMminor) < 0x30)
    {
        NvDebugLog("GPU %d does not have NVENC capabilities exiting", deviceID);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    cuResult = cuCtxCreate((CUcontext*)(&m_pDevice), CU_CTX_MAP_HOST, device);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuCtxCreate error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    cuResult = cuCtxPopCurrent(&cuContextCurr);
    if (cuResult != CUDA_SUCCESS)
    {
        NvDebugLog("cuCtxPopCurrent error:0x%x", cuResult);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }
    return NV_ENC_SUCCESS;
}

#if defined(NV_WINDOWS)
NVENCSTATUS CNvEncoder::InitD3D9(uint32_t deviceID)
{
    D3DPRESENT_PARAMETERS d3dpp;
    D3DADAPTER_IDENTIFIER9 adapterId;
    unsigned int iAdapter = NULL; // Our adapter
    HRESULT hr = S_OK;

    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_pD3D == NULL)
    {
        assert(m_pD3D);
        return NV_ENC_ERR_OUT_OF_MEMORY;;
    }

    if (deviceID >= m_pD3D->GetAdapterCount())
    {
        PRINTERR("Invalid Device Id = %d\n. Please use DX10/DX11 to detect headless video devices.\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    hr = m_pD3D->GetAdapterIdentifier(deviceID, 0, &adapterId);
    if (hr != S_OK)
    {
        PRINTERR("Invalid Device Id = %d\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;//D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    DWORD dwBehaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING;

    hr = m_pD3D->CreateDevice(deviceID,
        D3DDEVTYPE_HAL,
        GetDesktopWindow(),
        dwBehaviorFlags,
        &d3dpp,
        (IDirect3DDevice9**)(&m_pDevice));

    if (FAILED(hr))
        return NV_ENC_ERR_OUT_OF_MEMORY;

    return  NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::InitD3D10(uint32_t deviceID)
{
    HRESULT hr;
    IDXGIFactory * pFactory = NULL;
    IDXGIAdapter * pAdapter;

    if (CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory) != S_OK)
    {
        return NV_ENC_ERR_GENERIC;
    }

    if (pFactory->EnumAdapters(deviceID, &pAdapter) != DXGI_ERROR_NOT_FOUND)
    {
        hr = D3D10CreateDevice(pAdapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0,
            D3D10_SDK_VERSION, (ID3D10Device**)(&m_pDevice));
        if (FAILED(hr))
        {
            PRINTERR("Problem while creating %d D3d10 device \n", deviceID);
            return NV_ENC_ERR_OUT_OF_MEMORY;
        }
    }
    else
    {
        PRINTERR("Invalid Device Id = %d\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    return  NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::InitD3D11(uint32_t deviceID)
{
    HRESULT hr;
    IDXGIFactory * pFactory = NULL;
    IDXGIAdapter * pAdapter;

    if (CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory) != S_OK)
    {
        return NV_ENC_ERR_GENERIC;
    }

    if (pFactory->EnumAdapters(deviceID, &pAdapter) != DXGI_ERROR_NOT_FOUND)
    {
        hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
            NULL, 0, D3D11_SDK_VERSION, (ID3D11Device**)(&m_pDevice), NULL, NULL);
        if (FAILED(hr))
        {
            PRINTERR("Problem while creating %d D3d11 device \n", deviceID);
            return NV_ENC_ERR_OUT_OF_MEMORY;
        }
    }
    else
    {
        PRINTERR("Invalid Device Id = %d\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    return  NV_ENC_SUCCESS;
}
#endif

NVENCSTATUS CNvEncoder::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, NV_ENC_BUFFER_FORMAT inputFormat)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);
    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        nvStatus = m_pNvHWEncoder->NvEncCreateInputBuffer(uInputWidth, uInputHeight, &m_stEncodeBuffer[i].stInputBfr.hInputSurface, inputFormat);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        m_stEncodeBuffer[i].stInputBfr.bufferFmt = inputFormat;
        m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
        m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;
        nvStatus = m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
         m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;
        if (m_stEncoderInput.enableAsyncMode)
        {
            nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            if (nvStatus != NV_ENC_SUCCESS)
                return nvStatus;
            m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
        }
        else
            m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
    }

    m_stEOSOutputBfr.bEOSFlag = TRUE;

    if (m_stEncoderInput.enableAsyncMode)
    {
        nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
    }
    else
        m_stEOSOutputBfr.hOutputEvent = NULL;

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::AllocateMVIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, NV_ENC_BUFFER_FORMAT inputFormat)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    m_MVBufferQueue.Initialize(m_stMVBuffer, m_uEncodeBufferCount);
    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        // Allocate Input, Reference surface
        for (uint32_t j = 0; j < 2; j++)
        {
            nvStatus = m_pNvHWEncoder->NvEncCreateInputBuffer(uInputWidth, uInputHeight, &m_stMVBuffer[i].stInputBfr[j].hInputSurface, inputFormat);
            if (nvStatus != NV_ENC_SUCCESS)
                return nvStatus;
            m_stMVBuffer[i].stInputBfr[j].bufferFmt = inputFormat;
            m_stMVBuffer[i].stInputBfr[j].dwWidth = uInputWidth;
            m_stMVBuffer[i].stInputBfr[j].dwHeight = uInputHeight;
        }
        //Allocate output surface
        uint32_t encodeWidthInMbs = (uInputWidth + 15) >> 4;
        uint32_t encodeHeightInMbs = (uInputHeight + 15) >> 4;
        uint32_t dwSize = encodeWidthInMbs * encodeHeightInMbs * 64;
        nvStatus = m_pNvHWEncoder->NvEncCreateMVBuffer(dwSize, &m_stMVBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            PRINTERR("nvEncCreateMVBuffer error:0x%x\n", nvStatus);
            return nvStatus;
        }
        m_stMVBuffer[i].stOutputBfr.dwBitstreamBufferSize = dwSize;
        if (m_stEncoderInput.enableAsyncMode)
        {
            nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stMVBuffer[i].stOutputBfr.hOutputEvent);
            if (nvStatus != NV_ENC_SUCCESS)
                return nvStatus;
            m_stMVBuffer[i].stOutputBfr.bWaitOnEvent = true;
        }
        else
            m_stMVBuffer[i].stOutputBfr.hOutputEvent = NULL;
    }
    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::ReleaseIOBuffers()
{
    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        m_pNvHWEncoder->NvEncDestroyInputBuffer(m_stEncodeBuffer[i].stInputBfr.hInputSurface);
        m_stEncodeBuffer[i].stInputBfr.hInputSurface = NULL;
        m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;
        if (m_stEncoderInput.enableAsyncMode)
        {
            m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            //nvCloseFile(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
        }
    }

    if (m_stEOSOutputBfr.hOutputEvent)
    {
        if (m_stEncoderInput.enableAsyncMode)
        {
            m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEOSOutputBfr.hOutputEvent);
            //nvCloseFile(m_stEOSOutputBfr.hOutputEvent);
            m_stEOSOutputBfr.hOutputEvent = NULL;
        }
    }

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::ReleaseIOBuffersZC()
{
	//NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	CCudaAutoLock cuLock(m_pDevice);

	if(m_stEncodeBufferPtrArr != NULL){
		for(int i = 0; i< m_stEncodeBufferPtrCnt; i++){
			if(m_stEncodeBufferPtrArr[i] != NULL){
				switch(m_currentBufferType)
				{
					case NVENC_INPUT_BUFFER:
						__nv(m_pNvHWEncoder->NvEncDestroyInputBuffer(m_stEncodeBufferPtrArr[i]->stInputBfr.hInputSurface));
						//free(m_stEncodeBufferPtrArr[i]->stInputBfr.pRGBHostPtr);
						break;
					case CUDA_DEVICE_BUFFER:
						break;
					case CUDA_HOST_BUFFER:
						__nv(m_pNvHWEncoder->NvEncUnregisterResource(m_stEncodeBufferPtrArr[i]->stInputBfr.nvRegisteredResource));
						cuMemFreeHost(m_stEncodeBufferPtrArr[i]->stInputBfr.pRGBHostPtr);
						break;
					default:
						break;
				}
				
				__nv(m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBufferPtrArr[i]->stOutputBfr.hBitstreamBuffer));
				m_stEncodeBufferPtrArr[i]->stOutputBfr.hBitstreamBuffer = NULL;
				free(m_stEncodeBufferPtrArr[i]);
				m_stEncodeBufferPtrArr[i] = NULL;
			}
		}
	}
	free(m_stEncodeBufferPtrArr);
    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoder::ReleaseMVIOBuffers()
{
    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        for (uint32_t j = 0; j < 2; j++)
        {
            m_pNvHWEncoder->NvEncDestroyInputBuffer(m_stMVBuffer[i].stInputBfr[j].hInputSurface);
            m_stMVBuffer[i].stInputBfr[j].hInputSurface = NULL;
        }
        m_pNvHWEncoder->NvEncDestroyMVBuffer(m_stMVBuffer[i].stOutputBfr.hBitstreamBuffer);
        m_stMVBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;
        if (m_stEncoderInput.enableAsyncMode)
        {
            m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stMVBuffer[i].stOutputBfr.hOutputEvent);
            //nvCloseFile(m_stMVBuffer[i].stOutputBfr.hOutputEvent);
            m_stMVBuffer[i].stOutputBfr.hOutputEvent = NULL;
        }
    }

    return NV_ENC_SUCCESS;
}

void CNvEncoder::FlushMVOutputBuffer()
{
    MotionEstimationBuffer *pMEBufer = m_MVBufferQueue.GetPending();

    while (pMEBufer)
    {
            m_pNvHWEncoder->ProcessMVOutput(pMEBufer);
            pMEBufer = m_MVBufferQueue.GetPending();
    }
}

NVENCSTATUS CNvEncoder::FlushEncoder()
{
    NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvDebugLog("hwencoder nvencFlushEncoderQueue failed. nvStatus: %d", nvStatus);
        return nvStatus;
    }

    EncodeBuffer *pEncodeBufer = m_EncodeBufferQueue.GetPending();
    while (pEncodeBufer)
    {
        m_pNvHWEncoder->ProcessOutput(pEncodeBufer);
        pEncodeBufer = m_EncodeBufferQueue.GetPending();
    }

    return nvStatus;
}

NVENCSTATUS CNvEncoder::Deinitialize()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    ReleaseIOBuffers();
    nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
   
    CUresult cuResult = CUDA_SUCCESS;
    cuResult = cuCtxDestroy((CUcontext)m_pDevice);
    if (cuResult != CUDA_SUCCESS)
        NvDebugLog("cuCtxDestroy error:0x%x", cuResult);
    m_pDevice = NULL;
    return nvStatus;
}

NVENCSTATUS CNvEncoder::DeinitializeZC()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    ReleaseIOBuffersZC();
    nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
   
    CUresult cuResult = CUDA_SUCCESS;
    cuResult = cuCtxDestroy((CUcontext)m_pDevice);
    if (cuResult != CUDA_SUCCESS)
        NvDebugLog("cuCtxDestroy error:0x%x", cuResult);
    m_pDevice = NULL;
    return nvStatus;
}


#if 0
NVENCSTATUS loadframe(uint8_t *yuvInput[3], HANDLE hInputYUVFile, uint32_t frmIdx, uint32_t width, uint32_t height, uint32_t &numBytesRead, NV_ENC_BUFFER_FORMAT inputFormat)
{
    uint64_t fileOffset;
    uint32_t result;
    //Set size depending on whether it is YUV 444 or YUV 420
    uint32_t dwInFrameSize = 0;
    int anFrameSize[3] = {};
    switch (inputFormat) {
    default:
    case NV_ENC_BUFFER_FORMAT_NV12: 
        dwInFrameSize = width * height * 3 / 2; 
        anFrameSize[0] = width * height;
        anFrameSize[1] = anFrameSize[2] = width * height / 4;
        break;
    case NV_ENC_BUFFER_FORMAT_YUV444:
        dwInFrameSize = width * height * 3;
        anFrameSize[0] = anFrameSize[1] = anFrameSize[2] = width * height;
        break;
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        dwInFrameSize = width * height * 3;
        anFrameSize[0] = width * height * 2;
        anFrameSize[1] = anFrameSize[2] = width * height / 2;
        break;
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        dwInFrameSize = width * height * 6;
        anFrameSize[0] = anFrameSize[1] = anFrameSize[2] = width * height * 2;
        break;
    }
    fileOffset = (uint64_t)dwInFrameSize * frmIdx;
    result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER)
    {
        return NV_ENC_ERR_INVALID_PARAM;
    }
    nvReadFile(hInputYUVFile, yuvInput[0], anFrameSize[0], &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[1], anFrameSize[1], &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[2], anFrameSize[2], &numBytesRead, NULL);
    return NV_ENC_SUCCESS;
}
#endif
void PrintHelp()
{
    printf("Usage : NvEncoder \n"
        "-i <string>                  Specify input yuv420 file\n"
        "-o <string>                  Specify output bitstream file\n"
        "-size <int int>              Specify input resolution <width height>\n"
        "\n### Optional parameters ###\n"
        "-codec <integer>             Specify the codec \n"
        "                                 0: H264\n"
        "                                 1: HEVC\n"
        "-preset <string>             Specify the preset for encoder settings\n"
        "                                 hq : nvenc HQ \n"
        "                                 hp : nvenc HP \n"
        "                                 lowLatencyHP : nvenc low latency HP \n"
        "                                 lowLatencyHQ : nvenc low latency HQ \n"
        "                                 lossless : nvenc Lossless HP \n"
        "-startf <integer>            Specify start index for encoding. Default is 0\n"
        "-endf <integer>              Specify end index for encoding. Default is end of file\n"
        "-fps <integer>               Specify encoding frame rate\n"
        "-goplength <integer>         Specify gop length\n"
        "-numB <integer>              Specify number of B frames\n"
        "-bitrate <integer>           Specify the encoding average bitrate\n"
        "-vbvMaxBitrate <integer>     Specify the vbv max bitrate\n"
        "-vbvSize <integer>           Specify the encoding vbv/hrd buffer size\n"
        "-rcmode <integer>            Specify the rate control mode\n"
        "                                 0:  Constant QP\n"
        "                                 1:  Single pass VBR\n"
        "                                 2:  Single pass CBR\n"
        "                                 4:  Single pass VBR minQP\n"
        "                                 8:  Two pass frame quality\n"
        "                                 16: Two pass frame size cap\n"
        "                                 32: Two pass VBR\n"
        "-qp <integer>                Specify qp for Constant QP mode\n"
        "-i_qfactor <float>           Specify qscale difference between I-frames and P-frames\n"
        "-b_qfactor <float>           Specify qscale difference between P-frames and B-frames\n" 
        "-i_qoffset <float>           Specify qscale offset between I-frames and P-frames\n"
        "-b_qoffset <float>           Specify qscale offset between P-frames and B-frames\n" 
        "-picStruct <integer>         Specify the picture structure\n"
        "                                 1:  Progressive frame\n"
        "                                 2:  Field encoding top field first\n"
        "                                 3:  Field encoding bottom field first\n"
        "-devicetype <integer>        Specify devicetype used for encoding\n"
        "                                 0:  DX9\n"
        "                                 1:  DX11\n"
        "                                 2:  Cuda\n"
        "                                 3:  DX10\n"
        "-inputFormat <integer>       Specify the input format\n"
        "                                 0: YUV 420\n"
        "                                 1: YUV 444\n"
        "                                 2: YUV 420 10-bit\n"
        "                                 3: YUV 444 10-bit\n"
        "-deviceID <integer>           Specify the GPU device on which encoding will take place\n"
        "-meonly <integer>             Specify Motion estimation only(permissive value 1 and 2) to generates motion vectors and Mode information\n"
        "                                 1: Motion estimation between startf and endf\n"
        "                                 2: Motion estimation for all consecutive frames from startf to endf\n"
        "-preloadedFrameCount <integer> Specify number of frame to load in memory(default value=240) with min value 2(1 frame for ref, 1 frame for input)\n"
        "-temporalAQ                      1: Enable TemporalAQ\n"
        "-help                         Prints Help Information\n\n"
        );
}

//int CNvEncoder::EncoderInit(uint32_t ip, uint16_t port, int argc, ...)

int CNvEncoder::EncoderInit(const int argc, ...)
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	int result = 0;
	EncodeConfig encodeConfig;
	memset(&encodeConfig, 0, sizeof(EncodeConfig));

    encodeConfig.endFrameIdx = INT_MAX;
    encodeConfig.bitrate = 5000000;
    encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.deviceType = NV_ENC_CUDA;
    encodeConfig.codec = NV_ENC_H264;
    encodeConfig.fps = 30;
    encodeConfig.qp = 28;
    encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
    encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;
    encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
    encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET; 
    encodeConfig.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
    encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    encodeConfig.inputFormat = NV_ENC_BUFFER_FORMAT_ARGB;
	encodeConfig.height = HEIGHT_DEFAULT;
	encodeConfig.width  = WIDTH_DEFAULT;
	encodeConfig.codec = 0;
	//encodeConfig.sip   = ip;
	//encodeConfig.sport = port;

	//result = m_pNvHWEncoder->Init_tcp_connect();
	result = m_pNvHWEncoder->Init_tcp_serv();
	if(result < 0){
		NvDebugLog("tcp serv init failed, result: %d", result);
		return -1;
	}

    nvStatus = InitCuda(encodeConfig.deviceID);
	if(nvStatus != NV_ENC_SUCCESS){
		NvDebugLog("initcuda failed, nvStatus: %d", nvStatus);
		return -1;
	}
	
    nvStatus = m_pNvHWEncoder->Initialize(m_pDevice, NV_ENC_DEVICE_TYPE_CUDA);
    if (nvStatus != NV_ENC_SUCCESS){
		NvDebugLog("hwencoder initialize failed, nvstatus: %d", nvStatus);
		return -1;
	}

    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);
	nvStatus = m_pNvHWEncoder->CreateEncoder(&encodeConfig);
    if (nvStatus != NV_ENC_SUCCESS){
		NvDebugLog("create encoder failed, nvstatus: %d",nvStatus);
		return -1;
	}
	
	encodeConfig.maxWidth = encodeConfig.maxWidth ? encodeConfig.maxWidth : encodeConfig.width;
    encodeConfig.maxHeight = encodeConfig.maxHeight ? encodeConfig.maxHeight : encodeConfig.height;
    m_stEncoderInput.enableAsyncMode = encodeConfig.enableAsyncMode;
	
	m_uEncodeBufferCount = 1;
    m_uPicStruct = encodeConfig.pictureStruct;
    nvStatus = AllocateIOBuffers(encodeConfig.width, encodeConfig.height, encodeConfig.inputFormat);
    if (nvStatus != NV_ENC_SUCCESS){
		NvDebugLog("AllocateIOBuffers call failed, nvstatus: %d", nvStatus);
        return -1;
	}
	//NvDebugLog("Encoder init success.");
	return 0;
}

int CNvEncoder::EncoderInitZC(const int max_number_surface)
{
	int result = 0;
	m_stEncodeBufferPtrArr = (EncodeBufferPtr*)malloc(max_number_surface * sizeof(EncodeBufferPtr));
	if(!m_stEncodeBufferPtrArr)
	{
		NvDebugLog("m_stEncoderBufferPtrArr malloc error.");
		return -1;
	}
	memset(m_stEncodeBufferPtrArr, 0, max_number_surface*sizeof(EncodeBufferPtr));
	m_stEncodeBufferPtrCnt = max_number_surface;
	EncodeConfig encodeConfig;
	memset(&encodeConfig, 0, sizeof(EncodeConfig));

    encodeConfig.endFrameIdx = INT_MAX;
    encodeConfig.bitrate = 5000000;
    encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.deviceType = NV_ENC_CUDA;
    encodeConfig.codec = NV_ENC_H264;
    encodeConfig.fps = 30;
    encodeConfig.qp = 28;
    encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
    encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;
    encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
    encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET; 
    encodeConfig.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
    encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    encodeConfig.inputFormat = NV_ENC_BUFFER_FORMAT_ARGB;
	encodeConfig.height = HEIGHT_DEFAULT;
	encodeConfig.width  = WIDTH_DEFAULT;
	encodeConfig.codec = 0;

	result = m_pNvHWEncoder->Init_tcp_serv();
	if(result < 0){
		NvDebugLog("tcp serv init failed, result: %d", result);
		return -1;
	}

	__nv(InitCuda(encodeConfig.deviceID));
	__nv(m_pNvHWEncoder->Initialize(m_pDevice, NV_ENC_DEVICE_TYPE_CUDA));
    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);
	__nv(m_pNvHWEncoder->CreateEncoder(&encodeConfig));
	
	encodeConfig.maxWidth = encodeConfig.maxWidth ? encodeConfig.maxWidth : encodeConfig.width;
    encodeConfig.maxHeight = encodeConfig.maxHeight ? encodeConfig.maxHeight : encodeConfig.height;
    m_stEncoderInput.enableAsyncMode = encodeConfig.enableAsyncMode;
    m_uPicStruct = encodeConfig.pictureStruct;
	m_stEOSOutputBfr.bEOSFlag = TRUE;
    m_stEOSOutputBfr.hOutputEvent = NULL;
	NvDebugLog("init zc success.");

	return 0;
}

NVENCSTATUS CNvEncoder::RunMotionEstimationOnly(MEOnlyConfig *pMEOnly, bool bFlush)
{
    //uint8_t *pInputSurface = NULL;
    //uint8_t *pInputSurfaceCh = NULL;
    uint32_t lockedPitch = 0;
    //uint32_t dwSurfHeight = 0;
    static unsigned int dwCurWidth = 0;
    static unsigned int dwCurHeight = 0;
    //HRESULT hr = S_OK;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    MotionEstimationBuffer *pMEBuffer = NULL;

    if (bFlush)
    {
        FlushMVOutputBuffer();
        return NV_ENC_SUCCESS;
    }

    if (!pMEOnly)
    {
        assert(0);
        return NV_ENC_ERR_INVALID_PARAM;
    }

    pMEBuffer = m_MVBufferQueue.GetAvailable();
    if(!pMEBuffer)
    {
        m_pNvHWEncoder->ProcessMVOutput(m_MVBufferQueue.GetPending());
        pMEBuffer = m_MVBufferQueue.GetAvailable();
    }
    pMEBuffer->inputFrameIndex = pMEOnly->inputFrameIndex;
    pMEBuffer->referenceFrameIndex = pMEOnly->referenceFrameIndex;
    dwCurWidth = pMEOnly->width;
    dwCurHeight = pMEOnly->height;

    for (int i = 0; i < 2; i++)
    {
        unsigned char *pInputSurface = NULL;
        unsigned char *pInputSurfaceCh = NULL;
        nvStatus = m_pNvHWEncoder->NvEncLockInputBuffer(pMEBuffer->stInputBfr[i].hInputSurface, (void**)&pInputSurface, &lockedPitch);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        if (pMEBuffer->stInputBfr[i].bufferFmt == NV_ENC_BUFFER_FORMAT_NV12_PL)
        {
            pInputSurfaceCh = pInputSurface + (pMEBuffer->stInputBfr[i].dwHeight*lockedPitch);
            convertYUVpitchtoNV12(pMEOnly->yuv[i][0], pMEOnly->yuv[i][1], pMEOnly->yuv[i][2], pInputSurface, pInputSurfaceCh, dwCurWidth, dwCurHeight, dwCurWidth, lockedPitch);
        }
        else if (pMEBuffer->stInputBfr[i].bufferFmt == NV_ENC_BUFFER_FORMAT_YUV444)
        {
            unsigned char *pInputSurfaceCb = pInputSurface + (pMEBuffer->stInputBfr[i].dwHeight * lockedPitch);
            unsigned char *pInputSurfaceCr = pInputSurfaceCb + (pMEBuffer->stInputBfr[i].dwHeight * lockedPitch);
            convertYUVpitchtoYUV444(pMEOnly->yuv[i][0], pMEOnly->yuv[i][1], pMEOnly->yuv[i][2], pInputSurface, pInputSurfaceCb, pInputSurfaceCr, dwCurWidth, dwCurHeight, dwCurWidth, lockedPitch);
        }
        else if (pMEBuffer->stInputBfr[i].bufferFmt == NV_ENC_BUFFER_FORMAT_YUV420_10BIT)
        {
            unsigned char *pInputSurfaceCh = pInputSurface + (pMEBuffer->stInputBfr[i].dwHeight*lockedPitch);
            convertYUV10pitchtoP010PL((uint16_t *)pMEOnly->yuv[i][0], (uint16_t *)pMEOnly->yuv[i][1], (uint16_t *)pMEOnly->yuv[i][2], (uint16_t *)pInputSurface, (uint16_t *)pInputSurfaceCh, dwCurWidth, dwCurHeight, dwCurWidth, lockedPitch);
        }
        else
        {
            unsigned char *pInputSurfaceCb = pInputSurface + (pMEBuffer->stInputBfr[i].dwHeight * lockedPitch);
            unsigned char *pInputSurfaceCr = pInputSurfaceCb + (pMEBuffer->stInputBfr[i].dwHeight * lockedPitch);
            convertYUV10pitchtoYUV444((uint16_t *)pMEOnly->yuv[i][0], (uint16_t *)pMEOnly->yuv[i][1], (uint16_t *)pMEOnly->yuv[i][2], (uint16_t *)pInputSurface, (uint16_t *)pInputSurfaceCb, (uint16_t *)pInputSurfaceCr, dwCurWidth, dwCurHeight, dwCurWidth, lockedPitch);
        }
        nvStatus = m_pNvHWEncoder->NvEncUnlockInputBuffer(pMEBuffer->stInputBfr[i].hInputSurface);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
    }

    nvStatus = m_pNvHWEncoder->NvRunMotionEstimationOnly(pMEBuffer, pMEOnly);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        PRINTERR("nvEncRunMotionEstimationOnly error:0x%x\n", nvStatus);
        assert(0);
    }
    return nvStatus;

}

NVENCSTATUS CNvEncoder::EncodeFrame(const void* pData, bool bFlush, uint32_t width, uint32_t height)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    uint32_t lockedPitch = 0;
    EncodeBuffer *pEncodeBuffer = NULL;

    if (bFlush)
    {
        FlushEncoder();
        return NV_ENC_SUCCESS;
    }

    pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
    if(!pEncodeBuffer)
    {
        m_pNvHWEncoder->ProcessOutput(m_EncodeBufferQueue.GetPending());
        pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
    }

    unsigned char *pInputSurface;
  	m_pNvHWEncoder->NvEncLockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface, (void**)&pInputSurface, &lockedPitch);

	if(pEncodeBuffer->stInputBfr.bufferFmt == NV_ENC_BUFFER_FORMAT_ARGB)
	{
		convertBGRApitchtoARGB(pData, pInputSurface, width, height, width, lockedPitch);
	}
	else if(pEncodeBuffer->stInputBfr.bufferFmt == NV_ENC_BUFFER_FORMAT_ABGR)
	{
		convertBGRApitchtoABGR(pData, pInputSurface, width, height, width, lockedPitch);
	}
	
    m_pNvHWEncoder->NvEncUnlockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface);
	nvStatus = m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, width, height, (NV_ENC_PIC_STRUCT)m_uPicStruct);
	if(!bFlush)
		m_numFramesEncoded++;
	
	pEncodeBuffer = m_EncodeBufferQueue.GetPending();
    if(pEncodeBuffer)
    {
        m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
    }
    return nvStatus;
}

NVENCSTATUS CNvEncoder::EncodeFrameZC(const int id)
{
	static int write_cnt = 0;
	EncodeBufferPtr pEncodeBuffer = NULL;
	pEncodeBuffer = m_stEncodeBufferPtrArr[id];
	CCudaAutoLock cuLock(m_pDevice);
	uint32_t lockedPitch = 0;
	unsigned char *pInputSurface;
	int curWidth = pEncodeBuffer->stInputBfr.dwWidth;
	int curHeight = pEncodeBuffer->stInputBfr.dwHeight;
	
	switch(m_currentBufferType)
	{
		case NVENC_INPUT_BUFFER:
			//__nv(m_pNvHWEncoder->NvEncLockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface, (void**)&pInputSurface, &lockedPitch));
			//convertBGRApitchtoARGB(pEncodeBuffer->stInputBfr.pRGBHostPtr, pInputSurface, curWidth, curHeight, curWidth, lockedPitch);
#if 0
			if(m_numFramesEncoded % 30 == 0 && write_cnt < 500){
				fwrite(pEncodeBuffer->stInputBfr.pRGBHostPtr, curWidth*curHeight*4, 1, rgba);
				write_cnt++;
			}
#endif
			__nv(m_pNvHWEncoder->NvEncUnlockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface));
			__nv(m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, curWidth, curHeight, (NV_ENC_PIC_STRUCT)m_uPicStruct));
			__nv(m_pNvHWEncoder->ProcessOutput(pEncodeBuffer));
			__nv(m_pNvHWEncoder->NvEncLockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface, (void**)&pEncodeBuffer->stInputBfr.pRGBHostPtr, &lockedPitch));
			//NvLog("pRgbHostPtr: %p", pEncodeBuffer->stInputBfr.pRGBHostPtr);
			break;
		case CUDA_DEVICE_BUFFER:
			break;
		case CUDA_HOST_BUFFER:
			__nv( m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface));
		    __nv( m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, curWidth, curHeight));
			__nv( m_pNvHWEncoder->ProcessOutput(pEncodeBuffer));
			__nv( m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface));
			break;
		default:
			break;
	}
    
	m_numFramesEncoded++;
   	return NV_ENC_SUCCESS;
}

