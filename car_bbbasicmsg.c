#include "carplate_main.h"
#include "util/util_common.h"



static unsigned int KeyIDTable[256];
static void BB_InitKeyID(void);
unsigned int BB_GetKeyID(uint8* ID);

volatile int mediaMsgFlag = 0x00;
static stGpsInformat mediaGpsInfo = {0};
static stBBCommMsg stBBMsg;
static int packageTotal = 0;
static int packageCnt = 0;
static int pFileData = 0;
static unsigned int mediaID = 10000;

static void* SendPhotoMsg(void* arg);


static void IntToDWord(volatile uint8* dst, int src) {
	dst[0] = src / (256 * 256 * 256);
	src = src % (256 * 256 * 256);
	dst[1] = src / (256 * 256);
	src = src % (256 * 256);
	dst[2] = src / (256);
	src = src % (256);
	dst[3] = src;
}

static void IntToWord(volatile uint8* dst, int src) {
	dst[0] = src / (256);
	src = src % (256);
	dst[1] = src;
}


static void shell(const char* command, char* buffer) {
	FILE *fpRead;
	fpRead = popen(command, "r");
	char buf[256];
	memset(buf, 0, sizeof(buf));

	fgets(buf, sizeof(buf), fpRead);

	if (fpRead != NULL) {
		pclose(fpRead);
	}
	memcpy(&buffer[0], &buf[0], strlen(buf));
}

// 终端注册消息
void BB_TermRegister(void) {
	printf("Term Register!\n\n");

	const uint8 makerID[] = { 0x02, 0x00, 0x01, 0x03, 0xFF };
	const uint8 termModel[] = "TSINGTECK HJWS";
	const uint8 proviceID[2];
	const uint8 cityID[2];

	uint8 buffer[50] = { 0X00 };
	stBBCommMsg stBBMsg;

	//不进行分包发送
	stBBMsg.headFlag = 0;

	// 消息ID信息
	stBBMsg.msgID[0] = 0x01;
	stBBMsg.msgID[1] = 0x00;

	// 包数据长度
	stBBMsg.dataLen = 37 + 11;

	//地理信息：省域ID，
	memcpy(&stBBMsg.data[0], proviceID, 2);
	// 市县域ID
	memcpy(&stBBMsg.data[2], cityID, 2);

	memcpy(&stBBMsg.data[4], makerID, 5);
	memcpy(&stBBMsg.data[9], termModel, 20);
	memcpy(&stBBMsg.data[29], buffer, 7);
	stBBMsg.data[36] = 0;

	memcpy(&stBBMsg.data[37], buffer, 11);

	BB_SendCommMsg(&stBBMsg);
}

// 终端鉴权
void BB_Authen(int type) {
	uint8 tmpData[30];
	char authFile[50];
	memset(authFile, 0, sizeof(authFile));

	if (type == SERVER_TYPE_BB) {
		strcpy(authFile, BB_AUTHEN_FILE);
	} else {
		strcpy(authFile, TM_AUTHEN_FILE);
	}
	printf("authFile: %s\n", authFile);
	// 鉴权码长度
	BB_ReadFileData(authFile, 0, 1, tmpData);
	// 鉴权码
	BB_ReadFileData(authFile, 1, tmpData[0], &tmpData[1]);

	stBBCommMsg sBBMsg;
	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x01;
	sBBMsg.msgID[1] = 0x02;

	sBBMsg.dataLen = tmpData[0];
	memcpy(sBBMsg.data, &tmpData[1], tmpData[0]);

	printf("try to authen\n");
	PRINT("BB_Authen", " 01 02 ");
	showArray("", sBBMsg.data, sBBMsg.dataLen);
	BB_SendCommMsg(&sBBMsg);
	return;
}

// 终端心跳
void BB_TermBreath(void) {
	stBBCommMsg sBBMsg;

	sBBMsg.dataLen = 0;
	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x00;
	sBBMsg.msgID[1] = 0x02;

	BB_SendCommMsg(&sBBMsg);
	return;
}

//  平台通用应答
void BB_CommReply(uint8* pCMD, uint8* pSeri, uint8 uResult) {
	stBBCommMsg sBBMsg;

	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x00;
	sBBMsg.msgID[1] = 0x01;

	sBBMsg.dataLen = 5;
	memcpy(&sBBMsg.data[0], pSeri, 2);
	memcpy(&sBBMsg.data[2], pCMD, 2);
	sBBMsg.data[4] = uResult;

	BB_SendCommMsg(&sBBMsg);
	return;
}

void BB_roadsecReply() {
	stBBCommMsg sBBMsg;

	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x51;
	sBBMsg.msgID[1] = 0x02;

	sBBMsg.dataLen = 2+ 38*roadnum;
	sBBMsg.data[0] = 0;
	sBBMsg.data[1] = (uint8)roadnum;
	for(int i=0;i<roadnum;i++)
	{
		IntToDWord(&sBBMsg.data[2+38*i],roadID[i]);
		sBBMsg.data[6+38*i]=0;
		sBBMsg.data[7+38*i]=32;
		//printf("the BB_roadsecReply  md5 is %s\n",&roadMD5[i]);
		memcpy(&sBBMsg.data[8+38*i], &roadMD5[i], 32);
	}
	
	BB_SendCommMsg(&sBBMsg);
	return;
}

const char* getStateDescription(int state) {
	switch (state)
	{
	case 0:
		return "state_end";
	case 1:
		return "state_sending";
	case 2:
		return "state_multi";

	default:
		break;
	}
}

unsigned int BB_PLATEUPLOAD(uint8* carplate,uint8 carlen,uint8 carconfidence,int type,int color,uint8 *portnumber,uint8 portlen,
							uint8 *areaid,stGpsInformat *pGpsInfo,char * path) {
	while (mediaMsgFlag != 0) {
		usleep(100000);
	}
	printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_SENDING));
	setMediaMsgFlag(STATE_SENDING);
	memcpy(&mediaGpsInfo,pGpsInfo,sizeof(stGpsInformat));

	fileDataLen = readDataFromFile(path, fileDataBuff);
	//fileDataLen = BB_ReadFileData(path, 0, sizeof(fileDataBuff),fileDataBuff);
	printf("BB_PLATEUPLOAD upload path: %s, dataLen = %d\n", path, fileDataLen);
	if (fileDataLen < 20) {
		printf("[BB_PLATEUPLOAD]: invalid file\n");
		return -1;
	}

	//unsigned int mediaID = 10000;
	uint8 carlenget = carlen; 
	stBBCommMsg sBBMsg;
	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x51;
	sBBMsg.msgID[1] = 0x03;
	if(carlen == 0xFF)
	{
		carlenget=0;
	}
	sBBMsg.dataLen = 34+carlenget+portlen;
	sBBMsg.data[0] = 0;		
	sBBMsg.data[1] = cameraType+1;						
	sBBMsg.data[2] = 0;
	IntToWord(&sBBMsg.data[3],color);
	IntToWord(&sBBMsg.data[5],type);
	sBBMsg.data[7] = carlen;	
	memcpy(&sBBMsg.data[8], carplate, carlenget);


	IntToDWord(&sBBMsg.data[8+carlenget],atoi(areaid));

	sBBMsg.data[12+carlenget] = portlen;

	memcpy(&sBBMsg.data[13+carlenget], portnumber, portlen);
	sBBMsg.data[13+carlenget+portlen] = 00;
	sBBMsg.data[14+carlenget+portlen] = carconfidence;
	memcpy(&sBBMsg.data[15+carlenget+portlen], mediaGpsInfo.bbLat, 4);
	memcpy(&sBBMsg.data[19+carlenget+portlen], mediaGpsInfo.bbLng, 4);
	memcpy(&sBBMsg.data[23+carlenget+portlen], mediaGpsInfo.gpsTime, 6);
	sBBMsg.data[29+carlenget+portlen] = 1;
	mediaID = BB_GetKeyID(&sBBMsg.data[30+carlenget+portlen]);
	//printf("=====debug============the id0 = %x,id1 = %x,id2 = %x,id3 = %x\n",sBBMsg.data[26+carlen],sBBMsg.data[27+carlen],sBBMsg.data[28+carlen],sBBMsg.data[29+carlen]);
	BB_SendCommMsg(&sBBMsg);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	actionCode = 0x51;
	pthread_t thMedia;
	pthread_create(&thMedia, &attr, SendPhotoMsg, NULL);
	
	return 0;
}

unsigned int BB_PhotoUpload(char * path){
	while (mediaMsgFlag != 0) {
		usleep(100000);
	}
	printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_SENDING));
	setMediaMsgFlag(STATE_SENDING);
	memcpy(&mediaGpsInfo,pGpsInfo,sizeof(stGpsInformat));
	
	stBBCommMsg sBBMsg;
	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x08;
	sBBMsg.msgID[1] = 0x05;
	sBBMsg.dataLen = 9;
	sBBMsg.data[0] = takePhotoSeriNo / 256;
	sBBMsg.data[1] = takePhotoSeriNo % 256;
	sBBMsg.data[2] = 0;
	sBBMsg.data[3] = 0;
	sBBMsg.data[4] = 1;
	mediaID = BB_GetKeyID(&sBBMsg.data[5]);
	
	BB_SendCommMsg(&sBBMsg);

	// fileDataLen = BB_ReadFileData(path, 0, sizeof(fileDataBuff),fileDataBuff);
	fileDataLen = readDataFromFile(path, fileDataBuff);
	if (fileDataLen < 20) {
		printf("invalid file\n");
		return -1;
	}
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	actionCode = 0;
	pthread_t thMedia;
	pthread_create(&thMedia, &attr, SendPhotoMsg, NULL);
	return 0;
}

static void BB_InitKeyID(void) {
	unsigned int c;
	unsigned int i, j;
	for (i = 0; i < 256; i++) {
		c = (unsigned int) i;
		for (j = 0; j < 8; j++) {
			if (c & 1)
				c = (0xedb88320L+rand()) ^ (c >> 1);
			else
				c = c >> 1;
		}
		KeyIDTable[i] = c;
	}
}

unsigned int BB_GetKeyID(uint8* ID) {
	static time_t now;
	static struct tm *timeNow;
	time(&now);
	timeNow = localtime(&now);
	uint8 locTime[6];
	unsigned int i;
	unsigned int key;
	memset(locTime, 0, sizeof(locTime));
	BB_InitKeyID();
	locTime[0] = (timeNow->tm_year - 100) / 10 * 16
			+ (timeNow->tm_year - 100) % 10;
	locTime[1] = (timeNow->tm_mon + 1) / 10 * 16 + (timeNow->tm_mon + 1) % 10;
	locTime[2] = timeNow->tm_mday / 10 * 16 + timeNow->tm_mday % 10;
	locTime[3] = timeNow->tm_hour / 10 * 16 + timeNow->tm_hour % 10;
	locTime[4] = timeNow->tm_min / 10 * 16 + timeNow->tm_min % 10;
	locTime[5] = timeNow->tm_sec / 10 * 16 + timeNow->tm_sec % 10;
	unsigned int size = sizeof(locTime);
	for (i = 0; i < size; i++) {
		key = KeyIDTable[(key ^ locTime[i]) & 0xff] ^ (key >> 8);
	}
	ID[0] = 0xff & key;
	ID[1] = 0xff & (key >> 8);
	ID[2] = 0xff & (key >> 16);
	ID[3] = 0xff & (key >> 24);
	return key;
}

static void* SendPhotoMsg(void* arg) {

	//unsigned int mediaID = *(int*) arg;

	stBBMsg.msgID[0] = 0x08;
	stBBMsg.msgID[1] = 0x01;

	stBBMsg.dataLen = 36;

	stBBMsg.data[0] = 0xff & mediaID;
	stBBMsg.data[1] = 0xff & (mediaID >> 8);
	stBBMsg.data[2] = 0xff & (mediaID >> 16);
	stBBMsg.data[3] = 0xff & (mediaID >> 24);
	//printf("=====debug============the id0 = %x,id1 = %x,id2 = %x,id3 = %x\n",stBBMsg.data[0],stBBMsg.data[1],stBBMsg.data[2],stBBMsg.data[3]);
	//IntToDWord(stBBMsg.data, mediaID);					// 4个字节的mediaID
	stBBMsg.data[4] = 0x00;								// 上传图像类型为图片
	stBBMsg.data[5] = 0x00;								// 图片格式为 JPEG
	stBBMsg.data[6] = actionCode;								// 平台下发的指令
	stBBMsg.data[7] = cameraType+1;
	memcpy(&stBBMsg.data[8], &mediaGpsInfo, 28);	// 复制位置信息

	packageCnt = 0;
	stBBCommMsg* pBBMsg = &stBBMsg;

	printf("----------SendPhotoMsg...........\n");

	system("date");
	while (mediaMsgFlag != 0) {
		// 如果网络没有链接，则不进行数据发送
		if (netStatus  == FALSE) {
			usleep(10000);
			packageCnt = 0;
			printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_END));
			setMediaMsgFlag(STATE_END);
			continue;
		}

		if (mediaMsgFlag == 0x01) {
			/* 当读取的数据长度小于或者等于剩余数据长度时，不分包 */
			if (fileDataLen <= (BB_PKG_SIZE - pBBMsg->dataLen)) {
				//system("date");
				pBBMsg->headFlag = 0;
				/* 将读取的数据复制到传输结构体中 */
				memcpy(&pBBMsg->data[pBBMsg->dataLen], fileDataBuff, fileDataLen);
				pBBMsg->dataLen += fileDataLen;
				/* 结束发送循环 */
				printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_END));
				setMediaMsgFlag(STATE_END);
			} else { /* 第一包发现，需要进行分包发送。 */
				pBBMsg->headFlag = 1;
				// 总字节数/PCK_LENGTH，如果有余数，则包增加1
				packageTotal = (fileDataLen + pBBMsg->dataLen) / BB_PKG_SIZE;
				if ((fileDataLen + pBBMsg->dataLen) % BB_PKG_SIZE != 0) {
					packageTotal++;
				}

				packageCnt++;
				pBBMsg->packMsg[0] = packageTotal / 256;
				pBBMsg->packMsg[1] = packageTotal % 256;
				pBBMsg->packMsg[2] = packageCnt / 256;
				pBBMsg->packMsg[3] = packageCnt % 256;

				/* 复制数据并重写数据长度 */
				memcpy(&pBBMsg->data[pBBMsg->dataLen], fileDataBuff, BB_PKG_SIZE - pBBMsg->dataLen);

				/* BUFF 数据指针 */
				pFileData = BB_PKG_SIZE - pBBMsg->dataLen;
				pBBMsg->dataLen = BB_PKG_SIZE + 8192;
				/* 下次循环进入分包发送阶段 */
				printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_MULTI));
				setMediaMsgFlag(STATE_MULTI);
			}
		} else if (mediaMsgFlag == 0x02) { /* 进入分包发送阶段 */
			pBBMsg->headFlag = 1;
			packageCnt++;
			pBBMsg->packMsg[0] = packageTotal / 256;
			pBBMsg->packMsg[1] = packageTotal % 256;
			pBBMsg->packMsg[2] = packageCnt / 256;
			pBBMsg->packMsg[3] = packageCnt % 256;

			if (packageCnt % 100 == 0) {
				printf("%d/%d\n", packageCnt, packageTotal);
			}

			if (packageTotal > packageCnt) {
				pBBMsg->dataLen = BB_PKG_SIZE + 8192;	// 8192标记为分包选项
				memcpy(pBBMsg->data, &fileDataBuff[pFileData], BB_PKG_SIZE);
				pFileData += BB_PKG_SIZE;
			} else {
				//system("date");

				pBBMsg->dataLen = fileDataLen - pFileData + 8192;
				memcpy(pBBMsg->data, &fileDataBuff[pFileData], fileDataLen - pFileData);

				/* 结束发送 */
				printf("%s %d setMediaFlag: %s\n", __FILE__, __LINE__, getStateDescription(STATE_END));
				setMediaMsgFlag(STATE_END);
			}
		}

		//printf("---------sendPhotoMsg loop--------------\n");
		BB_SendCommMsg(pBBMsg);
		usleep(50000);
	}

	return NULL;
}

void setMediaMsgFlag(int flag) {
	mediaMsgFlag = flag;
}


/* 系统版本信息上传 */
#define versionNum 1
const char version[versionNum][128]={{VERSION_CARPLATE}};

void BB_VersionMsg(void) {
	PRINT("BB_VersionMsg", "upload version info\n");
    char buffer[1024] = {0};
	stBBCommMsg sBBMsg;
	shell("cat /root/app/readme |head -n 1", buffer);
	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x0F;
	sBBMsg.msgID[1] = 0x10;

	sBBMsg.dataLen = versionNum*32 + 2 + 32;
	memset(sBBMsg.data, 0, sizeof(sBBMsg.data));
	sBBMsg.data[0] = 0;
	sBBMsg.data[1] = versionNum + 1;
	memcpy((char*)&sBBMsg.data[2],buffer,20);
	for(int i=0;i<versionNum;i++)
	{
		sprintf((char*) &sBBMsg.data[2+32+32*i], version[i]);
	}

	BB_SendCommMsg(&sBBMsg);
}



/* 系统受到FTP消息后，做出回复 */
void BB_FtpReply(void) {
	stBBCommMsg sBBMsg;
	sBBMsg.headFlag = 0;

	sBBMsg.msgID[0] = 0x0F;
	sBBMsg.msgID[1] = 0x11;

	sBBMsg.dataLen = 1;
	sBBMsg.data[0] = 0;

	BB_SendCommMsg(&sBBMsg);

	return;
}


int BB_FtpUpdate(uint8* data) {
	char ftpBuff[256];
	memset(ftpBuff, 0, sizeof(ftpBuff));

	if (data[0] == '$') 
	{
		memset(ftpBuff, 0, sizeof(ftpBuff));

		//关闭可能正在使用的的ftp程序
		BB_Stop("ftpUpdate.sh");
		//通知程序进行ftp升级
		sprintf(ftpBuff, "/root/etc/update/ftpUpdate.sh &");
		printf("%s\n", ftpBuff);
		system(ftpBuff);
	} else if (data[0] = '#') 
	{
		memset(ftpBuff, 0, sizeof(ftpBuff));
		sprintf(ftpBuff, "/root/etc/update/upload_log.sh &");
		printf("%s\n", ftpBuff);
		system(ftpBuff);
	} else 
	{
		printf("bad ftpinfo format,please start with $\n");
	}

	return 1;
}

int BB_Stop(const char *processKeyName)
{
	printf("stop possible ftp process\n");
	char cmd[64]={0};
	sprintf(cmd, "ps -ef | grep %s | grep -v grep |awk '{ print $1}'|xargs kill -9", processKeyName);
	printf("%s\n", cmd);
	system(cmd);
	return 0;
}
