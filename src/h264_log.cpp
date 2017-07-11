#include "h264_log.h"

static FILE *gLogFile = NULL;

char gLogFileName[] = "/var/log/h264_route.log";

void H264_LogInitFile(char *fileName) {
	if(fileName == NULL) {
		printf("log filename is NULL!\n");
		exit(1);
	}
	if((gLogFile = fopen(fileName, "w")) == NULL) {
		printf("open log file error!\n");
		exit(1);
	}
}

void H264_LogCloseFile() 
{
	fclose(gLogFile);
}

void H264_VLog(const char* file, int line, const char *format, va_list args) {
	char *logStr = NULL;	
	if(format == NULL) 
		return;
	logStr = (char*)malloc(strlen(format) + 128);	
	if(logStr)
		sprintf(logStr, "[FILE: %s, LINE: %d]\t\t %s\n", file, line, format);

	if(gLogFile != NULL) {
		char fileLine[256];
		vsnprintf(fileLine, 255, logStr, args);
		fwrite(fileLine, strlen(fileLine), 1, gLogFile);
		fflush(gLogFile);
	}
	
	free(logStr);	
}

void H264_Log(const char* file, int line, const char *format, ...) {
	va_list args;
	va_start(args, format);
	H264_VLog(file, line, format, args);
	va_end(args);
}

void H264_DebugLog(const char* file, int line, const char *format, ...) {
#ifdef H264_DEBUGGING
	va_list args;
	va_start(args, format);
	H264_VLog(file, line, format, args);
	va_end(args);
#endif
}

