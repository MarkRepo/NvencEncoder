#ifndef _H264_LOG_H_
#define _H264_LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

//#define H264_DEBUGGING

extern char gLogFileName[];

void 		H264_VLog(const char* file, int line, const char *format, va_list args);
void 		H264_Log(const char* file, int line, const char *format, ...);
void 		H264_DebugLog(const char* file, int line, const char *format, ...);
void 		H264_LogInitFile(char *fileName);
void 		H264_LogCloseFile();

#define NvLog(format, args...) 			H264_Log(__FILE__, __LINE__, format, ##args)

#ifdef H264_DEBUGGING
#define NvDebugLog(format, args...)      H264_DebugLog(__FILE__, __LINE__, format, ##args)
#else
#define NvDebugLog(format, args...)
#endif

#endif