#include "carplate_main.h"


static int iSock = 0;

static void* BB_SendThread(void* arg);
static void* BB_ReadThread(void* arg);
static void* BB_TimeThread(void* arg);
static int bbWorkTime = 0;
static int bbStartTime = 0;


volatile int netStatus = FALSE;
volatile int netAuthen = FALSE;

pthread_mutex_t media_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* DebugNet_Handler(void* arg);
static void CheckNet_Handle(int sig);
static void* Connect_Handler(void* arg);

static int retryTimes = 0;
static int RETRY_LIMIT = 5;

static int devNetFlag = 0;
int logFd = 0;
extern uint8 car_role;

char masterServerIP[30] = {0};
int slaveSock = 0; 
static void* slaveSocketThread(void* arg);

void BB_KeepAlive(void) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thDebug;
	pthread_create(&thDebug, &attr, DebugNet_Handler, NULL);

	pthread_t thConnect;
	pthread_create(&thConnect, &attr, Connect_Handler, NULL);
}

bool isBubiaoConnected() {
	return netStatus && netAuthen;
}

void initClientSocket() {
	printf("initClientSocket\n");
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t slaveClientTh;
	pthread_create(&slaveClientTh, &attr, slaveSocketThread, NULL);
}


static void* slaveSocketThread(void* arg) {
	slaveSock = socket(AF_INET, SOCK_STREAM, 0);
	if (slaveSock == -1) {
		perror("socket client");
		return;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	printf("slaveSocketThread masterServerIP=%s\n", masterServerIP);
	inet_pton(AF_INET, masterServerIP, &server_addr.sin_addr.s_addr);
	server_addr.sin_port = htons(SERVER_PORT);
	
	
	int ret = connect(slaveSock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret == -1) {
		printf("client connect error\n");
		return;
	}
	
	printf("slave client connect succ!!\n");
}

int sendSlaveData(uint8* data, int len) {
	if (slaveSock <= 0) {
		printf("invalid connect socket\n");
		return;
	}

	if (data[1] == 0x08 && data[2] == 0x01) {
		PRINT("[sendSlaveData]", "send media to master\n");
	} else {
		//showArray("send to Master: ", data, len);
	}
	
	return send(slaveSock, data, len, 0);
}


static void* Connect_Handler(void* arg) {
	printf("start connect handler\n");
	int ret = 0;
	signal(SIGPIPE, CheckNet_Handle);
	GsmstatFlag = 0;

	while (1) 
	{
		if (netStatus == FALSE) 
		{
			ret = BB_ConnectServer(BB_NET_IP, BB_NET_PORT);
			printf("connect Bubiao state: %d\n", ret);
			if (ret == TRUE) {
				netStatus = TRUE;
				GsmstatFlag = 1;
			}
		}
		sleep(3);
	}
}

static void CheckNet_Handle(int sig) {
	// ??????SIGPIPE?????????????????????????????????
	printf("CATCH SIGPIPE at %s, %d\n",__FILE__,__LINE__);
	sleep(5);

	if (car_role == 0) {
		initClientSocket();
	}
	
}

static void* DebugNet_Handler(void* arg) {
	while (1) {
		sleep(10);
	}
}

int BB_ConnectServer(char* url, int port) {
	int i = 0;
	// ??????????????????
	int retVal = 0, retFlag = 0;
	struct timeval timeo = { 3, 0 };
	struct sockaddr_in serv;
	struct ifreq interface;

#if 0
//	inet_pton(AF_INET, ip, &serv.sin_addr);
	struct hostent *hptr = gethostbyname(url);
	printf("urlurl = %s\n",url);
	if(hptr == NULL){
		return -EFAULT;
	}
	iSock = socket(AF_INET, SOCK_STREAM, 0);
	serv.sin_port = htons(port);
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = *((unsigned long*)hptr->h_addr_list[0]);
//	Acquire socket here ...
#endif

	unsigned long add = inet_addr(url); //??????????????????ipv4??????
	if(add == INADDR_NONE){
		printf("bb connect domain name\n");
		struct hostent *hptr = gethostbyname(url);
		printf("url = %s\n",url);
		if(hptr == NULL){
			return -EFAULT;
		}
		iSock = socket(AF_INET, SOCK_STREAM, 0);
		serv.sin_port = htons(port);
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = *((unsigned long*)hptr->h_addr_list[0]);
	}else{

		printf("bb connect ip addr: ");
		serv.sin_family = AF_INET;
		inet_pton(AF_INET, url, &serv.sin_addr);
		serv.sin_port = htons(port);
		iSock = socket(AF_INET, SOCK_STREAM, 0);
	}

	strncpy(interface.ifr_ifrn.ifrn_name, BB_NET_TYPE, IFNAMSIZ);


	if (setsockopt(iSock, SOL_SOCKET, SO_BINDTODEVICE, (char *) &interface,
			sizeof(interface)) < 0) {
		printf("SO_BINDTODEVICE failed-----\n");
		for (i = 0; i < 5; i++) {
			printf(" %s:%d------%s:bind %d\n", url, port, BB_NET_TYPE, i);
			sleep(1);
		}
		return -1;
	}
	setsockopt(iSock, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
	for (i = 0; i < 10; i++) {
		retFlag = 0;
		printf(" %s:%d------%s:connect %d\n", url, port, BB_NET_TYPE, i);
		retVal = connect(iSock, (struct sockaddr*) &serv, sizeof(serv));
		if (retVal != -1) {
			retFlag = 1;
			break;
		}
		sleep(5);
	}

	if (!retFlag) {
		printf("Server bb Connect Error!\n");
		savelog("Server bb Connect Error!\n");
		return -1;
	}
	printf("BuBiao Server Connect Success!\n");
	savelog("BuBiao Server Connect Success!\n");
	bbWorkTime = 0;
	bbStartTime = 0;
	// ????????????
	BB_Authen(SERVER_TYPE_BB);
	return 1;
}
int BB_DisconnectServer(void) {
	printf("BB_DisconnectServer");
	int ret = 0;
	ret = close(iSock);
	netStatus = FALSE;

	BB_KeepAlive();
	return ret;
}


// int BB_NetSendDataTest(uint8* uData, int iDataLen) {
// 	int ret = 0;
// 	ret = write(iSock, uData, iDataLen);
// 	return ret;
// }


static int BB_NetSendData(uint8* uData, int iDataLen) {
	int ret = 0;
	if (!(uData[1] == 0x08 && uData[2] == 0x01)) {
		//PRINT("", "");
		//showArray("socket real send: ", uData, iDataLen);
	}
	ret = send(iSock, uData, iDataLen, MSG_WAITALL);
	return ret;
}

static int BB_NetRecvData(uint8* uData) {
	int retVal = 0;

	struct timeval tv;
	tv.tv_sec = 1;		// 1S
	tv.tv_usec = 0;

	fd_set rdfds;
	FD_ZERO(&rdfds);
	FD_SET(iSock, &rdfds);	// ????????????-1

	retVal = select(iSock + 1, &rdfds, NULL, NULL, &tv);
	// ?????????
	if (retVal > 0) {
		retVal = read(iSock, uData, 1);
	}
	// ???????????????????????????
	return retVal;
}

void BB_RecSndServInit(void) {
	// ?????????????????????????????????
	//pthread_t thBBSnd;
	//pthread_create(&thBBSnd, NULL, BB_SendThread, NULL);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// ???????????????????????????
	pthread_t thBBRecv;
	pthread_create(&thBBRecv, &attr, BB_ReadThread, NULL);

	pthread_t thBBTime;
	pthread_create(&thBBTime, &attr, BB_TimeThread, NULL);
}


static void* BB_TimeThread(void* arg) {
	pthread_mutex_t *flag_mutex = (pthread_mutex_t*) arg;

	while (1) {
		bbWorkTime++;
		bbStartTime++;

		sleep(1);

		if (bbStartTime > OFFLINE_PERIOD) {
			bbStartTime = 0;
			netStatus = FALSE;

			// pthread_mutex_trylock(flag_mutex);
			printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_END));
			setMediaMsgFlag(STATE_END);  // interrupt send
			// pthread_mutex_unlock(flag_mutex);

			PRINT("", "connection timeout\n");
			if (iSock > 0) {
				close(iSock);
			}
		}
		if (bbWorkTime >= 5 * 60 *60) {
			bbWorkTime = 0;
			printf(" net Timeout\n");
		}

	}
}

static void* BB_ReadThread(void* arg) {
	uint8 c, ret = 0, headFlag = 0;
	uint8 uRdBuff[1024];
	int nread = 0;

	//printf("BB_ReadThread receive: ");
	while (1) {
		// ????????????????????????????????????????????????????????????
		if (netStatus  == FALSE) {
			usleep(10000);
			continue;
		}

		ret = BB_NetRecvData(&c);
		if (ret != 1) {
			usleep(10000);
			continue;
		}
		
		//printf("%x", c);

		// ???????????????????????????
		if (c == 0x7E && headFlag == 0) {
			headFlag = 1;
			// ??????????????????
			nread = 0;
			memset(uRdBuff, 0, sizeof(uRdBuff));
		}

		// ????????????????????????????????????????????? successfully
		if (headFlag) {
			uRdBuff[nread] = c;
			nread++;

			// ???????????????
			if (nread > 2 && c == 0x7e) {
				// ???????????????
				// ????????????7E????????????????????????
				BB_Parse(&uRdBuff[1], nread - 2, SERVER_TYPE_BB);

				//printf("Bubiao parse here.............\n");
				bbWorkTime = 0;
				bbStartTime = 0;
#if 0
				if(1){
					printf("receive:\n");
					int i;
					for (i = 0; i < nread; i++) {
						printf("%02x ", uRdBuff[i]);
					}
					printf("\n");
				}
#endif
				// ??????????????????
				nread = 0;
				headFlag = 0;
				memset(uRdBuff, 0, sizeof(uRdBuff));
			}
		}

		if (nread > 1023) {
			// ??????????????????
			nread = 0;
			memset(uRdBuff, 0, sizeof(uRdBuff));
		}

	}
	printf("\n");
	return NULL;
}


static uint8 dstData[10240];
#if 0
static void* BB_SendThread(void* arg) {
	stBBSendData stBBSndData;
	int ret;
	while (1) 
	{
		// ????????????????????????????????????????????????????????????
		if (netStatus  == FALSE) {
			usleep(10000);
			continue;
		}
		// ??????????????????????????????netStatus

		ret = stSysIpc.GetAsyncMsg(BB_ASYNC_KEY, &stBBSndData, BB_ASYNC_SIZE);
		if (ret > 0) 
		{
			// 3G????????????
			ret = BB_NetSendData(stBBSndData.message, stBBSndData.iDataLen);
			if( (stBBSndData.message[1]==0x08)&&(stBBSndData.message[2]==0x01))
			{
				memset(dstData, 0, 1024*10);
				int iDataLen = ReEscapeData(dstData, stBBSndData.message, stBBSndData.iDataLen);
				printf("socket send: %d/%d\n",dstData[16],dstData[14]);

			}

			if (ret < 0) 
			{
				// ?????????????????????
				netStatus = FALSE;
				netAuthen = FALSE;
				printf("Server disconnet\n");
			}
		}
		usleep(10000);
	}
	return NULL;
}
#endif

uint8 DecToBCD(uint8 dec) {
	return ((dec / 10) * 16 + dec % 10);
}

uint8 BCDToDec(uint8 bcd) {
	return ((bcd / 16) * 10 + bcd % 16);
}

int hex2byte(uint8 *dst, const char *src) {

	int i = 0;
	while (*src) {
		if (' ' == *src) {
			src++;
			continue;
		}
		sscanf(src, "%02X", dst);
		src += 2;
		dst++;
		i++;
	}
	if (GPS_LEN != i) {
		return -1;
	}
	return 0;
}

void IntToDWord(uint8* dst, int src) {
	dst[0] = src / (256 * 256 * 256);
	src = src % (256 * 256 * 256);
	dst[1] = src / (256 * 256);
	src = src % (256 * 256);
	dst[2] = src / (256);
	src = src % (256);
	dst[3] = src;
}

// ?????????????????????????????????????????????
void StrToDWord(char* cSrc, uint8* dstData) {
	int iTemp = 0;
	iTemp = atoi(cSrc);

	dstData[0] = iTemp / (256 * 256 * 256);
	iTemp = iTemp % (256 * 256 * 256);

	dstData[1] = iTemp / (256 * 256);
	iTemp = iTemp % (256 * 256);

	dstData[2] = iTemp / 256;
	dstData[3] = iTemp % 256;

	return;
}

// ?????????????????????????????????????????????
void StrToWord(char* cSrc, uint8* dstData) {
	int iTemp = 0;
	iTemp = atoi(cSrc);

	dstData[0] = iTemp / 256;
	dstData[1] = iTemp % 256;

	return;
}

// ????????????????????????256
uint8 StrToU8(char* cSrc) {
	int iTemp = 0;
	iTemp = atoi(cSrc);
	return ((uint8) iTemp);
}

// ??????sum
uint8 CheckSum(uint8* buff, int length) {
	int i;
	uint8 sum = 0;

	for (i = 0; i < length; i++) {
		sum ^= buff[i];
	}

	return sum;
}

//????????????
int EscapeData(uint8 *dst, uint8* source, int length) {
	uint8 addNum = 0;
	int index = 0;

	for (index = 0; index < length; index++) {
		if (source[index] == 0x7d) {
			dst[index + addNum] = 0x7d;
			addNum += 1;
			dst[index + addNum] = 0x01;
		} else if (source[index] == 0x7e) {
			dst[index + addNum] = 0x7d;
			addNum += 1;
			dst[index + addNum] = 0x02;
		} else {
			dst[index + addNum] = source[index];
		}
	}

	return length + addNum;
}

// ??????????????????
int ReEscapeData(uint8 *dst, uint8* source, int length) {
	uint8 subNum = 0;
	int index = 0;
	for (index = 0; index < length; index++) {
		if (source[index] == 0x7d) {
			if (source[index + 1] == 0x01) {
				dst[index - subNum] = 0x7d;
				subNum++;
				index++;
			} else if (source[index + 1] == 0x02) {
				dst[index - subNum] = 0x7e;
				subNum++;
				index++;
			}
		} else {
			dst[index - subNum] = source[index];
		}
	}
	return length - subNum;
}

int BB_SendData(uint8* data, int dataLen) {
	int ret;
	stBBSendData sBBSndData;
	sBBSndData.msgType = 0x01;
	sBBSndData.iDataLen = dataLen;
	memcpy(sBBSndData.message, data, sBBSndData.iDataLen);
#if 0
	int ret = stSysIpc.SndAsyncMsg(BB_ASYNC_KEY, &sBBSndData, BB_ASYNC_SIZE);
	if(ret != 0)
	{
		printf("ret=%d\n",ret);
	}
	//BB_NetSendDataTest(data, sBBSndData.iDataLen);
#endif
	ret = BB_NetSendData(sBBSndData.message, sBBSndData.iDataLen);
	char dataBuffer[1024 * 10] = {0};
	if(sBBSndData.message[1] == 0x08 && sBBSndData.message[2] == 0x01)
	{
		// memset(dstData, 0, 1024*10);
		int iDataLen = ReEscapeData(dataBuffer, sBBSndData.message, sBBSndData.iDataLen);
		PRINT("BB_SendData", "socket send: %d/%d, dataLen: %d\n", dataBuffer[16], dataBuffer[14], iDataLen);

	}

	if (ret < 0) {
		PRINT("BB_SendData", "send len < 0\n");
	}
	// if (ret < 0) // ?????????????????????
	// {
	// 	netStatus = FALSE;
	// 	netAuthen = FALSE;
	// 	PRINT("", "Server disconnet\n");
	// 	savelog("Server disconnet\n");
	// }

	return ret;
}

static void GetPhonenum(uint8* dst) {
	memcpy(dst, devSeriNum, 6);
}

static int iSeriCnt = 0;
static int iMediaSeriCnt = 0xF000;
static void GetSerinum(uint8* dst, int flag) {
	if (!flag) {
		iSeriCnt++;
		if (iSeriCnt > 0xF000)
			iSeriCnt = 0;

		dst[0] = iSeriCnt / 256;
		dst[1] = iSeriCnt % 256;
	} else {
		iMediaSeriCnt++;
		if (iMediaSeriCnt > 0xFFFF)
			iMediaSeriCnt = 0xF000;

		dst[0] = iMediaSeriCnt / 256;
		dst[1] = iMediaSeriCnt % 256;
	}
}

bool checkBufferValidation(uint8* fileDataBuff, int fileDataLen) {
	for (int i = 0; i < fileDataLen; i++) {
		int count = 0;
		while (i < fileDataLen && fileDataBuff[i++] == 0x00) {
			count++;

			if (count > 200) {
				return false;
			}
		}
	}
	return true;
}

void BB_SendCommMsg(stBBCommMsg* stBBMsg) {
	uint8 srcData[BB_ASYNC_SIZE], dstData[BB_ASYNC_SIZE];
	int iDataLen;
	iDataLen = stBBMsg->dataLen;
	// 0~1??? ??? MSGID
	memcpy(&srcData, stBBMsg->msgID, 2);
	/*????????????, ?????????????????????*/
	srcData[2] = iDataLen / 256;
	srcData[3] = iDataLen % 256;

	/*????????????*/
	GetPhonenum(&srcData[4]);

	/*????????????????????????*/
	if (stBBMsg->headFlag) {

		/*???????????????*/
		GetSerinum(&srcData[10], stBBMsg->headFlag);
		iDataLen += 16;
		iDataLen -= 8192;
		memcpy(&srcData[12], stBBMsg->packMsg, 4);
		memcpy(&srcData[16], stBBMsg->data, iDataLen);

	} else {
		/*???????????????*/
		GetSerinum(&srcData[10], stBBMsg->headFlag);
		memcpy(&srcData[12], stBBMsg->data, iDataLen);
		iDataLen += 12;
	}

	/*???????????????*/
	srcData[iDataLen] = CheckSum(srcData, iDataLen);
	iDataLen += 1;

	/*??????*/
	iDataLen = EscapeData(&dstData[1], srcData, iDataLen);
	iDataLen += 1;

	/*?????????????????????*/
	dstData[0] = 0x7e;
	dstData[iDataLen] = 0x7e;
	iDataLen += 1;

	if (car_role == 0) {
		sendSlaveData(dstData, iDataLen);
	} else if (car_role == 1) {
		relayBuBiaoData(dstData, iDataLen);
	}


	if (!checkBufferValidation(dstData, iDataLen)) {
		showArray("invalid buffer: ", dstData, 1000);
		printf("not valid buffer\n");

		// pthread_mutex_trylock(&media_flag_mutex);
		printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_END));
		setMediaMsgFlag(STATE_END);  // interrupt send
		// pthread_mutex_unlock(&media_flag_mutex);
		return;
	}

	int ret = BB_SendData(dstData, iDataLen);
	if (ret < 0) {
		PRINT("BB_SendCommMsg", "send data failed\n");
	}

#if 0
	if(iDataLen < 512){

		int i;
		for (i = 0; i < iDataLen; i++) {
			if (i > 0 && i % 25 == 0) {
				printf("\n");
			}
			printf("%02x ", dstData[i]);
		}
		printf("\n");
	}
#endif

}



char fileName[5000][256];
// ??????????????????????????????????????????????????????
int ReadDir(char* dirName) {
	int fileNum = 0;
	//  ???????????????????????????????????? 0
	if (dirName == NULL)
		return 0;

	// ??????????????????????????????????????????0
	DIR* dir = opendir(dirName);
	if (dir == NULL)
		return 0;

	struct dirent *file;
	while ((file = readdir(dir)) != NULL) {
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		strcpy(fileName[fileNum++], file->d_name);
	}

	return fileNum;
}

int ReadFileEnd(char* fileName, int offset, int dataSize, uint8* dst) {
	int readNum = 0;

	FILE* file = NULL;
	file = fopen(fileName, "r");
	if (file == NULL)
		return readNum;

	fseek(file, offset, SEEK_END);
	readNum = fread(dst, 1, dataSize, file);
	fclose(file);

	return readNum;
}

void savelog(const char *src)
{
	uint8 logBuff[256]={0};
	uint8 gpsTime[6]={0};
	time_t timep;
	struct tm *p_time;
	time(&timep);
	p_time = gmtime(&timep);		
	gpsTime[0] = bin2bcd(p_time->tm_year-100);
	gpsTime[1] = bin2bcd(1+p_time->tm_mon);
	gpsTime[2] = bin2bcd(p_time->tm_mday);
	gpsTime[3] = bin2bcd(p_time->tm_hour+8);
	gpsTime[4] = bin2bcd(p_time->tm_min);
	gpsTime[5] = bin2bcd(p_time->tm_sec);
	
	logFd = open("/mnt/sdcard/network.log",O_RDWR | O_CREAT | O_APPEND);
	if(logFd<0)
	{
		printf("logFd open filed....\n");
		exit(-1);
	}
	
	if(0x22 == gpsTime[0])
	{
		sprintf(logBuff,"[20%02x-%02x-%02x:%02x:%02x:%02x]:%s",\
		gpsTime[0],gpsTime[1],gpsTime[2],\
		gpsTime[3],gpsTime[4],gpsTime[5],src);		
	}
	write(logFd,logBuff,strlen(logBuff));
	close(logFd);
}
