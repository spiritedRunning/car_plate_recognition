#include "carplate_main.h"


stBBAUFile BBUpInfo[AU_FILE_NUM];

static void* LocTaskThread(void* arg);
static void* LocWorkThread(void* arg);


void BB_LocTaskStart(void) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thLoc;
	pthread_create(&thLoc, &attr, LocTaskThread, NULL);

	pthread_t thWork;
	pthread_create(&thWork, &attr, LocWorkThread, NULL);

}

static void* LocWorkThread(void* arg) {
	static int locWorkTime = 0;
	uint8 last_alarmflag = 0;
	while (1) {
		if (netStatus  == FALSE) {
			printf("netStatus: %d\n", netStatus);
			sleep(1);
			continue;
		}

		if (netAuthen  == FALSE) {
			printf("netAuthen: %d\n", netAuthen);
			sleep(1);
			continue;
		}

		locWorkTime = (locWorkTime + 1) % 0xFFFF;
		if (locWorkTime % LOCATION_INTVERVAL == 0 || last_alarmflag != alarmflag) {
			if(last_alarmflag != alarmflag)
			{
				//printf(">>>>>>>>>>>>>Trigger activation code failure alarm\n");
				pGpsInfo->extraA5Msg[0] = 0xA5;
				pGpsInfo->extraA5Msg[1] = 0x06;
				pGpsInfo->extraA5Msg[4] &= 0x00;
				pGpsInfo->extraA5Msg[4] |= alarmflag;
				last_alarmflag = alarmflag;
			}
			BB_LocMsgReply(NULL, NULL);
		}
		sleep(1);
	}

}

static void* LocTaskThread(void* arg) {

	while (TRUE) {

		if (netStatus == FALSE) {
			usleep(200000);
			continue;
		}
		if (netAuthen == FALSE) {
			usleep(200000);
			continue;
		}

		usleep(500000);
	}
	return NULL;
}

void BB_LocMsgReply(uint8* uSeri, stGpsInformat* stGpsInfo)
{
	//printf("Bubiao location upload\n");
	stBBCommMsg sBBMsg;
	sBBMsg.headFlag = 0;

	if (stGpsInfo == NULL) {
		stGpsInfo = pGpsInfo;
	}
	sBBMsg.msgID[0] = 0x02;
	sBBMsg.msgID[1] = 0x00;
	sBBMsg.dataLen = 28 + 8;
	memset(sBBMsg.data, 0, 1024);
	memcpy(sBBMsg.data, stGpsInfo, sizeof(stGpsInformat));

	// PRINT("location upload: ", "02 00 ");
	// showArray("", sBBMsg.data, sBBMsg.dataLen);
	PRINT("", "location upload\n");
	BB_SendCommMsg(&sBBMsg);
}


