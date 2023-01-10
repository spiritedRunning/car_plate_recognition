#include "carplate_main.h"
#include "util/cjson.h"


#define CMD_REGISTE				0x8100     	//	服务器回复注册信息

#define CMD_AHTHEN				0x8001		//	服务器通用应答

#define CMD_ELEC_FENCE			0x5101		//  服务器下发电子围栏

#define CMD_FTP_UPDATE			0x8f11		//  服务器下发远程升级命令

#define CMD_NET_TIME			0x8F10		//  服务器下发系统时间

#define CMD_CHANGE_PARA			0x8300		//  服务器下发参数

#define CMD_UPLOAD_PARA			0x8310		//  服务器查询所有参数

#define CMD_TAKE_PHOTO			0x8801		//	摄像头立即拍照


int reStartCnt = 180 * 5;
static uint8 dstData[1024];


#define TMPFILE "/root/app/ret"

uint8 roaddata_buf[1024*1000]={0};
uint8 roadID[100]={0};
uint8 roadMD5[100][32]={0};
int roadnum;


static void SetSysTime(uint8* bcdTime) {
	char dateCmd[100];
	sprintf(dateCmd, "date -s \"%02d-%02d-%02d %02d:%02d:%02d\"",
			2000 + bcdTime[0] / 16 * 10 + bcdTime[0] % 16,
			bcdTime[1] / 16 * 10 + bcdTime[1] % 16,
			bcdTime[2] / 16 * 10 + bcdTime[2] % 16,
			bcdTime[3] / 16 * 10 + bcdTime[3] % 16,
			bcdTime[4] / 16 * 10 + bcdTime[4] % 16,
			bcdTime[5] / 16 * 10 + bcdTime[5] % 16);
	system(dateCmd);
	return;
}

static int traversal_file(const char *path)
{
	int i=0;
	uint8 *md5;
	DIR *dir;
	struct dirent *ptr;
	char filename[32]={0};
	
	if ((dir = opendir(path)) == NULL)
	{
		perror("Open dir error...");
		return -1;
	}
	while ((ptr = readdir(dir)) != NULL)
	{
		if(strcmp(ptr->d_name, ".")==0 || strcmp(ptr->d_name, "..")==0)     
			continue;

		//printf("d_name = %s\n", ptr->d_name);
		//printf("d_type = %d, DT_REG = %d, DT_DIR = %d, DT_UNKNOWN = %d", ptr->d_type, DT_REG, DT_DIR, DT_UNKNOWN);		
		switch (ptr->d_type)
		{
			case DT_REG:			 
				//printf("the file name is %s/%s\n",path,ptr->d_name);
				sscanf(ptr->d_name,"%d",&roadID[i]);
				sprintf(filename,"%s/%s", path, ptr->d_name);
				md5 = MD5_file((uint8* )filename,32);
				//printf("the md5 is %s\n",md5);
				memcpy(&roadMD5[i],md5,32);
				free(md5);
				i++;
				break;
			case DT_DIR:			 
				break;
			case DT_UNKNOWN:		 
				printf("unknown file type.\n");
				break;
			default:
				break;
		}
	}
	closedir(dir);

	return 0;
}

static int store_roaddata(int len)
{
	//char *response = malloc(1000 * 1024);
	char *roadinfo = (char *)malloc(100 * 1024);
	char *roadname = (char *)malloc(100);
	int roadFd;
	int ret;
#if 0
	if(len != 0)
	{
		ret = inflate_read(roaddata_buf,len,&response,1);
		if(ret == -1)
		{
			printf("error,no gzip json data\n");
			return -1;
		}	
	}
#endif
	printf("decompre:==================>\n%s\n",roaddata_buf);

	//printf("debug=============================--the file is %s,line is %d\n",__FILE__,__LINE__);
	char *json = (char *)roaddata_buf;

	cJSON *pJson = cJSON_Parse(json);
	cJSON *arrayItem = cJSON_GetObjectItem(pJson,"data"); 
	cJSON *object0 = cJSON_GetObjectItem(arrayItem,"sectionList");
	cJSON *item0 = cJSON_GetObjectItem(arrayItem,"sectionNum"); 
	roadnum = item0->valueint;
	printf(".................>>>>>>>>the roadnum is %d\n",roadnum);

	if(access("/data/carports",F_OK) == -1)
	{
        if (mkdir("/data/carports",0777))
        {
			printf("creat file path failed!!!\n");
			exit(-1);
        }
 	}
	for(int i=0;i<roadnum;i++)
	{
		cJSON *object = cJSON_GetArrayItem(object0,i);   
		cJSON *item = cJSON_GetObjectItem(object,"roadSectionId");
		sprintf(roadname,"/data/carports/%d",item->valueint);
		item = cJSON_GetObjectItem(object,"spaceList");
		strcpy(roadinfo,item->valuestring);
		//printf(".................>>>>>>>>the roadname is %s\n",roadname);
		roadFd = open(roadname,O_WRONLY | O_CREAT | O_TRUNC);
		if(roadFd<0)
		{
			printf("roadFd open filed....\n");
			exit(-1);
		}		
		write(roadFd,roadinfo,strlen(roadinfo));
		close(roadFd);	
	}

	free(roadname);
	free(roadinfo);
	//free(response);
	return 0;
}

static int SavePara(char *parafile, const char *tmpData, int len)
{
	int tmpfd = 0;
	tmpfd = open(parafile,O_RDWR | O_CREAT | O_TRUNC);
	if(tmpfd<0)
	{
		printf("tmpfd open filed....\n");
		exit(-1);
	}

	write(tmpfd,tmpData,len);
	close(tmpfd);

	system("sync");
	return 0;
}

static int UpdateparaFromTmp(char *parafile)
{
	char paradata[1024]={0};
	char line[128] = {0}; 
	FILE * fd = fopen(parafile, "r");
	int ret;
    if(fd == NULL)
	{
		perror("Open file error...");
		exit(1);
	}
    while (fgets(line , 128, fd))
    {
        if(memcmp(line, "carplatescore", strlen("carplatescore")) == 0)
        {
            sscanf(line, "%*[^=]=%f", &carnum_score);           
        }
        if(memcmp(line, "cameratype", strlen("cameratype")) == 0 )
        {
            sscanf(line, "%*[^=]=%d", &cameraType); 
        }
		if(memcmp(line, "carscore", strlen("carscore")) == 0 )
        {
            sscanf(line, "%*[^=]=%f", &car_score); 
        }
		if(memcmp(line, "activacode", strlen("activacode")) == 0 )
        {
            sscanf(line, "%*[^=]=%[^;]", activacode); 
        }
		if(memcmp(line, "rtspenable", strlen("rtspenable")) == 0 )
		{
			sscanf(line, "%*[^=]=%d", &isRTSPEnable); 
		}
		if(memcmp(line, "gpsoffset", strlen("gpsoffset")) == 0 )
		{
			sscanf(line, "%*[^=]=%f", &gpsoffset); 
		}
		if(memcmp(line, "vehicle_roi", strlen("vehicle_roi")) == 0 )
		{   
            int x,y,width,height;
			ret = sscanf(line, "%*[^=]=[%d,%d,%d,%d]", &x,&y,&width,&height); 
            if(ret == 4)
            {
                vehicle_det_rect = cv::Rect(x,y,width,height);
            }
		}
        if(memcmp(line, "angle_region", strlen("angle_region")) == 0 )
		{   
            int angleMin,angleMax;
			ret = sscanf(line, "%*[^=]=[%d,%d]", &angleMin,&angleMax); 
            if(ret == 2)
            {
            	angleRegion[0] = angleMin;
            	angleRegion[1] = angleMax;
            }
			printf("%d %d %d \n",ret,angleMin,angleMax);
		}
    }
    fclose(fd);

	sprintf(paradata,"[car_para]\ncarplatescore=%.2f;\ncameratype=%d;\ncarscore=%.2f;\nactivacode=%s;\nrtspenable=%d;\ngpsoffset=%.3f;\nangle_region=[%d,%d];\nvehicle_roi=[%d,%d,%d,%d];\n",
		carnum_score,cameraType,car_score,activacode,isRTSPEnable,gpsoffset,angleRegion[0],angleRegion[1],vehicle_det_rect.x,vehicle_det_rect.y,vehicle_det_rect.width,vehicle_det_rect.height);
	SavePara("/root/app/conf/car_para.txt",paradata,strlen(paradata));
	
	return 0;
}

static int UploadPara()
{
	char data[1024];
	memset(data, 0, 1024);

	FILE * fp = NULL;
	int file_size;
	fp = fopen("/root/app/conf/car_para.txt", "rb");
	if (fp == NULL) {
		printf("open /root/app/conf/car_para.txt error\n");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fread(data, 1, file_size, fp);
	fclose(fp);

	printf("%s\n",data);

	stBBCommMsg sBBMsg;

	sBBMsg.headFlag = 0;
	sBBMsg.msgID[0] = 0x03;
	sBBMsg.msgID[1] = 0x10;

	sBBMsg.dataLen = strlen(data);
	printf("sBBMsg.dataLen:%d \n",sBBMsg.dataLen);
	sBBMsg.data[0] = 0x01;
	memcpy(&sBBMsg.data[0], &data[0], strlen(data));

	BB_SendCommMsg(&sBBMsg);

	return 0;
}


// 终端注册回复数据分析
static int BB_RegReply(uint8* data, int iDataLen, int type);


// 返回帧数据解析
void BB_Parse(uint8* data, int iDataLen, int type) {
	/* 如果进入该函数，证明有数据接收 */
	reStartCnt = 180 * 5;
	short iReplyID = 0;
	memset(dstData, 0, 1024);

	// 反向转码
	iDataLen = ReEscapeData(dstData, data, iDataLen);
	// PRINT("Bubiao reply: ", "");
	// showArray("", dstData, iDataLen);

	// 检测数据长度问题
	int iPackLen;
	// iPackLen = readByte2Short(&data[2], 2);
	iPackLen = data[2] * 256 + data[3];
	if (iDataLen != iPackLen + 13) {
		return;
	}

	// 检测数据校验码
	uint8 cSum = 0;
	cSum = CheckSum(dstData, iDataLen - 1);
	if (cSum != dstData[iDataLen - 1]) {
		return;
	}

	// 数据没有问题
	uint8* pCmdID = &dstData[0];
	// short iCommandID = readByte2Short(&dstData[0], 2);;
	int iCommandID = dstData[0] * 256 + dstData[1];

	// 消息流水号
	uint8* pSeri = &dstData[10];

	// 数据包指针
	uint8* pData = &dstData[12];

	int ret = -2;

	char roadURL[128]={0};
	uint8 urlserver[32]={0};
	char urlport[16]={0};
	uint8 urlname[64]={0};

	switch (iCommandID) {
		case CMD_REGISTE:
			ret = BB_RegReply(pData, iPackLen, type);
			if (ret == 0) {
				// 进行鉴权
				BB_Authen(type);
			} else {
				sleep(10);
				BB_TermRegister();
				break;
			}
			GsmstatFlag = 1;
			printf("Term  Register successfully!\n\n");
			break;
		case CMD_AHTHEN: // 0x80 01
			// iReplyID = readByte2Short(&pData[2], 2);
			iReplyID = pData[2] * 256 + pData[3];
			// 终端鉴权回复
			if (iReplyID == 0x0102) {
				PRINT("auth reply:", " 01 02 ");
				showArray("", pData, 5);

				if (pData[4] != 0x00) {
					sleep(10);
					// 如果鉴权失败，则重新进行注册
					printf("Term authen failed!\n\n");
					BB_TermRegister();
					break;
				}

				printf("===============auth success!==============\n");
				netAuthen = TRUE;

				GsmstatFlag = 1;
				sleep(10);
				BB_VersionMsg();
				
				traversal_file("/data/carports");
				BB_roadsecReply();
			} else { // 其他通用回复
				GsmstatFlag = 1;
				reStartCnt = 200 * 5;
			}
			break;

		case CMD_ELEC_FENCE:
			{
			printf("CMD_ELEC_FENCE\n");	
			printf("GET URL PARA:\n");
			char cmd[128] = {0};
			int len = pData[1]*256+pData[2];
			memcpy(roadURL,&pData[3],len);
			
			sscanf(roadURL,"https://%[a-z.0-9]:%[0-9]/%s",urlserver,urlport,urlname);
			printf("the url is %s,%s,%d,%s\n",roadURL,urlserver,atoi(urlport),urlname);
			//len = get_https_data(NULL,urlserver,atoi(urlport),urlname,roaddata_buf,sizeof(roaddata_buf));
			sprintf(cmd,"curl '%s' --compressed --insecure -o road.txt",roadURL);
			system(cmd);
			
			FILE * fp = NULL;
		 	int file_size;
		 	fp = fopen("road.txt", "rb");
		 	if (fp == NULL) {
				printf("null\n");
				return;
		 	}
		 	fseek(fp, 0, SEEK_END);
		 	file_size = ftell(fp);
		 	fseek(fp, 0, SEEK_SET);

		 	fread(roaddata_buf, 1, file_size, fp);
		 	fclose(fp);
			
			//printf(".>>>>>>>>>>the roaddata_buf is \n%s\n",roaddata_buf);
			ret = store_roaddata(0);
			if(ret == -1)
			{
				printf("the url is error,please check\n");
				break;
			}
			traversal_file("/data/carports");
			BB_roadsecReply();
			}
			break;

		case CMD_FTP_UPDATE:
			{
			printf("\n\n");
			printf("update info = %s\n", &data[12]);

			BB_FtpUpdate(&data[12]);
			BB_FtpReply();
			BB_VersionMsg();		
			}
			break;
		case CMD_NET_TIME:
			printf("0x8F10 = %s\n", &data[0]);	
			SetSysTime(pData);
			break;
		case CMD_CHANGE_PARA:
			{
				printf("\n\n\n");
				printf("GET SERVER PARA:\n");
				printf("%s\n",pData);

				char tmpData[2048] = {0};
				int len = 0;

				memcpy(tmpData,pData,iPackLen);
				len = iPackLen;

				ret = SavePara("/root/app/conf/tmppara.txt", tmpData, len);//暂时存入临时文件里
				if(ret == 0)//写入成功
				{
					printf("tmp data write ok \n");
		
					ret = UpdateparaFromTmp("/root/app/conf/tmppara.txt");
					if( 0 == ret)
					{
						init_parmeter("/root/app/conf/car_para.txt");
						printf("common reply\n");
						BB_CommReply(pCmdID, pSeri, 0);
						return;	
					}
				}
		
				BB_CommReply(pCmdID, pSeri, 1);
			}
			break;
		
		case CMD_UPLOAD_PARA:
			printf("\n\n\n");
			printf("CMD_UPLOAD_PARA\n");
			UploadPara();
			break;
		case CMD_TAKE_PHOTO:
			takePhotoFlag = 1;
			takePhotoSeriNo = pSeri[0] * 256 + pSeri[1];
			break;
		default:
			printf("no manage cmd\n");
			break;
	}

}

// 终端注册应答，如果注册成功则存储鉴权码
static int BB_RegReply(uint8* data, int iDataLen, int type) {
	printf("BB_RegReply start");
	int iResult = -1;
	iResult = data[2];

	uint8 tmpData[30];
	switch (iResult) {
	// 注册成功
	case 0x00:
		tmpData[0] = iDataLen - 3;
		memcpy(&tmpData[1], &data[3], iDataLen - 3);

		showArray("tmpData: ", tmpData, iDataLen - 2);
		if (type == SERVER_TYPE_BB) {
			BB_WriteFileData(BB_AUTHEN_FILE, 0, iDataLen - 2, tmpData);
		} else {
			BB_WriteFileData(TM_AUTHEN_FILE, 0, iDataLen - 2, tmpData);
		}
		
		break;
		// 车辆已被注册
	case 0x01:

		break;

		// 数据库中无该车辆
	case 0x02:

		break;

		// 终端已被注册
	case 0x03:

		break;

		// 数据库中无该终端
	case 0x04:

		break;

	default:
		iResult = 5;
		break;
	}
	return iResult;
}
static void BB_UploadRet(void) {

 	uint8 data[1024];
 	memset(data,0,1024);


 	FILE * fp = NULL;
 	int file_size;
 	fp = fopen(TMPFILE, "rb");
 	if (fp == NULL) {
 		 printf("null\n");
 		return;
 	}
 	fseek(fp, 0, SEEK_END);
 	file_size = ftell(fp);
 	fseek(fp, 0, SEEK_SET);

 	fread(data, 1, file_size, fp);
 	fclose(fp);


 	stBBCommMsg sBBMsg;

 	sBBMsg.headFlag = 0;
 	sBBMsg.msgID[0] = 0x03;
 	sBBMsg.msgID[1] = 0x10;


 	sBBMsg.dataLen = file_size;
 	sBBMsg.data[0]=0x01;
 	memcpy(&sBBMsg.data[0], &data[0], file_size);

 	BB_SendCommMsg(&sBBMsg);

 }

