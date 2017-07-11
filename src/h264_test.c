#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "h264_route.h"
#include "nvFileIO.h"

int main(int argc, char** argv)
{
	char* filename = argv[1];
	HANDLE hInput;
	DWORD  filesize;
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(struct sockaddr_in));
	inet_pton(AF_INET, "192.168.2.50", &servaddr.sin_addr);
	uint32_t sip = ntohl(servaddr.sin_addr.s_addr);
	uint16_t sport = 8000;
	int totalFrames = 0, width = 0, height = 0;
	sscanf(argv[2], "%d", &width);
	sscanf(argv[3], "%d",  &height);
	hInput = nvOpenFile(filename);
    if (hInput == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open \"%s\"\n", filename);
        return 1;
    }
	
	nvGetFileSize(hInput,&filesize);
	totalFrames = filesize/(width*height*4);
	
	h264_init(sip, sport, 0);

	int frmIdx = 0;
	pixman_image_t pImage;
	for(frmIdx = 0; frmIdx < totalFrames; frmIdx++){
		
	}
	
	return 0;
}
