/**
 * Created by zach 2022/10/20
 * 
 * Parse and integrated Bubiao data from other slave 1126 clients
 */

#include "car_master_server.h"
#include "util/timer.h"

using namespace std;
using namespace cv;

#define EPOLL_MAX_NUM 	50
#define BUFFER_MAX_LEN 4096 * 2


#define CAR_PARKING_INFO_UPLOAD		0x5103   	// upload carplate info from client
#define CAR_MEDIA_UPLOAD			0x0801 		// upload media content

int epfd = 0;
int listen_fd = 0;

const int CAM_FRONT = 1;
const int CAM_BACK = 2;
const int CAM_CENTER = 3;

volatile sig_atomic_t stop = 0;

const string separator = "#";
const string underscore = "_";

char UPLOAD_ROOT_PATH[50] = {0};
uint8 buffer[BUFFER_MAX_LEN];
uint8 slaveDstData[BUFFER_MAX_LEN];


extern GPSData g_gpsData;
unordered_map<string, map<int, vector<uint8>>> mediaMap;
unordered_map<string, ParkingCarInfo> devToParkingInfoMap;


priority_queue<ParkingCarPair> parkingPairPq;

unordered_map<string, int> clientsMap;
static void* initServerThread(void* arg);


static void uploadRoutine() {
	this_thread::sleep_for(chrono::milliseconds(30 * 1000));
	while (true) {

		findBestMatchMedia();
		this_thread::sleep_for(chrono::milliseconds(10 * 1000));
	}
	
}

void cleanup() {
	printf("clean up socket resources\n");
	// Cleanup resources
	if (close(listen_fd) == -1) {
		perror("close listen_fd");
		exit(EXIT_FAILURE);
	}

	if (close(epfd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}
}

void setMasterServerTerminate() {
	cleanup();
	stop = 1;
}

void initBubiaoServer() {
    printf("..............init Master Server............\n");

	// pthread_attr_t attr;
	// pthread_attr_init(&attr);
	// pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thServer;
	pthread_create(&thServer, NULL, initServerThread, NULL);

	std::thread uploadMergeTimer(uploadRoutine);
 	uploadMergeTimer.detach();

	char mkdirCMD[100] = {0};
	sprintf(UPLOAD_ROOT_PATH, "/mnt/sdcard/uploadimg/20%02d-%02d-%02d/", 
						g_gpsData.gpsTime[0], g_gpsData.gpsTime[1], g_gpsData.gpsTime[2]);
	
	sprintf(mkdirCMD, "mkdir -p %s", UPLOAD_ROOT_PATH);
	system(mkdirCMD);

	// pthread_join(thServer, NULL);
}

static int add_epollfd(int epollfd, int fd, int state) {
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;

	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static int delete_epollfd(int epollfd, int fd, int state) {
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	
	return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}


static void* initServerThread(void* arg) {
    int client_fd = 0;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    struct epoll_event events[EPOLL_MAX_NUM];

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    // bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    bind(listen_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));

    // listen
    listen(listen_fd, 10);

    // epoll create
    epfd = epoll_create(EPOLL_MAX_NUM);
    if (epfd < 0) {
        perror("epoll create");
        close(epfd);
        close(listen_fd);
        return 0;
    }
	add_epollfd(epfd, listen_fd, EPOLLIN);

    //my_events = (epoll_event*) malloc(sizeof(struct epoll_event) * EPOLL_MAX_NUM);

    printf("epoll_create succ\n");
    while (!stop) {
		//printf("stop = %d\n", stop);
        int active_fds_cnt = epoll_wait(epfd, events, EPOLL_MAX_NUM, 10 * 1000);

        for (int i = 0; i < active_fds_cnt; i++) {
            if (events[i].data.fd == listen_fd && (events[i].events & EPOLLIN)) {
                client_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accpet");
					usleep(1000);
                    continue;
                }

				add_epollfd(epfd, client_fd, EPOLLIN);

				char ip[20];
				inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)); 
				int port = ntohs(client_addr.sin_port);
				if (port != 0) {
					clientsMap[string(ip)] = client_fd;
				}
				
				printf("New connection: [%s:%d], fd: %d, connections count: %d\n", ip, port, client_fd, clientsMap.size());
				logfile("New connection: [%s:%d], fd: %d, connections count: %d\n", ip, port, client_fd, clientsMap.size());
				
            } else {
				memset(buffer, 0, sizeof(BUFFER_MAX_LEN));

                client_fd = events[i].data.fd;
                // start read
				uchar c;
                int len = recv(client_fd, &c, 1, 0);
                //printf("read count: %d\n", n);
                
				if (len > 0) {
					int index = 0;

					if (c != 0x7e) {
						printf("invalid header\n");
						usleep(1000);
						continue;
					}

					buffer[index++] = c;
					do {
						len = read(client_fd, &c, 1);
						if (len > 0) {
							buffer[index++] = c;
						} else {
							usleep(1000);
						}
					} while (c != 0x7e);
						
					buffer[index] = '\0';
                    relayBuBiaoData(buffer, index);
				} else if (len <= 0) {
					for (auto& x : clientsMap) {
						if (x.second == client_fd) {
							cout << "Remove connection: " << x.first << ", ";
							logfile("Remove connection: %s\n", x.first.c_str());
							clientsMap.erase(x.first);
							break;
						}
					}
					// if (clientsMap.find(client_fd) != clientsMap.end()) {
						// printf("Remove connection2 %s. ", clientsMap[client_fd]);
					// }	
					//cout << "Close fd: " << client_fd << ", connection count: " << clientsMap.size() << endl; 
					printf("Close fd: %d, connection count: %d\n", client_fd, clientsMap.size());
					logfile("Close fd: %d, connection count: %d\n", client_fd, clientsMap.size());

					delete_epollfd(epfd, client_fd, EPOLLIN);
                    close(client_fd);
                } 
            }
        }
        usleep(1000);
    }

	cleanup();
}

pair<ParkingCarPair, int> chooseMostEffective(map<string, pair<int, int>>& carPlateCount, map<string, ParkingCarPair>& carPlatePairs, 
	ParkingCarPair curPair, ParkingCarInfo curParkingInfo) {
	
	string carNo = curParkingInfo.carNo;
	int probability = curParkingInfo.carProbability;

	if (carPlateCount.count(carNo) == 0) {
		carPlateCount[carNo] = make_pair(1, probability);
	} else {
		carPlateCount[carNo].first++;
		carPlateCount[carNo].second += probability;
	}

	carPlatePairs[carNo] = curPair;

	string mostEffectivePlate;
	int mostFreq = 0, mostEffectiveProbability = 0;
	for (auto& entry : carPlateCount) {
		string carNo = entry.first;
		int count = entry.second.first;
		int probability = entry.second.second;
		int averageProb = probability / count;

		if (count > mostFreq || (count == mostFreq && averageProb > mostEffectiveProbability)) {
			mostEffectivePlate = carNo;
			mostFreq = count;
			mostEffectiveProbability = averageProb;
		} 
	}

	return make_pair(carPlatePairs[mostEffectivePlate], mostEffectiveProbability);
}

void handleUpload(pair<ParkingCarPair, int> effective, vector<ParkingCarPair>& pairVec) {
	ParkingCarPair bestPair = effective.first;
	int bestProb = effective.second;
	ParkingCarInfo bestCarInfo = bestPair.carInfo;
	ParkingCarMedia bestMedia = bestPair.carMedia;

	if (bestCarInfo.camId == CAM_CENTER) {
		uploadMedia(bestCarInfo, bestMedia);
	} else {
		combineLeftAndRight(bestCarInfo, bestMedia, pairVec, bestProb);
	}
	
}

static void findBestMatchMedia() {
	PRINT("", "--------- find best match media to upload --------\n");
	logfile("--------- find best match media to upload --------\n");

	ParkingCarInfo FBestCarInfo{}, BBestCarInfo{}, CBestCarInfo{};
	ParkingCarMedia FBestCarMedia{}, BBestCarMedia{}, CBestCarMedia{};

	int size = parkingPairPq.size();
	if (size == 0) {
		// cout << "parking queue is empty" << endl;
		return;
	}
	cout << "findBestMatchMedia parkingQueue size: " << parkingPairPq.size() << endl;
	logfile("findBestMatchMedia parkingQueue size: %d\n", parkingPairPq.size());

	pair<ParkingCarPair, int> effective;
	string lastParkingNo = parkingPairPq.top().carInfo.spaceCode;

	map<string, pair<int, int>> carPlateCount;  // pair: frequency and total probability
	map<string, ParkingCarPair> carPlatePairs;
	vector<ParkingCarPair> pairVec;

	for (int i = 0; i < size; i++) {
		ParkingCarPair cur = parkingPairPq.top();
		pairVec.push_back(cur);
		parkingPairPq.pop();

		ParkingCarInfo curParkingInfo = cur.carInfo;

		if (curParkingInfo.spaceCode == lastParkingNo) {
			effective = chooseMostEffective(carPlateCount, carPlatePairs, cur, curParkingInfo);
		} else { // upload last parking info
			if (!effective.first.isempty()) {
				handleUpload(effective, pairVec);
			}
			
			carPlateCount.clear();
			carPlatePairs.clear();
			effective = chooseMostEffective(carPlateCount, carPlatePairs, cur, curParkingInfo);
		};
	}	

	if (!effective.first.isempty()) {
		handleUpload(effective, pairVec);
	}

	cout << "findBestMatchMedia end" << endl;
}

int mergePicwithHorizontally(string imgpath1, string imgpath2, string outputPath, string spacecode) {
    cout << "merget output path: " << outputPath << endl;
    Mat img1 = imread(imgpath1);
    Mat img2 = imread(imgpath2);

    if (!img1.data || !img2.data) {
        cout << "Could not load input image file" << endl;
        return -1;
    }

    resize(img1, img1, Size(img1.cols, img1.rows));
    resize(img2, img2, Size(img2.cols, img2.rows));

	cout << dec;
    cout << "img1 size: " << img1.size() << ", img2 size: " << img2.size() << endl;

    Mat result;
    hconcat(img1, img2, result);

	// add watermark
	// putText(result, spacecode.c_str(), cv::Point(10, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
	
	// char text[256];
	// sprintf(text,"[%.8f,%.8f]", g_gpsData.latlon.lat, g_gpsData.latlon.lon);
	// putText(result, text, cv::Point(10, 680), CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);


    vector<int> compress_param;
    compress_param.push_back(CV_IMWRITE_JPEG_QUALITY);
    compress_param.push_back(50);

    cout << "merge size:  " << result.size() << endl;
    if (result.size().width > 0 && result.size().height > 0) {
        imwrite(outputPath, result, compress_param);
    }
    
	logfile("merget output path succ: %s\n", outputPath.c_str());
    return 0;
}


static void combineLeftAndRight(ParkingCarInfo bestCarInfo, ParkingCarMedia bestCarMedia, vector<ParkingCarPair>& pairVec, int avergeProb) {
	string frontImage, backImage;

	logfile("bestCarInfo camId: %d, spaceID: %s, carplate: %s, prob: %d\n", bestCarInfo.camId, bestCarInfo.spaceCode.c_str(), 
			bestCarInfo.carNo, bestCarInfo.carProbability);
	if (bestCarInfo.camId == CAM_FRONT) {
		frontImage = bestCarMedia.imageUrl;

		for (int i = 0; i < pairVec.size(); i++) {
			if (pairVec[i].carInfo.camId == CAM_BACK) {
				backImage = pairVec[i].carMedia.imageUrl;
			}
		}
	} else if (bestCarInfo.camId == CAM_BACK) {
		for (int i = 0; i < pairVec.size(); i++) {
			if (pairVec[i].carInfo.camId == CAM_FRONT) {
				frontImage = pairVec[i].carMedia.imageUrl;
			}
		}
		
		backImage = bestCarMedia.imageUrl;
	}
	cout << "front img path: " << frontImage << ", back image path: " << backImage << endl;
	logfile("front img path: %s, \nback image path: \n%s\n", frontImage.c_str(), backImage.c_str());

	if (frontImage.empty() || backImage.empty()) {
		if (frontImage.empty()) {
			cout << "only upload back side" << endl;
			logfile("only upload back side\n");
		}
		if (backImage.empty()) {
			cout << "only upload front side" << endl;
			logfile("only upload front side\n");
		}

		uploadMedia(bestCarInfo, bestCarMedia);	
		return;
	}

	// /mnt/sdcard/uploadimg/2022-11-10/013062509088#01-01-003#1_D22222_86_1669860213881.jpg
	//  , mnt, sdcard, uploadimg, 2022-11-10, 013062509088, 01-01-003, 1, D22222, 86, 1669860213881, jpg
	auto tokens = split_str(bestCarMedia.imageUrl, "[/#_.]");
	cout << "split result: " << endl;
	for (auto& item : tokens) { 
		cout << item << ", ";
	}
	cout << "\n" << endl;
	if (tokens.size() < 7) {
		cout << "invalid img path\n" << endl;
		return;
	}

	string combineTargetPath = UPLOAD_ROOT_PATH + tokens[5] + separator + bestCarInfo.spaceCode + separator + 
			to_string(CAM_FRONT) + to_string(CAM_BACK) + underscore + string(bestCarInfo.carNo) + underscore 
			+ to_string(avergeProb) + underscore + to_string(getCurrentTimeStamp()) + ".jpg";
	mergePicwithHorizontally(frontImage, backImage, combineTargetPath, bestCarInfo.spaceCode);

	bestCarMedia.imageUrl = combineTargetPath;	

	uploadMedia(bestCarInfo, bestCarMedia);	

	cout << "combineLeftAndRight end\n" << endl;
}

static void uploadMedia(ParkingCarInfo carInfo, ParkingCarMedia carMedia) {
	if (carInfo.isemtpy() || carMedia.isemtpy()) {
		return;
	}

	cout << "uploadMedia url: " << carMedia.imageUrl << endl;
	logfile("uploadMedia url: %s\n", carMedia.imageUrl.c_str());
	pushParkingEvent(carInfo, carMedia);

	PRINT("", "-------------------uploadMedia end-------------------\n");
	logfile("-------------------uploadMedia end-------------------\n");
}



void parseMsgHeader(uint8* msg, MsgHeader* header) {
	header->msgID = readByte2Short(msg, 0);
	short prop = readByte2Short(msg, 2);
	header->isMultiPack = (prop & 0x2000) == 0x2000 ? 1 : 0;
	header->bodyLen = prop & 0x03FF;

	memcpy(header->devNo, &msg[4], 6);
	header->serial = readByte2Short(msg, 10);

	if (header->isMultiPack) {
		header->packCount = readByte2Short(msg, 12);
		header->packIdx = readByte2Short(msg, 14);
	}
}

bool checkPackLoss(map<int, uint8*> byteMap, int packNo) {
	int maxIndex = 0;
	map<int, uint8*>::iterator iter;

	for (iter = byteMap.begin(); iter != byteMap.end(); iter++) {
		maxIndex = max(maxIndex, iter->first);
	}

	return maxIndex + 1 == packNo;
}

int convertPlateColor(int color) {
	int plateColor = 0;
	switch (color)
	{
	case 5001:  // blue
		plateColor = 11;
		break;
	case 5002:	// yellow
		plateColor = 21;
	case 5003: // white
		plateColor = 41;
	case 5004: // black
		plateColor = 31;
	case 5005: // green
		plateColor = 51;
	
	default:
		break;
	}
	return plateColor;
}

int convertCarType(int type) {
	int carType = 0;
	switch (type)
	{
	case 6001:
		carType = 11;
		break;
	case 6004:
		carType = 41;
	case 6005:
		carType = 51;
	
	default:
		break;
	}
	return carType;
}


void relayBuBiaoData(uint8* buffer, int len) {
	//cout << "start RelayBuBiaoData" << endl;
	memset(slaveDstData, 0, BUFFER_MAX_LEN);
	//showArray("buffer data: ", buffer, len);
	len = ReEscapeData(slaveDstData, buffer, len);

	if (slaveDstData[0] != 0x7E || slaveDstData[len - 1] != 0x7E) {
		printf("invalid msg header\n");
		return;
	}

	uint8 realData[len - 2] = {0};
	memcpy(realData, &slaveDstData[1], len - 2);
	//showArray("unWrapped data: ", realData, len - 2);
	len = sizeof(realData) / sizeof(uint8);

	uint8 cSum = 0;
	cSum = CheckSum(realData, len - 1);
	if (cSum != realData[len - 1]) {
		printf("invalid check sum\n");
		return;
	}

	MsgHeader msgHeader;
	parseMsgHeader(realData, &msgHeader);

	//cout << "serial no: " << +msgHeader.serial << endl;

	char serialcode[13] = {0};
	int8ToHexStr(msgHeader.devNo, 0, 6, serialcode);
	//printf("devNo: %s\n", serialcode);

	switch (msgHeader.msgID) {
		case CAR_PARKING_INFO_UPLOAD: {
			cout << "msgId: 0x" << uppercase << hex << msgHeader.msgID << endl;
			uint8* pData = &realData[12];  // 数据包指针

			uint8 eventType = pData[0];
			uint8 camId = pData[1];
			uint8 carType = pData[2];
			short plateColor = readByte2Short(pData, 3);
			short platetype = readByte2Short(pData, 5);
			printf("camId: %d, color: %d, carType: %d\n", camId, plateColor, carType);

			uint8 pLen = pData[7];
			uint8 plateNo[pLen + 1] = {0};
			memcpy(plateNo, &pData[8], pLen);
			printf("plateNo: %s\n", plateNo);		

			int roadID = readByte2Int(pData, 8 + pLen);

			uint8 parkNoLen = pData[12 + pLen];  
			char parkNo[parkNoLen + 1] = {0};
			memcpy(parkNo, &pData[13 + pLen], parkNoLen);
			printf("parkNoLen = %d, parkNo: %s\n", parkNoLen, parkNo);

			int contentLen = pLen + parkNoLen;
			short confidence = readByte2Short(pData, 13 + contentLen);
			int latitude = readByte2Int(pData, 15 + contentLen);
			int longtitude = readByte2Int(pData, 19 + contentLen);
			printf("confidence: %d, latitude: %d, longtitude: %d\n", confidence, latitude, longtitude);

			if (confidence < 0 || confidence > 100) {
				confidence = 0;
			}

			uint8 captureTime[7] = {0};
			readBytes(pData, 23 + contentLen, captureTime, 6);
			char timeStr[13] = {0};
			int8ToHexStr(captureTime, 0, 6, timeStr);
			printf("captureTime: %s\n", timeStr);

			string str(timeStr);
			time_t platetime = convert2Timestamp(str);
			printf("convert time: %d\n", platetime);
			if (platetime == 0) {
				platetime = getCurrentTimeStamp() / 1000;
				printf("new platetime: %d\n", platetime);
			}


			uint mediaCount = pData[29 + contentLen];
			int mediaID = readByte2Int(pData, 30 + contentLen);
			printf("captureCount: %d, captureMediaID: %d\n", mediaCount, mediaID);

			int type = 0;
			if (parkNoLen == 0xFF) {
				type = 0; // 无车
			} else if (parkNoLen > 0) {
				type = 1; // 有车有车牌
			} else {
				type = 2; // 有车无车牌
			}

			ParkingCarInfo parkingcarInfo;
			memset(parkingcarInfo.carNo, 0, sizeof(parkingcarInfo.carNo));
			printf("carplate pLen: %d\n", pLen);
			if (pLen != 0xFF) {
				memcpy(parkingcarInfo.carNo, plateNo, pLen);
				parkingcarInfo.carNo[pLen] = '\0';
			}
			parkingcarInfo.plateLen = pLen;
			
			parkingcarInfo.camId = camId;
			parkingcarInfo.carProbability = confidence;
			parkingcarInfo.color = convertPlateColor(plateColor);
			parkingcarInfo.eventTime = to_string(platetime);
			parkingcarInfo.eventType = type;
			parkingcarInfo.lotcode = roadID;
			parkingcarInfo.spaceCode = parkNo;
			parkingcarInfo.plateType = convertCarType(platetype);

			devToParkingInfoMap[serialcode] = parkingcarInfo;
			cout << "parkingPairQueue size: " << +parkingPairPq.size() << endl;
		
			break;
		}
		case CAR_MEDIA_UPLOAD: {
			//cout << "msgId: 0x0" << uppercase << hex << msgHeader.msgID << endl;
			if (msgHeader.isMultiPack) {
				uint8 *pData = &realData[16];

				short packNo = msgHeader.packIdx;
				short packCount = msgHeader.packCount;

				//printf("packNo = %d, packCount = %d\n", packNo, packCount);
				if (packNo == 1) { 
					int contentID = readByte2Int(pData, 0);
					uint8 mediaType = pData[4];
					uint8 mediaFormat = pData[5];
					uint8 event = pData[6];
					if (event != 0x51) {  // only handle parking event
						printf("Not parking event media\n");
						break;
					}

					uint8 channelID = pData[7];
					printf("mediaId: %d, channelID: %d\n", contentID, channelID);

					uint8 location[29] = {0};
					readBytes(pData, 8, location, 28);
					uint8 time[7] = {0};
					memcpy(time, &location[22], 6);
					char captureTimeStamp[13] = {0};
					int8ToHexStr(time, 0, 6, captureTimeStamp);
					printf("captureTimeStamp: %s\n", captureTimeStamp);

					int byteLen = len - 16 - 36 - 1;
					//printf("packNo=1, byteLen = %d\n", byteLen);
					vector<uint8> bytes(&pData[36], &pData[36] + byteLen);
				
					map<int, vector<uint8>> byteMap;
					byteMap[packNo] = bytes;

					mediaMap.erase(serialcode);
					mediaMap[serialcode] = byteMap;

				} else { // package index > 1
					//printf("mediaMap size: %d\n", mediaMap.size());

					map<int, vector<uint8>> byteMap;
					unordered_map<string, map<int, vector<uint8>>>::iterator iter;
					if ((iter = mediaMap.find(serialcode)) != mediaMap.end()) {
						byteMap = iter->second;
					} else {
						return;
					} 

					//cout << "byteMap last index: " << +byteMap.end()->first << ", packNo: " << +packNo << endl;
					if (byteMap.end()->first + 1 != packNo) {
						printf("pack loss and drop\n");
						return;
					}

					int byteLen = len - 16 - 1;
					vector<uint8> bytes(pData, pData + byteLen);
					//printf("packNo=%d, byteLen = %d\n", packNo, byteLen);

					byteMap[packNo] = bytes;
					mediaMap[serialcode] = byteMap;

					// rebuild image
					if (packNo == packCount) {
						int size = byteMap.size();
						printf("start save image..... byteMap size: %d\n", size);

						if (packCount != size) {
							printf("pack incomplete, drop\n");
							return;
						}

						unordered_map<string, ParkingCarInfo>::iterator it;
						it = devToParkingInfoMap.find(serialcode);
						if (it == devToParkingInfoMap.end()) {
							cout << "can't find specific parking info" << endl;
						}
						ParkingCarInfo currentCarInfo = it->second;

						vector<uint8> mediaContent;
						map<int, vector<uint8>>::iterator iter;

						int index = 0;
						for (iter = byteMap.begin(); iter != byteMap.end(); iter++) {
							// cout << "index = " << index++ << endl;
							std::move(iter->second.begin(), iter->second.end(), std::back_inserter(mediaContent));
						}

						string path = UPLOAD_ROOT_PATH + string(serialcode) + separator + currentCarInfo.spaceCode + separator
									+ to_string(currentCarInfo.camId) + underscore + string(currentCarInfo.carNo) + underscore 
									+ to_string(currentCarInfo.carProbability) + underscore + to_string(getCurrentTimeStamp()) + ".jpg";
						PRINT("", "save pic to : %s\n", path.c_str());
						writeToFile(mediaContent, path);

						addWaterMark(path, currentCarInfo.camId, currentCarInfo.spaceCode);


						ParkingCarMedia currMedia;
						currMedia.eventTime = getCurrentTimeStamp() / 1000; 
						currMedia.imageUrl = path;
						currMedia.eventType = 3;

						// push to the queue
						ParkingCarPair pair;
						pair.serialcode = serialcode;
						pair.carInfo = currentCarInfo;
						pair.carMedia = currMedia;
						parkingPairPq.push(pair);
					}
					
				}
			}
			break;
		}

	}
}

static void addWaterMark(string path, int camId, string spacecode) {
	Mat image = imread(path);
	if (image.empty()) {
		printf("file %s is not exist\n", path.c_str());
		return;
	}

	putText(image, spacecode.c_str(), cv::Point(10, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);

	// if (camId == CAM_FRONT) {
	// 	putText(image, "RF", cv::Point(1200, 650), CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
	// } else if (camId == CAM_BACK) {
	// 	putText(image, "RB", cv::Point(1200, 650), CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
	// } else {
	// 	putText(image, "RC", cv::Point(1200, 650), CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
	// }
	
	char text[256];
	sprintf(text,"[%.8f,%.8f]", g_gpsData.latlon.lat, g_gpsData.latlon.lon);
	putText(image, text, cv::Point(10, 680), CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);

	imwrite(path.c_str(), image);

	// memset(text, 0, sizeof(text));
	// sprintf(text,"20%02d-%02d-%02d %02d:%02d:%02d.%02d%d" , 
	// 	g_gpsData.gpsTime[0], g_gpsData.gpsTime[1], g_gpsData.gpsTime[2], g_gpsData.gpsTime[3], 
	// 	g_gpsData.gpsTime[4], g_gpsData.gpsTime[5], g_gpsData.gpsTime[6], g_gpsData.gpsTime[7]);
	// putText(image, text, cv::Point(10, 710),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);


}