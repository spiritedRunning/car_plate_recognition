#pragma once

#if defined(_MSC_VER)
#define XHLPR_API __stdcall
#else
#define XHLPR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif
	typedef void*   XHLPR_SESS;

	//点坐标
	struct Point
	{
		float x;
		float y;
	};

	//矩形框
	typedef struct _Rect
	{
		float x;	                                 //左上角点X
		float y;	                                 //左上角点Y
		float width;	                             //宽度
		float height;	                             //高度
	}Rect;

	//车牌
#define PLATE_CHAR 20
	typedef struct _PlateInfo
	{
		Point points[4];                             //车牌坐标点,分别为左上角、右上角、右下角、左下角
		float points_score;                          //车牌坐标点置信度
		char plateNumber[PLATE_CHAR];                //车牌号
		float number_score;                          //车牌号置信度
		int type;                                    //类型
		int color;                                   //颜色
		float qualityScore;                          //质量评分
		int trackID;                                 //车牌跟踪ID
	}PlateInfo;

	//车辆信息
	typedef struct _VehicleInfo {
		Rect vehicle_rect;                           //车辆位置矩形框
		float score;                                 //车辆矩形框置信度
		int trackID;                                 //车辆跟踪ID
	}VehicleInfo;


#ifdef __cplusplus
}
#endif