
#pragma once

#include "carplate_main.h"
#include "parking_upload.h"
#include <sys/epoll.h>

using uint8 = uint8_t;


typedef struct _MsgHeader {
	short msgID;
	uint8 bodyLen;
	uint8 isMultiPack;  

	uint8 devNo[6];
	short serial;

	short packCount;
	short packIdx;
} MsgHeader;



void initBubiaoServer();
void findBestMatchMedia();
void combineLeftAndRight(ParkingCarInfo frontCarInfo, ParkingCarMedia frontCarMedia, vector<ParkingCarPair>& pairVec, int prob);
void uploadMedia(ParkingCarInfo carInfo, ParkingCarMedia carMedia);
void addWaterMark(string path, int camId, string spacecode);