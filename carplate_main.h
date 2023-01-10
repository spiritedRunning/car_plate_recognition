#ifndef CARPLATE_OSD_FRAME_H_  
#define CARPLATE_OSD_FRAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <net/if.h>
#include <termios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/vfs.h>
#include <sys/shm.h>
#include <linux/watchdog.h>
#include <dlfcn.h>
#include <dirent.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <iconv.h>


#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include <fstream>
#include <iostream>
#include <thread>
#include <stdint.h>
#include <string>
#include <vector>

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>


#include <zlib.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "opencv2/opencv.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"


#include <opencv2/imgproc/imgproc.hpp>
#include <chrono>


#include "common/sample_common.h"
#include "rkmedia_api.h"
#include "librtsp/rtsp_demo.h"


#include "xhlpr_api.h"
#include "xhlpr_type.h"
#include "util/json.hpp"
#include "car_master_server.h"
#include "util/util_common.h"
#include "parking_upload.h"

using namespace std;
using namespace std::chrono;


typedef unsigned char 	uint8;

#define TRUE 	1
#define FALSE	0

#define PIC_STORE_DIR   		"/data/"
#define MED_STORE_DDIR_FMT  	"%02x-%02x-%02x/"   	
#define PIC_STORE_FL_FTM 		"%02x%02x%02x.jpg"   	



#define BB_DEV_SERI_FILE		"/root/app/conf/phonenum"
#define BB_AUTHEN_FILE			"/root/app/conf/authen"
#define TM_AUTHEN_FILE			"/root/app/conf/authen_tm"


#define BB_NET_TYPE		"eth0"
#define TM_NET_TYPE		"eth0"

#define BB_NET_IP   	"ps.v2net.cn"
#define BB_NET_PORT		1331

//#define BB_NET_IP 		"172.16.10.5"
//#define BB_NET_PORT		1331

// #define BB_NET_IP 		"172.16.10.16"
// #define BB_NET_PORT		2222

//#define BB_NET_IP   	"192.168.1.150"
//#define BB_NET_PORT		 18086


#define SERVER_TYPE_BB  0
#define SERVER_TYPE_TM  1

#define SERVER_PORT 	7800


#define LOCATION_INTVERVAL      10
#define OFFLINE_PERIOD          30

#define VERSION_CARPLATE       		"COMMON_V34_1_10"

extern uint8 takePhotoFlag;
extern unsigned short takePhotoSeriNo;

extern uint8 cameraType;
extern float carnum_score; 
extern float car_score;
extern char activacode[64];
extern uint8 isRTSPEnable;
extern float gpsoffset;

extern uint8 angleRegion[2];

extern cv::Rect vehicle_det_rect;

extern uint8 roadID[100];
extern uint8 roadMD5[100][32];
extern int roadnum;


extern volatile int netStatus;		// 声明系统与后台的连接状态
extern volatile int netAuthen;

extern const int BB_ASYNC_KEY;		// 用于多连接使用

extern uint8 devSeriNum[6];			// 产品唯一识别号码
extern uint8 paraFlag;
extern uint8 alarmflag;

extern char masterServerIP[30]; 

extern volatile uint8 GsmstatFlag;

#define STATE_END		0
#define STATE_SENDING	1
#define STATE_MULTI		2


#define BB_PKG_SIZE	    (1023*5)
#define BB_ASYNC_SIZE	(1200*5)

#define   AU_FILE_NUM   10

#define GPS_LEN 50

#define PRINT(tag, fmt, ...) printf("%s %s " fmt"", get_cur_time(), tag, ##__VA_ARGS__)


typedef struct _LatLon {
    double lat;
    double lon;
}LatLon;

typedef struct _GPSData {
    char available;
    LatLon latlon;
    unsigned short high;
    float speed;
    float direct;
    unsigned char gpsTime[8];
    
}GPSData;


typedef struct {
	uint8 flag;
	uint8 stat;
	uint8 data[6];
} stBBAUFile;
extern stBBAUFile BBUpInfo[AU_FILE_NUM];
void BB_MediaAutoReply(void);

void BB_UploadMedia();

typedef struct {
	long msgType;
	int iDataLen;		// 数据长度
	uint8 message[BB_ASYNC_SIZE];
} stBBSendData;

// 部标协议内部传输协议
typedef struct {
	uint8 headFlag;
	uint8 msgID[2];
	uint8 packMsg[4];
	int dataLen;
	uint8 data[BB_PKG_SIZE+1];
} stBBCommMsg;



typedef struct {
	volatile uint8 alarmFlag[4];	// 报警标志
	volatile uint8 statFlag[4];		// 状态标志
	volatile uint8 bbLat[4];    	// 部标经度
	volatile uint8 bbLng[4];    	// 部标纬度
	volatile uint8 high[2];     	// 当前海拔
	volatile uint8 speed[2];		// GPS速度
	volatile uint8 direct[2];   	// 行驶方向
	volatile uint8 gpsTime[6];  	// GPS标准时间
	volatile uint8 extraA5Msg[8];		
} stGpsInformat;


extern stGpsInformat* pGpsInfo;

extern unsigned char frameData[1280 * 720 * 2];

extern int fileDataLen;
extern uint8 fileDataBuff[1024 * 1024];
//extern uint8* fileDataBuff;
extern uint8 actionCode;

/**************md5&&https*********************/

int get_https_data(char *ip,char *name,int portnumber,char *get_str,char *rsp_str,int rsp_buf_len);
unsigned char *MD5_file (unsigned char *path, int md5_len);
int inflate_read(char *source,int len,char **dest,int gzip);

typedef unsigned long int UINT4;
/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;
void MD5Init (MD5_CTX *mdContext);
void MD5Update (MD5_CTX *mdContext, unsigned char *inBuf, unsigned int inLen);
void MD5Final (MD5_CTX *mdContext);
/*************************************************************************/



void GpsServStart(void);
int rv1126_init(void);

//void PicSaveTask(char *yuv_osd_buffer);
void PicSaveTask(cv::Mat& img);
int nv12_to_jpeg(FILE  *pFileName,const char* pYUVBuffer, const int nWidth, const int nHeight ,int quality);
int UTF2Unicode(wchar_t *wstr, int size, char *utf8);
void NV12_to_rgb24(unsigned char* yuvbuffer,unsigned char* rgbbuffer, int width,int height);
int rgb_to_jpg(uchar *pdata, char *jpg_file, int width, int height);



int OpenTtyDev(const char *Dev);
int SetTtySpeed(int fd, int speed);
int SetTtyParity(int fd, int databits, int stopbits, int parity);
int Seri_Initial(char* dev, int speed, int databits, int stopbits,int parity);


void BB_TermRegister(void);
void BB_Authen(int type);
void BB_TermBreath(void);
void BB_CommReply(uint8* pCMD, uint8* pSeri, uint8 uResult);
void BB_roadsecReply();
void BB_VersionMsg(void);
void BB_FtpReply(void);
void BB_LocTaskStart(void);
void BB_LocMsgReply(uint8* uSeri, stGpsInformat* stGpsInfo);
void setMediaMsgFlag(int flag);

void TM_LocTaskStart(void);
void TM_LocMsgReply(uint8* uSeri, stGpsInformat* stGpsInfo);
/*
 * 		数据解析跳转函数
 *
 */
void BB_Parse(uint8* data, int iDataLen, int type);

void relayBuBiaoData(uint8* buffer, int len);
/*
 * 		网络连接与信息读取
 *
 */

int BB_NetSendDataTest(uint8* uData, int iDataLen);
void BB_RecSndServInit(void);
int BB_ConnectServer(char* url, int port);
int BB_DisconnectServer(void);
void BB_KeepAlive(void);
static void BB_UploadRet(void);

int TM_NetSendDataTest(uint8* uData, int iDataLen);
void TM_RecSndServInit(void);
int TM_ConnectServer(char* url, int port);
int TM_DisconnectServer(void);
void TM_KeepAlive(void);
static void TM_UploadRet(void);

/*
 * 		基础操作函数  BB_Comm.c
 */
uint8 DecToBCD(uint8 dec);
uint8 BCDToDec(uint8 bcd);
void IntToDWord(uint8* dst, int src);
void StrToDWord(char* cSrc, uint8* dstData);
void StrToWord(char* cSrc, uint8* dstData);
uint8 StrToU8(char* cSrc);
uint8 CheckSum(uint8* buff, int length);
int hex2byte(uint8 *dst, const char *src);

int EscapeData(uint8 *dst, uint8* source, int length);
int ReEscapeData(uint8 *dst, uint8* source, int length);
int ReadDir(char* dirName);
int ReadFileEnd(char* fileName, int offset, int dataSize, uint8* dst);
int readDataFromFile(char* filepath, uint8* dst);
/*
 * 		参数文件的操作 BB_Para.c
 */
//void BB_TotalParaReply(uint8* pSeri);
void BB_ReadDevSeriNum(void);
void BB_SetPara(uint8* data);
void BB_WriteDefPara();
//void BB_WritePara(int iParaID, uint8 datLen, uint8* pData);
//int BB_ReadPara(int iParaID, int* dataLen, uint8* pData);
int BB_ReadFileData(char* fileName, int offset, int dataLen, uint8* dst);
int BB_WriteFileData(char* fileName, int offset, int dataLen, uint8* src);
/* 信息透传 */
void BB_TransmitMsg(uint8 dataID, int dataLen, uint8* data);
/* 报警信息上传 */
void BB_AlarmTask(void);
/* 多媒体信息上传 */
void BB_MediaMsgReply(int cmd, uint8* pSeri, uint8* pData);
unsigned int BB_TakePhotoReply(uint8* pSeri);
void BB_TakePhotoUpload(unsigned int mediaID, uint8* pSeri);
void BB_TransmitApc(uint8 dataID, int dataLen, uint8* data,stGpsInformat* stGpsInfo);
unsigned int BB_GetKeyID(uint8* ID);
void BB_SendCommMsg(stBBCommMsg* stBBMsg);
void BB_SendCommMsgTest(stBBCommMsg* stBBMsg);
int BB_FtpUpdate(uint8* data);
int BB_TTSParse(uint8* data);
int  BB_Resolve(void);
int BB_UpdateParaFromTmp(const char* tmpFile);
int BB_SaveTmpPara(const char* tmpFile,uint8* data, int iDataLen);
void BB_UploadPara();
int BB_Stop(const char *processName);
void doLog(const char* text);
int BB_RtVideo(uint8* data, int iDataLen);

unsigned int BB_PLATEUPLOAD(uint8* carplate,uint8 carlen,uint8 carconfidence,int type,int color,uint8 *portnumber,uint8 portlen,uint8 *areaid,stGpsInformat *pGpsInfo,char * path);
void push_buffer_venc(uint8 *bufs, int buf_size);
void push_buffer_disp(int8_t *bufs, int buf_size);

unsigned int BB_PhotoUpload(char * path);

int  ParkrecThread(double baselat,double baselng);
void CarportDetStart(void);
void savelog(const char *src);
int init_parmeter(char *parafile);

void startUploadService();
bool isBubiaoConnected();
void setMasterServerTerminate();

void initClientSocket();
int sendSlaveData(uint8* data, int len);

inline void showArray(char* str, uint8_t *data, int len);
const char* getStateDescription(int state);

void pushParkingEvent(ParkingCarInfo carInfo, ParkingCarMedia carMedia);

#endif /* carplate_osd_frame.h */
