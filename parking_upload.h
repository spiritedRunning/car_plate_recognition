#pragma once
#include <string>
#include <codecvt>
#include "util/util_common.h"

using namespace std;


typedef struct _ParkingCarInfo {
    string spaceCode; // 泊位号
    char carNo[32]; //  车牌号码，无车或者车牌错误都传空
    int plateLen;
    int camId; // 当前左中右cam
    int color; //   识别的车牌颜色 无车牌传0
    short plateType; // 识别的车牌种类  无车牌传0
    string lotcode;  // 停车场编号 非必须
    int carProbability; // 置信度0-100
    int eventType;  // 1有车有车牌 2有车无车牌 0无车
    string eventTime;  // 照片抓拍时间戳，单位【秒】

    bool isemtpy() {
        return spaceCode.empty() && carNo[0] == '\0' && camId == 0 && carProbability == 0;
    }

    void printInfo() {
        cout << dec;
        cout << "spaceCode: " << spaceCode << ", carNo: " << carNo << ", plateLen: " << plateLen << ", camId: " << camId << 
        ", color: " << color << ", plateType: " << plateType << ", lotcode: " << lotcode << ", carProbability: " << carProbability <<
        ", eventType: " << eventType << ", eventTime: " << eventTime << endl;
    }
} ParkingCarInfo;

typedef struct _ParkingCarMedia {
    string taskID;  // 任务编号 【一个task目前只能传一张图，测试只传一张，生产拼图】
    string imageUrl;   // 图片Http或Https的url完整路径 非必须
    string content;  // 图片Base64编码 去掉(data:image/jpeg;base64,)  非必须
    int eventType;  // 图片类型 1车牌抠图 2多张事件图拼接 3单张事件图
    long eventTime;   // 照片抓拍时间戳，单位【秒】

    bool isemtpy() {
        return taskID.empty() && imageUrl.empty() && content.empty();
    }

    void printInfo() {
        cout << dec;
        cout << "taskID: " << taskID << ", imageUrl: " << imageUrl << ", eventType: " << eventType << ", eventTime: " << eventTime << endl;
    }

} ParkingCarMedia;

typedef struct _ParkingCarPair {
    string serialcode;
    ParkingCarInfo carInfo;
    ParkingCarMedia carMedia;

    bool isempty() {
        return carInfo.isemtpy() || carMedia.isemtpy();
    }

    bool operator< (const _ParkingCarPair &other) const {
		if (carInfo.spaceCode == other.carInfo.spaceCode) {
			return carInfo.camId < other.carInfo.camId;
		}

		return carInfo.spaceCode < other.carInfo.spaceCode;
	}

} ParkingCarPair;

void requestToken();
void keepAlive();
void pushTrackingInfo();