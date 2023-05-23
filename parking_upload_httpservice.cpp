/**
 * Created by zach 2022/11/1
 * 
 * Handle http request to third-party server
 */
#include "util/sion.h"
#include "util/md5.h"
#include "util/timer.h"
#include "util/base64.h"
#include "carplate_main.h"
#include "parking_upload.h"

using namespace sion;
using namespace std;
using Json = nlohmann::json;

extern GPSData g_gpsData;

static const string PASSWORD = "1f76618858J7CXz38nDw3t2A0b200ddfaca";
static const string identity = "szqyws";
static const string BASE_URL = "http://mid.qa.sparkingcd.com/api/rest/" + identity + "/v1";

// parking lot No. for testing
static const string LOT_CODE = "51010400000030107";
static const string SPACE_CODE = "3107010";   // 3107001 ~ 3107054

static const int ERROR_TOKEN = 101;

static char TOKEN[1024] = {0};
char devId[13] = {0};

int retryTime = 0;

Async async_thread_pool;

void startUploadService() {
    async_thread_pool.Start();
    
    int8ToHexStr(devSeriNum, 0, 6, devId);
    cout << "devId: " << devId << endl;
    logfile("devId: %s\n", devId);

    std::thread tokenThread(requestToken);
    tokenThread.detach();

    std::thread heartbeatThread(keepAlive);
    heartbeatThread.detach();

    std::thread pushTrackingThread(pushTrackingInfo);
    pushTrackingThread.detach();
}


Header getHttpHeader(string token) {
    Header header;
    header.Add("Accept", "application/json");
    header.Add("Content-Type", "application/json");
    header.Add("Authorization", devId);

    if (!token.empty()) {
        token.erase(0, 1);
        token.erase(token.size() - 1, 1);
        header.Add("token", token);
    }
    return header;
}



string getSign(String params) {
    size_t pos = params.find("image_content");
    if (pos == string::npos) {
        printf("params: %s\n", params.c_str());
    }
    
    String sign = md5(params);
    //cout << "md5 sign:" << sign.ToLowerCase() << endl;
    return sign.ToLowerCase();
}

void requestToken() {
    string url = BASE_URL + "/system/token";
    this_thread::sleep_for(chrono::milliseconds(10 * 1000));

    cout << "requestToken url:" << url << endl;
    logfile("requestToken url: %s\n", url.c_str());

    String tag = md5(devId + PASSWORD + identity);
    long time = getCurrentTimeStamp() / 1000;
    cout << "tag: " << tag << ", time: " << dec << time << endl;


    while (true) {
        try {       
            Json params;
            params["client_tag"] = tag;
            params["event_time"] = to_string(time);

            url = url + "?client_tag=" + tag + "&event_time=" + to_string(time) + "&sign=" + getSign(params.dump());
            PRINT("http_request", "url: %s\n", url.c_str());
            auto id = async_thread_pool.Run([=] {
                return Request().SetUrl(url).SetHeader(getHttpHeader("")).SetHttpMethod(Method::Get);
            });
            auto pkg = async_thread_pool.Await(id); 

            
            // cout << "AsyncWait:  " << pkg.resp.GetHeader().Data().size() << pkg.err_msg << endl;
            // cout << "resq status: " << pkg.resp.Status() << endl;
            // cout << "AsyncWait resp: " << pkg.resp.StrBody() << endl;

            PRINT("", "Http Response:\n");
            Json reqObj = Json::parse(pkg.resp.StrBody());
            if (reqObj.contains("code")) {
                cout << "code: " << reqObj["code"] << endl;
                cout << "msg: " << reqObj["msg"] << endl;
            }
            
            cout << "token: " << reqObj["token"] << endl;
            memset(TOKEN, 0, sizeof(TOKEN));
            strcpy(TOKEN, reqObj["token"].dump().c_str());

            cout << "expire time: " << reqObj["expire"] << endl;
        } catch (const exception& e) {
            cerr << e.what() << '\n';

            sleep(10);
            requestToken();
        }

        this_thread::sleep_for(chrono::milliseconds(30 * 60 * 1000));
    }

}


void keepAlive() {
    this_thread::sleep_for(chrono::milliseconds(20 * 1000));

    String requestUrl = BASE_URL + "/patrolParking/heart";
    
    while (true) {
        try {
            PRINT("[keepAlive]", "requestUrl: %s\n", requestUrl.c_str());
            logfile("[keepAlive] requestUrl: %s\n", requestUrl.c_str());
            long currentTime = getCurrentTimeStamp() / 1000;

            Json params;
            params["device_id"] = devId;
            params["gps_value"] = to_string(g_gpsData.latlon.lon) + "," + to_string(g_gpsData.latlon.lat);
            params["event_time"] = to_string(currentTime);
            params["status"] = "1";
            
            Payload::FormData form;
            form.Append("device_id", devId);
            form.Append("gps_value", to_string(g_gpsData.latlon.lon) + "," + to_string(g_gpsData.latlon.lat));
            form.Append("event_time", to_string(currentTime));
            form.Append("status", "1");
            form.Append("sign", getSign(params.dump()));

            
            auto id = async_thread_pool.Run([=] {
                return Request().SetUrl(requestUrl).SetHeader(getHttpHeader(TOKEN)).SetHttpMethod(Method::Post).SetBody(form);
            });
            auto pkg = async_thread_pool.Await(id); 

            PRINT("", "Http Response:\n");
            Json reqObj = Json::parse(pkg.resp.StrBody());
            string code = reqObj.value("code", "");
            string msg = reqObj.value("msg", "");
            
            cout << "code: " << code << endl;
            cout << "msg: " << msg << endl;
            logfile("[keepAlive] code: %s, msg: %s\n", code.c_str(), msg.c_str());
            
            
            if (stoi(code) == ERROR_TOKEN) {
                cout << "[keepAlive] retryToken" << endl;
                requestToken();
            }

    
        } catch (const exception& e) {
            cerr << e.what() << '\n';
        }

        this_thread::sleep_for(chrono::milliseconds(20 * 1000));
    }
    
}

void pushCarImage(ParkingCarMedia carMedia) {
    String requestUrl = BASE_URL + "/patrolParking/image";
    cout << "pushCarImage requestUrl: " << requestUrl << endl;
    logfile("[pushCarImage] requestUrl: %s\n", requestUrl.c_str());

    try {
        vector<uint8_t> data = readDataFromFile(carMedia.imageUrl);
        string image(data.begin(), data.end());

        Json params;
        params["device_id"] = devId;
        params["task_id"] = carMedia.taskID;
        params["image_content"] = base64_encode(image, false);
        params["event_type"] = to_string(carMedia.eventType);
        params["event_time"] = to_string(carMedia.eventTime);
        
        
        Payload::FormData form;
        form.Append("device_id", devId);
        form.Append("task_id", carMedia.taskID);
        form.Append("image_content", base64_encode(image, false));
        form.Append("event_type", to_string(carMedia.eventType));
        form.Append("event_time", to_string(carMedia.eventTime));
        form.Append("sign", getSign(params.dump()));

        cout << "device_id:" << devId << ", task_id:" << carMedia.taskID << ", event_type:" << carMedia.eventType << ", event_time:" 
            << carMedia.eventTime << ", sign:" << getSign(params.dump()) << endl;
 
        auto id = async_thread_pool.Run([=] {
            return Request().SetUrl(requestUrl).SetHeader(getHttpHeader(TOKEN)).SetHttpMethod(Method::Post).SetBody(form);
        });
        auto pkg = async_thread_pool.Await(id, 3000); 

        PRINT("", "Http Response:\n");
        Json reqObj = Json::parse(pkg.resp.StrBody());
        string code = reqObj.value("code", "");
        string msg = reqObj.value("msg", "");
        
        cout << "code: " << code << endl;
        cout << "msg: " << msg << endl;
        logfile("[pushCarImage] code: %s, msg: %s\n\n", code.c_str(), msg.c_str());

        carMedia.printInfo();

        int ret = stoi(code);
        if (ret == 1) { // request failed
            if (retryTime > 3) {
                return;
            } 
            retryTime++;
            cout << "upload image failed! retry " << retryTime << " times" << endl;
            this_thread::sleep_for(chrono::milliseconds(3 * 1000));
            pushCarImage(carMedia);
        } else if (ret == 0) {
            retryTime = 0;
        }
        
        if (ret == ERROR_TOKEN) {
            cout << "[pushCarImage] retryToken" << endl;
            requestToken();
        }

    } catch (const exception& e) {
        cerr << e.what() << '\n';
    }
}


void pushParkingEvent(ParkingCarInfo carInfo, ParkingCarMedia carMedia) {
    String requestUrl = BASE_URL + "/patrolParking/event";
    cout << "pushParkingEvent requestUrl: " << requestUrl << endl;
    logfile("[pushParkingEvent] requestUrl: %s\n", requestUrl.c_str());

    carInfo.printInfo();
    // for (auto& x : carInfo.carNo) {
    //     cout << hex << x << " ";
    // }
    // cout << endl;
    printf("plate no: %s\n", carInfo.carNo);

    char utf8_plate[30] = "";
    if (carInfo.plateLen != 0 && carInfo.plateLen != 0xFF) {
        
        std::stringstream ss;
        ss << std::hex;
        for (auto& b : carInfo.carNo) {
            if (b != 0) {
                ss << (int)b << " ";
            }
            
        }
        std::string str = ss.str();
        std::cout << str << std::endl;

        gbk_to_utf8(carInfo.carNo, utf8_plate, 30);
        cout << "utf8 plate no: " << utf8_plate << endl;
    }

    try {
        Json params;
        string plate(utf8_plate);

        params["device_id"] = devId;
        params["space_code"] = SPACE_CODE;
        params["car_number"] = plate.empty() ? "" : plate;
        params["car_color"] = to_string(carInfo.color);
        params["car_type"] = to_string(carInfo.plateType);
        params["lot_code"] = LOT_CODE;
        params["car_probability"] = to_string(carInfo.carProbability);
        params["event_type"] = to_string(carInfo.eventType);
        params["event_time"] = carInfo.eventTime;
        
        
        Payload::FormData form;
        form.Append("device_id", devId);
        form.Append("space_code", SPACE_CODE);
        form.Append("car_number", plate.empty() ? "" : plate);
        form.Append("car_color", to_string(carInfo.color));
        form.Append("car_type", to_string(carInfo.plateType));
        form.Append("lot_code", LOT_CODE);
        form.Append("car_probability", to_string(carInfo.carProbability));
        form.Append("event_type", to_string(carInfo.eventType));
        form.Append("event_time", carInfo.eventTime);
        form.Append("sign", getSign(params.dump()));


        auto id = async_thread_pool.Run([=] {
            return Request().SetUrl(requestUrl).SetHeader(getHttpHeader(TOKEN)).SetHttpMethod(Method::Post).SetBody(form);
        });
        auto pkg = async_thread_pool.Await(id); 

        PRINT("", "Http Response:\n");
        Json reqObj = Json::parse(pkg.resp.StrBody());
        string code = reqObj.value("code", "");
        string msg = reqObj.value("msg", "");
        string taskID = reqObj.value("task_id", "");

        if (!taskID.empty()) {
            logfile("start push car image for taskID: %s\n", taskID.c_str());
            carMedia.taskID = taskID;
            pushCarImage(carMedia);
        }
        
        
        if (stoi(code) == ERROR_TOKEN) {
            cout << "[pushParkingEvent] retryToken" << endl;
            requestToken();
        }
        
    } catch (const exception& e) {
        cerr << e.what() << '\n';
    }
}


void pushTrackingInfo() {
    this_thread::sleep_for(chrono::milliseconds(40 * 1000));

    String requestUrl = BASE_URL + "/patrolParking/trajectory";
    
    while (true) {
        try {
            PRINT("[pushTrackingInfo]", "requestUrl: %s\n", requestUrl.c_str());
            logfile("[pushTrackingInfo] requestUrl: %s\n", requestUrl.c_str());

            long currentTime = getCurrentTimeStamp() / 1000;

            Json params;
            params["device_id"] = devId;
            params["gps_value"] = to_string(g_gpsData.latlon.lon) + "," + to_string(g_gpsData.latlon.lat);
            params["event_time"] = to_string(currentTime);
            params["status"] = "1";
            
            Payload::FormData form;
            form.Append("device_id", devId);
            form.Append("gps_value", to_string(g_gpsData.latlon.lon) + "," + to_string(g_gpsData.latlon.lat));
            form.Append("event_time", to_string(currentTime));
            form.Append("status", "1");
            form.Append("sign", getSign(params.dump()));

            
            auto id = async_thread_pool.Run([=] {
                return Request().SetUrl(requestUrl).SetHeader(getHttpHeader(TOKEN)).SetHttpMethod(Method::Post).SetBody(form);
            });
            auto pkg = async_thread_pool.Await(id); 

            PRINT("", "Http Response:\n");
            Json reqObj = Json::parse(pkg.resp.StrBody());
            string code = reqObj.value("code", "");
            string msg = reqObj.value("msg", "");
            
            cout << "code: " << code << endl;
            cout << "msg: " << msg << endl;
            logfile("[pushTrackingInfo] code: %s, msg: %s\n", code.c_str(), msg.c_str());
            
            if (stoi(code) == ERROR_TOKEN) {
                cout << "[pushTrackingInfo] retryToken" << endl;
                requestToken();
            }
        } catch (const exception& e) {
            cerr << e.what() << '\n';
        }

        this_thread::sleep_for(chrono::milliseconds(2 * 60 * 1000));
    }
}

