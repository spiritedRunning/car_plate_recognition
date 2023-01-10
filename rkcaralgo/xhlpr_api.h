#pragma once

#include "xhlpr_type.h"


enum {
	LPR_OK = 0,		               		  // 接口调用成功

	LPR_NG = -1,                          // 接口调用致命错误

	LPR_NG_ACTIVATE = 3019,	              // 激活失败

	LPR_PARAMETER_SET_ERR = 3020,         // 参数值不合法

	LPR_NG_CONFIG = 3021,	              // 配置失败

	LPR_NG_INIT_CAR = 3022,	              // 车辆检测初始化失败

	LPR_NG_INIT_PLATE = 3023,	          // 车牌检测初始化失败

	LPR_NG_INIT_OCR = 3024,	              // 车牌识别初始化失败

	LPR_NG_SESS_NULL = 3025,	          // Session为空

	LPR_NG_IMAGE_DATA = 3026,	          // 图片错误

	LPR_NG_OCR = 3027,	                  // 车牌识别错误

	LPR_PARAMETER_NULLPTR_ERR = 3028      // 参数地址为空
};

//车牌颜色
enum {
	LPR_COLOR_UNKNOW = 5000,              //未知
	LPR_COLOR_BLUE = 5001,                //蓝
	LPR_COLOR_YELLOW = 5002,              //黄
	LPR_COLOR_WHITE = 5003,	              //白
	LPR_COLOR_BLACK = 5004,	              //黑
	LPR_COLOR_GREEN = 5005,	              //绿
};

enum {
	LPR_TYPE_UNKNOW = 6000,               //未知	
	LPR_TYPE_BLUE = 6001,                 //蓝牌小型车牌   single_blue
	LPR_TYPE_YELLOW_SINGLE = 6002,        //黄牌单牌       single_yellow
	LPR_TYPE_YELLOW_DOUBLE = 6003,        //黄牌双牌       double_yellow
	LPR_TYPE_GREEN_GREEN = 6004,          //小型新能源牌   small_new_energy
	LPR_TYPE_GREEN_YELLOW = 6005,         //大型新能源牌   big_new_energy
	LPR_TYPE_J = 6006,                    //警车牌         white_police
	LPR_TYPE_WJ_SINGLE = 6007,            //武警牌         single_armed_police
	LPR_TYPE_WJ_DOUBLE = 6008,            //武警牌         double_armed_police
	LPR_TYPE_JUN_SINGLE = 6009,           //军车牌         single_army
	LPR_TYPE_JUN_DOUBLE = 6010,           //军车牌         double_army
	LPR_TYPE_LING = 6011,                 //领馆车牌       consulate
	LPR_TYPE_SHI = 60012,                 //使馆车牌       diplomatic_mission
	LPR_TYPE_GUA = 60013,                 //挂车车牌       double_yellow_gua
	LPR_TYPE_XUE = 60014,                 //驾校车牌	   coach
	LPR_TYPE_MINHANG = 60015,             //民航车牌	   civil_aviation
	LPR_TYPE_HK_MACAO = 60016,            //港澳车牌	   hk_and_macao
	LPR_TYPE_YINGJI = 60017,              //应急车牌	   emergency
	LPR_TYPE_SPECIAL = 60018,             //特殊车牌	   special
};

#ifdef __cplusplus
extern "C" {
#endif
	int XHLPR_API XHLPRAPI_Version(float* version);
	typedef int (XHLPR_API *PXHLPRAPI_Version(float *));
	
	int XHLPR_API XHLPRInit(const char *license_dir, const char *product_code);
	typedef int (XHLPR_API *PXHLPRInit)(const char *, const char *);

	/******************************Vehicle Detection******************************/
	int XHLPR_API VDCreate(XHLPR_SESS *sess,const char *pattern, int track = 0);
	typedef int (XHLPR_API *PVDCreate)(XHLPR_SESS *,const char *,int);

	int XHLPR_API VehicleDetect(XHLPR_SESS sess, unsigned char *data, int width, int height, VehicleInfo **vehicle,int *count,Rect *roi=0x0 );
	typedef int (XHLPR_API *PVehicleDetect)(XHLPR_SESS, unsigned char *, int, int, VehicleInfo **,int *, Rect *);
	
	int XHLPR_API VDDestroy(XHLPR_SESS *sess);
	typedef int (XHLPR_API *PVDDestroy)(XHLPR_SESS *);


	/***************************License Plate Detection**************************/
	int XHLPR_API PDCreate(XHLPR_SESS *sess);
	typedef int (XHLPR_API *PPDCreate)(XHLPR_SESS *);

	int XHLPR_API PlateDetect(XHLPR_SESS sess, unsigned char *data, int width, int height,  PlateInfo **plate,int *count,VehicleInfo *vehicle = 0x0, Rect *roi = 0x0 );
	typedef int (XHLPR_API *PPlateDetect)(XHLPR_SESS, unsigned char *, int, int, PlateInfo **,int *,VehicleInfo*,Rect*);

	int XHLPR_API PlateDetectTrack(XHLPR_SESS sess, unsigned char *data, int width, int height, PlateInfo **plate, int *count, VehicleInfo *vehicle,int vehicle_num);
	typedef int (XHLPR_API *PPlateDetectTrack)(XHLPR_SESS, unsigned char *, int, int, PlateInfo **,int *,VehicleInfo*,int);

	int XHLPR_API ImageQuality(XHLPR_SESS sess, unsigned char *data, int width, int height , PlateInfo *plate);
	typedef int (XHLPR_API *PImageQuality)(XHLPR_SESS,unsigned char *, int , int  , PlateInfo *);

	int XHLPR_API PDDestroy(XHLPR_SESS *sess);
	typedef int (XHLPR_API *PPDDestroy)(XHLPR_SESS *);


	/***************************License Plate Recognition**************************/
	int XHLPR_API PRCreate(XHLPR_SESS *sess);
	typedef int (XHLPR_API *PPRCreate)(XHLPR_SESS *, char *, int);

	int XHLPR_API PlateOCR(XHLPR_SESS sess, unsigned char *data, int width, int height, PlateInfo *plate);
	typedef int (XHLPR_API *PPlateOCR)(XHLPR_SESS, unsigned char *, int, int, PlateInfo *);

	int XHLPR_API PRDestroy(XHLPR_SESS *sess);
	typedef int (XHLPR_API *PPRDestroy)(XHLPR_SESS *);



	int XHLPR_API XHLPRFinal();
	typedef int (XHLPR_API *PXHLPRFinal)();
#ifdef __cplusplus
}
#endif
