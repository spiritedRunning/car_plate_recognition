#ifndef INCLUDE_COMM_UTIL_H
#define INCLUDE_COMM_UTIL_H

#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <stdint.h>
#include <memory.h>
#include <regex>
#include "iconv.h"
#include "../car_gbkcode.h"

using namespace std;

// eturn current time in milliseconds
inline int64_t getCurrentTimeStamp() {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


inline char* get_cur_time() {
	static char s[32] = {0};
	time_t t;
	struct tm* ltime;
	struct timeval stamp;

	gettimeofday(&stamp, NULL);
	ltime = localtime(&stamp.tv_sec);
	s[0] = '[';
	strftime(&s[1], 20, "%Y-%m-%d %H:%M:%S", ltime);
	sprintf(&s[strlen(s)], ".%03ld]", stamp.tv_usec / 1000);
	return s;
}

inline time_t convert2Timestamp(string& str) {
	long long num = strtoll(str.c_str(), nullptr, 10);
	if (num == 0) {
		return 0;
	}

	struct tm time_struct;
    strptime(str.c_str(), "%y%m%d%H%M%S", &time_struct);

    return mktime(&time_struct);
}

inline void writeToFile(vector<uint8_t> mediaContent, string filename) {
	printf("write to file: %s\n", filename.c_str());
	ofstream fp;
	fp.open(filename, ios::out | ios::binary);
	fp.write((char *)&mediaContent[0], mediaContent.size() * sizeof(uint8_t));

	fp.close();
}


inline vector<uint8_t> readDataFromFile(string file_path) {
	ifstream instream(file_path, ios::in | ios::binary);
    vector<uint8_t> data((istreambuf_iterator<char>(instream)), istreambuf_iterator<char>());
    return data;
}

inline vector<string> split_str(const string str, const string regex_str) {
	regex regexz(regex_str);
	return {sregex_token_iterator(str.begin(), str.end(), regexz, -1), sregex_token_iterator()};
}

inline string _lowercase(string s) {
	string ret;
	for (int i = 0; i < s.size(); i++) {
		ret[i] = tolower(s[i]);
	}
	return ret;
}

inline string _uppercase(string s) {
	string ret;
	for (int i = 0; i < s.size(); i++) {
		ret[i] = toupper(s[i]);
	}
	return ret;
}

inline short readByte2Short(uint8_t* src, int i) {
	return (src[i] << 8) | src[i + 1];
}

inline int readByte2Int(uint8_t* src, int i) {
	return (src[i] << 24) | (src[i + 1] << 16) | (src[i + 2] << 8) | src[i + 3];
}	

inline void readBytes(uint8_t* src, int start, uint8_t* dst, int len) {
	memcpy(dst, &src[start], len * sizeof(uint8_t));
}

inline void int8ToHexStr(uint8_t* src, int start, int len, char* dst) {
	for (int i = 0; i < len; i++) {
		sprintf(&dst[2 * i], "%02x", src[i]);
	}
	dst[2 * len + 1] = '\0';
}



inline string hex2Str(char* hex) {
	int len = strlen(hex);
    char ascii_string[len / 2 + 1];
    for (int i = 0; i < len; i += 2)  {
        sscanf(hex + i, "%2hhx", &ascii_string[i / 2]);
    }

    char gbk_string[128];

    // Set up a pointer to the input and output strings
    char *in_str = ascii_string;
    char *out_str = gbk_string;

    iconv_t cd = iconv_open("GBK", "ASCII");

    size_t in_len = sizeof(ascii_string);
    size_t out_len = sizeof(gbk_string);
    iconv(cd, &in_str, &in_len, &out_str, &out_len);

	return gbk_string;
}

inline void gbk_to_utf8(const char* src, char* dst, int len) {
    int ret = 0;
    size_t inlen = strlen(src) + 1;
    size_t outlen = len;

    // The iconv function in Linux requires non-const char *
    // So we need to copy the source string
    char* inbuf = (char *)malloc(len);
    char* inbuf_hold = inbuf;   // iconv may change the address of inbuf
                                // so we use another pointer to keep the address
    memcpy(inbuf, src, len);

    char* outbuf2 = NULL;
    char* outbuf = dst;
    iconv_t cd;

    // if src==dst, the string will become invalid during conversion 
	// since UTF-8 is 3 chars in Chinese but GBK is mostly 2 chars
    if (src == dst) {
		printf("gbk_to_utf8 src == dst\n");
        outbuf2 = (char*) malloc(len);
        memset(outbuf2, 0, len);
        outbuf = outbuf2;
    }

    cd = iconv_open("UTF-8", "GBK");
	printf("gbk_to_utf8 iconv_open: %d\n", cd);
    if (cd != (iconv_t)-1) {
        ret = iconv(cd, &inbuf, &inlen, &outbuf, &outlen);
        if (ret != 0)
            printf("iconv failed err: %s\n", strerror(errno));

        if (outbuf2 != NULL) {
            strcpy(dst, outbuf2);
			printf("iconv dst: %s\n", dst);
            free(outbuf2);
        }

        iconv_close(cd);
    } else {
		//printf("iconv_open err_msg: %s\n", explain_iconv_open("UTF-8", "GBK"));
	}
    free(inbuf_hold);   // Don't pass in inbuf as it may have been modified
}

static unsigned char bin2bcd(unsigned char bin) {
	return ((bin / 10) << 4) | (bin % 10);
}

inline void logfile(const char* format, ...) {
	va_list args;
	va_start(args, format);

	time_t timep;
	time(&timep);
	struct tm *p_time = gmtime(&timep);		

	uint8_t gpsTime[6] = {0};
	gpsTime[0] = bin2bcd(p_time->tm_year - 100);
	gpsTime[1] = bin2bcd(1 + p_time->tm_mon);
	gpsTime[2] = bin2bcd(p_time->tm_mday);
	gpsTime[3] = bin2bcd(p_time->tm_hour + 8);
	gpsTime[4] = bin2bcd(p_time->tm_min);
	gpsTime[5] = bin2bcd(p_time->tm_sec);
	
	int logFd = open("/mnt/sdcard/logger.log", O_RDWR | O_CREAT | O_APPEND);
	if (logFd < 0) {
		printf("open logger.log failed\n");
		exit(-1);
	}
	
	char logBuff[256] = {0};
	vsnprintf(logBuff, 256, format, args);
	//printf("[logfile]logBuff: %s\n", logBuff);
	//printf("gps [%02x-%02x-%02x %02x:%02x:%02x]\n", gpsTime[0], gpsTime[1], gpsTime[2], gpsTime[3], 
	//		gpsTime[4], gpsTime[5]);

	char logResult[512] = {0};
	sprintf(logResult, "[%02x-%02x-%02x %02x:%02x:%02x]: %s", gpsTime[0], gpsTime[1], gpsTime[2], gpsTime[3], 
			gpsTime[4], gpsTime[5], logBuff);
	//printf("[logfile] after sprintf: %s\n", logResult);		
	
	write(logFd, logResult, strlen(logResult));
	close(logFd);


	va_end(args);
}

inline void showArray(const char* str, const uint8_t *data, const int len) {
	printf("%s", str);
	for(int i = 0; i < len; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}

inline void showVectorArr(vector<uint8_t>& data) {
	for (size_t i = 0; i < data.size(); i++) {
		cout << setfill('0') << setw(2) << hex << (int)data[i] << " ";
	}
	cout << endl;
}

#endif