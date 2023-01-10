#include "carplate_main.h"



// 1个字节与2字节与4字节参数存储方式相同，但是返回数据长度不同
static int iBytePara[] = { 0x0084,		// 车牌颜色
		};
// 2字节参数
static int iWordPara[] = { 0x0031,		// 电子围栏半径（非法位移阀值）

		0x0081,		// 车辆所在的省域ID
		0x0082,		// 车辆所在的市域ID
		};
// 4字节参数
static int iDWordPara[] = { 0x0001,   	// 终端心跳发送间隔
		0x0018,		// TCP服务器端口

		0x0020,		// 位置汇报策略，0：定时汇报；1：定距汇报；2：定时与定距汇报
		0x0021,		// 位置汇报方案，0：根据ACC状态；1：根据登录状态与ACC状态。

		0x0022,		// 驾驶员未登录汇报时间间隔
		0x0027,		// 休眠时汇报时间间隔
		0x0028,		// 紧急报警时，汇报时间间隔
		0x0029,		// 缺省时间汇报间隔

		0x002c,		// 缺省距离汇报间隔
		0x002d,		// 驾驶员未登录汇报间隔
		0x002e,		// 休眠时，汇报距离间隔
		0x002f, 	// 紧急报警时，汇报距离间隔

		0x0045,		// 终端电话接听策略，0：自动接听；1. ACC ON 时自动接听，OFF时手动接听
		0x0046,		// 每次通话最长时间(S)，0为不允许通话，0xFFFFFFFF为部限制。
		0x0047,		// 当月最长通话时间

		0x0055,		// 最高速度，单位为公里每小时
		0x0056,		// 超速持续时间
		0x0057,		// 连续驾驶时间门限
		0x0058, 	// 当天累计驾驶时间门限
		0x0059,		// 最小休息时间
		0x005a,		// 最长停车时间
		0x005B,		// 超速预警预警差值
		0x005C,		// 疲劳驾驶预警差值

		0x0080,		// 车辆里程表读数
		};
// string 类型参数
static int iStrPara[] = { 0x0013,		// 主服务器IP地址或者域名
		0x0017,		// 备用服务器IP地址或者域名

		0x0040,		// 监控平台电话号码
		0x0041,		// 复位电话号码
		0x0042,		// 恢复出厂设置电话号码

		0x0048,		// 监听电话号码

		0x0083,		// 车牌号码
		};


static inline int CheckParaType(int paraID) {
	int type = 0, i;
	// 一个字节长度的参数
	for (i = 0; i < sizeof(iBytePara) / 4; i++) {
		if (paraID == iBytePara[i])
			type = 0x01;
	}
	// 两个字节长度的参数
	for (i = 0; i < sizeof(iWordPara) / 4; i++) {
		if (paraID == iWordPara[i])
			type = 0x02;
	}
	// 4字节长度参数
	for (i = 0; i < sizeof(iDWordPara) / 4; i++) {
		if (paraID == iDWordPara[i])
			type = 0x04;
	}
	// 字符串参数
	for (i = 0; i < sizeof(iStrPara) / 4; i++) {
		if (paraID == iStrPara[i])
			type = 1000 + i * 50;
	}

	return type;
}

int readDataFromFile(char* filepath, uint8* dst) {
	FILE *f = fopen(filepath, "rb");
	if (!f) {
		printf("file not exist!!\n");
		return 0;
	}

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	//dst = (uint8*) malloc((length + 1) * sizeof(uint8));
	// if (dst) {
	
	if (length != fread(dst, 1, length, f)) {
		fclose(f);
		return 0;
	}
	// }	

	fclose(f);
	dst[length] = '\0';

	return length;
}




int BB_ReadFileData(char* fileName, int offset, int dataLen, uint8* dst) {
	FILE* fp = NULL;
	int ret = -1;

	/* 如果文件不存在，则建立该文件，并返回 FALSE */
	ret = access(fileName, F_OK);
	if (ret != 0) 
	{
		printf("the file is %s,line is %d\n",__FILE__,__LINE__);
		fp = fopen(fileName, "wb+");
		fclose(fp);
		return FALSE;
	} 
	else 
	{
		fp = fopen(fileName, "rb+");
	}
//	printf("offset=%d\n", offset);
	// 移动到指定位置
	ret = fseek(fp, offset, SEEK_SET);
	if (ret != 0) {
		printf("File seek error.\n");
		return FALSE;
	}
//	printf("dataLen=%d\n", dataLen);
	// 读取指定位置数据
	ret = fread(dst, 1, dataLen, fp);

	fclose(fp);
	return ret;
}

int BB_WriteFileData(char* fileName, int offset, int dataLen, uint8* src) {
	FILE* fp = NULL;
	int ret = -1;

	/* 如果文件不存在，则建立该文件*/
	ret = access(fileName, F_OK);
	if (ret != 0)
		fp = fopen(fileName, "wb+");
	else
		fp = fopen(fileName, "rb+");

	// 移动到指定位置
	ret = fseek(fp, offset, SEEK_SET);
	if (ret != 0) {
		printf("File seek error. %s\n", src);
		return FALSE;
	}
	// 写入指定位置数据
	ret = fwrite(src, 1, dataLen, fp);

	fclose(fp);
	system("sync");
	return ret;
}


void BB_ReadDevSeriNum(void) {
	int fd = open(BB_DEV_SERI_FILE, O_RDONLY);
	if (fd < 0) {
		printf("Can't open SeriNum File! %s\n", BB_DEV_SERI_FILE);
		return;
	}

	char cPhoneNum[12];
	int nread = read(fd, cPhoneNum, 12);
	if (nread < 12) {
		close(fd);
		printf("BB SeriNum Error!\n");
		return;
	}

	int index;
	char cTemp[3] = { 0 };
	for (index = 0; index < 6; index++) {
		memcpy(cTemp, &cPhoneNum[2 * index], 2);
		devSeriNum[index] = StrToU8(cTemp);
		devSeriNum[index] = DecToBCD(devSeriNum[index]);
	}
	close(fd);

}

