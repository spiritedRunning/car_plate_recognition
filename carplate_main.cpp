#include "carplate_main.h"
#include "util/chtext.h"
#include "car_gbkcode.h"

static char debugMsg[] = "$GPRMC,051504.900,A,3108.27655273,N,12038.57309193,E,3.5,158.8,191222,,,D*62\n";

static void* GpsThread(void* arg);

static void* CarPortDetThread(void* arg);

stGpsInformat* pGpsInfo;
stGpsInformat stGpsInfo = {0};
volatile uint8 GsmstatFlag;
uint8 devSeriNum[6];
int fileDataLen = 0;
uint8 actionCode = 0;
unsigned short takePhotoSeriNo;
uint8 fileDataBuff[1024 * 1024];
uint8 alarmflag = 0;


void ParseGpsDataToJTTGPS(stGpsInformat* pGpsInfo,GPSData* gpsData);

GPSData g_gpsData;
static LatLon g_pre_latlon;
static uint8 detFlag = 0; //启动车辆检测标记

typedef struct _CarportInfo {
    char areaId[50];
    char portId[50];
	LatLon bLatLon;
    LatLon eLatLon;
}CarportInfo;

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

uint8 takePhotoFlag = 0;

uint8 cameraType = 0;
float carnum_score = 0.5; 
float car_score = 0.5;
char activacode[64] = {0};
uint8 isRTSPEnable = 1;
float gpsoffset; //定位模块偏移量
uint8 angleRegion[2];
uint8 car_role = 0;  // 0 client, 1 server


float cosAngleMin = 0;
float cosAngleMax = 0;

cv::Rect vehicle_det_rect;

pthread_mutex_t pth_lock_port_init;
pthread_mutex_t pth_lock_gps;
pthread_mutex_t pth_lock_port;
static CarportInfo g_portInfo;
vector<CarportInfo> carportList;

static int baudrate = 115200; 
static uint8 uGpsBuff[1024];

static int frameErr = 0;

#define GPS_DEV "/dev/ttyS3"
#define LOGGER_PATH "/mnt/sdcard/logger.log"

#define SQRT2 sqrt(2)
#define SQRT3 sqrt(3)
#define PI 3.1415926
#define EARTH_RADIUS 6378137 //地球近似半径
#define EARTH_CIRCUMFERNCE EARTH_RADIUS*PI*2
#define PER_LON_METER 360 / (EARTH_CIRCUMFERNCE) //纬度方向1米对应度数

double radian(double d);
double angle(double d);
double get_distance(double lat1,double lng1,double lat2,double lng2);

// 求弧度
double radian(double d) {
    return d * PI / 180.0; //角度1˚ = π / 180
}

// 求角度
double angle(double d) {
    return d / PI * 180.0; //角度1˚ = π / 180
}

//计算距离
double get_distance(double lat1,double lng1,double lat2,double lng2) {
    double radLat1 = radian(lat1);
    double radLat2 = radian(lat2);
    double a = radLat1 - radLat2;
    double b = radian(lng1) - radian(lng2);
    double dst = 2 * asin((sqrt(pow(sin(a / 2),2) + cos(radLat1) * cos(radLat2) * pow(sin(b / 2),2) )));
    dst = dst * EARTH_RADIUS;
    dst= round(dst * 100) / 100;
    return dst;
}

int gpsLog = 0;

#define TEST_ARGB32_PIX_SIZE 4

static void set_argb8888_buffer(RK_U32 *buf, RK_U32 size, RK_U32 color) {
  for (RK_U32 i = 0; buf && (i < size); i++)
    *(buf + i) = color;
}


static bool quit = false;
static FILE *g_output_file;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static FILE *g_port_log_file;
static FILE *g_gps_log_file;
MPP_CHN_S g_stViChn;
MPP_CHN_S g_stVencChn;

static char optstr[] = "?::f:g:G:o:l:L:R:b:t:";
static const struct option long_options[] = {
    {"file", optional_argument, NULL, 'f'},
    {"gpsLog level", optional_argument, NULL, '?'},
    {"gpsData", optional_argument, NULL, 'G'},
    {"output_path", required_argument, NULL, 'o'},
    {"log path", required_argument, NULL, 'l'},
    {"gpslog path", required_argument, NULL, 'L'},
    {"rtsp", required_argument, NULL, 'R'},
    {"gps baudrate", required_argument, NULL, 'b'},
    {"camera type", required_argument, NULL, 't'},
    {"help", optional_argument, NULL, '?'},
    {NULL, 0, NULL, 0},
};

static void print_usage(const char *name) {
  printf("usage example:\n");
  printf("\t-f | carport folder, Default:\"carports\"\n");
  printf("\t-g | gps log print\n");
  printf("\t-L | gps log path\n");
  printf("\t-l | portinfo match log \n");
  printf("\t-R | enable RTSP \n");
  printf("\t-t | camera type:0-Rignt Front 1-Right Bcak 2-Right Center \n");
  printf("\t-o | --output_path: output path, Default:NULL\n");
  printf("\n");
}

int UtfToGbk(char* pszBufIn, int nBufInLen,char* pszBufOut)
{
	int i = 0;
	int j = 0, nLen;
	unsigned short unicode;
	unsigned short gbk;
	for(; i < nBufInLen; i++, j++)
	{
		if((pszBufIn[i] & 0x80) == 0x00)		
		{
			nLen = 1;
			pszBufOut[j]= pszBufIn[i];
		}
/*		
		else if((pszBufIn[i] & 0xE0) == 0xC0)// 2λ
		{
			nLen = 2;
			unicode = (pszBufIn[i] & 0x1F << 6) | (pszBufIn[i+1]& 0x3F);
		}*/
		else if ((pszBufIn[i] & 0xF0) == 0xE0) // 3λ 
		{

			if (i+ 2 >= nBufInLen) return -1; 
			unicode = (((int)(pszBufIn[i] & 0x0F)) << 12) | (((int)(pszBufIn[i+1] & 0x3F)) << 6) | (pszBufIn[i+2]  & 0x3F); 
			gbk = mb_uni2gb_table[unicode-0x4e00];
			pszBufOut[j]= gbk/256;
			pszBufOut[j+1] = gbk%256;
			j++;
			i+=2;
		}
		else
		{
			return -1;
		}
	}
	//*pnBufOutLen = j;
	//fprintf(stderr,"pnbufoutlen=%d\n",*pnBufOutLen);
	return 0;
}
//排序接口
void qusort(float *data, int *index, int start, int end) {
	int i, j;    //定义变量为基本整型
	i = start;    //将每组首个元素赋给i
	j = end;    //将每组末尾元素赋给j

	*data = *(data + start);
	*index = *(index + start);

	while (i < j)
	{
		while (i < j&&data[0] >= data[j])
			j--;    //位置左移
		if (i < j)
		{
			*(data + i) = *(data + j);
			*(index + i) = *(index + j);

			i++;    //位置右移
		}
		while (i < j&&data[i] >= data[0])
			i++;    //位置左移
		if (i < j)
		{
			*(data + j) = *(data + i);
			*(index + j) = *(index + i);
		}
	}

	*(data + i) = *data;
	*(index + i) = *index;

	if (start < i)
		qusort(data, index, start, j - 1);    //对分割出的部分递归调用qusort()函数
	if (i < end)
		qusort(data, index, j + 1, end);
}

//计算矩形框面积
float calcRectArea(XHLPR_API::Rect &rect) {
	return rect.width * rect.height;
}

void sort_vehicle(VehicleInfo *vehicle,int count,int *index) {
	if (count == 1) {
		index[1] = 0;
		// return 0;
	}
	else {
		float area[count + 1];
		area[0] = 0;
		for (int i = 0; i < count; i++) {
			area[i + 1] = calcRectArea(vehicle[i].vehicle_rect);
			index[i + 1] = i;
		}
		qusort(area, index, 1, count);
	}
}

cv::Mat nv12Frame;
cv::Mat overlay = Mat::zeros(95, FRAME_WIDTH, CV_8UC3);
#define SCALAR_WHITE Scalar(255, 255, 255)


XHLPR_SESS sess_vd; //车辆检测会话
XHLPR_SESS sess_pd; //车牌检测会话

CvxText cvxText("simsun.ttc");
int UTF2Unicode(wchar_t *wstr, int size, char *utf8)
{
    int size_s = strlen(utf8);
    int size_d = size;

    wchar_t *des =wstr;
    char *src = utf8;
    memset(des, 0, size * sizeof(wchar_t));

    int s = 0, d = 0;
//    bool toomuchbyte = true; //set true to skip error prefix.

    while (s < size_s && d < size_d)
    {
        unsigned char c = src[s];
        if ((c & 0x80) == 0) 
        {
            des[d++] += src[s++];
        } 
        else if((c & 0xE0) == 0xC0)  ///< 110x-xxxx 10xx-xxxx
        {
            wchar_t &wideChar = des[d++];
            wideChar  = (src[s + 0] & 0x3F) << 6;
            wideChar |= (src[s + 1] & 0x3F);
            s += 2;
        }
        else if((c & 0xF0) == 0xE0)  ///< 1110-xxxx 10xx-xxxx 10xx-xxxx
        {
            wchar_t &wideChar = des[d++];

            wideChar  = (src[s + 0] & 0x1F) << 12;
            wideChar |= (src[s + 1] & 0x3F) << 6;
            wideChar |= (src[s + 2] & 0x3F);

            s += 3;
        } 
        else if((c & 0xF8) == 0xF0)  ///< 1111-0xxx 10xx-xxxx 10xx-xxxx 10xx-xxxx 
        {
            wchar_t &wideChar = des[d++];

            wideChar  = (src[s + 0] & 0x0F) << 18;
            wideChar  = (src[s + 1] & 0x3F) << 12;
            wideChar |= (src[s + 2] & 0x3F) << 6;
            wideChar |= (src[s + 3] & 0x3F);
            s += 4;
        } 
        else 
        {
            wchar_t &wideChar = des[d++]; ///< 1111-10xx 10xx-xxxx 10xx-xxxx 10xx-xxxx 10xx-xxxx 

            wideChar  = (src[s + 0] & 0x07) << 24;
            wideChar  = (src[s + 1] & 0x3F) << 18;
            wideChar  = (src[s + 2] & 0x3F) << 12;
            wideChar |= (src[s + 3] & 0x3F) << 6;
            wideChar |= (src[s + 4] & 0x3F);
            s += 5;
        }
    }

    return d;
}

typedef struct plateupload_run_data_ {
    uint8 plate_number[32];
    uint8 plate_number_len;
    uint8 plate_number_score;
	int plate_type;
    int plate_color;
    uint8 port_number[50];
    uint8 port_number_len;
    uint8 area_id[50];
    stGpsInformat sGpsInfo;
	char path[128];
}plateupload_run_data;

class plateupload_safe_queue
{
private:
    //互斥锁
	mutable std::mutex mut;
	//缓存的待发送的数据
	std::queue<plateupload_run_data> data_queue;
	//条件变量
	std::condition_variable data_cond;
	//结束标志变量
	bool over;
	//结束时，缓存数据是否为空的标志变量
	bool flag;
public:
    plateupload_safe_queue() {
		over = false;
		flag = true;
	}

    //析构函数，释放内存
	~plateupload_safe_queue() {
	}

    //入队操作
	void push(plateupload_run_data new_value)
	{
		std::lock_guard<std::mutex> lk(mut);
		data_queue.push(new_value);
		data_cond.notify_one();
	}
	/*pop data_queue的数据，如果data_queue为空就一直等待
	over   data_queue.empty()      返回
	!over  data_queue.empty()      等待
	over   !data_queue.empty()     返回
	!over  !data_queue.empty()     返回
	*/
	void wait_and_pop(plateupload_run_data& value)
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {
            return ((!(data_queue.empty() ^ over)) || over); 
        });
		if (data_queue.empty()) { 
            flag = false; return; 
        };
		value = data_queue.front();
		data_queue.pop();
	}
	//重载wait_and_pop，与上相同，本demo中未使用
	std::shared_ptr<plateupload_run_data> wait_and_pop()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {
            return ((!(data_queue.empty() ^ over)) || over); 
        });
		if (data_queue.empty()) { 
            flag = false; return std::shared_ptr<plateupload_run_data>(); 
        }
		std::shared_ptr<plateupload_run_data> res(std::make_shared<plateupload_run_data>(data_queue.front()));
		data_queue.pop();
		return res;
	}
	//不管有没有队首元素，如果队列有元素就返回元素，并返回true，否则返回false，本demo中未使用
	bool try_pop(plateupload_run_data& value)
	{
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return false;
		std::cout << data_queue.size() << std::endl;
		value = data_queue.front();
		data_queue.pop();
		return true;
	}
	//重载try_pop，与上相同，本demo中未使用
	std::shared_ptr<plateupload_run_data> try_pop()
	{
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return std::shared_ptr<plateupload_run_data>();
		std::shared_ptr<plateupload_run_data> res(std::make_shared<plateupload_run_data>(data_queue.front()));
		data_queue.pop();
		return res;
	}
	//查看队列是否为空
	bool empty() const
	{
		std::lock_guard<std::mutex> lk(mut);
		return data_queue.empty();
	}
	//检测主线程是否结束
	bool is_over() {
		std::unique_lock<std::mutex> lk(mut);
		return over;
	}
	//检测主线程结束，over设置为true
	int set_over() {
		std::unique_lock<std::mutex> lk(mut);
		over = true;
		data_cond.notify_one();
		return 0;
	}
    //返回缓存检测车牌数量，本demo中未使用
	int size() {
		std::unique_lock<std::mutex> lk(mut);
		return data_queue.size();
	}

    void try_send(){
        cout << "start try_send" << endl;
        plateupload_run_data run_data;
		wait_and_pop(run_data);
        
		if (!flag || !isBubiaoConnected()) {
            printf("flag: %d, Bubiao connected: %d\n", flag, isBubiaoConnected());
            return;
        }

        cout << "start BB_PLATEUPLOAD" << endl;
        BB_PLATEUPLOAD(run_data.plate_number,run_data.plate_number_len,run_data.plate_number_score,run_data.plate_type,
            run_data.plate_color,run_data.port_number,run_data.port_number_len,run_data.area_id,&run_data.sGpsInfo,run_data.path);
    }
};

//车牌识别线程队列
plateupload_safe_queue  plateupload_queue;

//车牌识别线程函数
void *PlateUpload_run(void *threadarg)
{
    printf("-----------PlateUpload_run start------------\n");
	//车牌检测结束，并且队列待识别的数据为0则结束识别
	for (; !plateupload_queue.is_over() || !plateupload_queue.empty();) {
        cout << "is_over: " << plateupload_queue.is_over() << " size: " << plateupload_queue.size() << endl;
	    plateupload_queue.try_send();
    
	}

    printf("------------PlateUpload_run end------------\n");
	pthread_exit(NULL);
}

//缓存的检测图片和检测车牌信息结构体
typedef struct xhlpr_run_data_ {
    uint8 vdCount;
    uint8 pdCount;
	PlateInfo plate;
	cv::Mat img;
    CarportInfo portInfo;
    GPSData gpsData;
}xhlpr_run_data;

class threadsafe_queue
{
private:
	//互斥锁
	mutable std::mutex mut;
	//缓存的待识别的检测数据
	std::queue<xhlpr_run_data> data_queue;
	//缓存的待发送的识别数据
    std::vector<xhlpr_run_data> cache_port_plates;
	//最后一个发送的识别数据
	std::string last_plate;
    std::string last_port;
	//条件变量
	std::condition_variable data_cond;
	//缓存量，每隔多少缓存发送一次车牌号
	int cache_size = 3;
	//每缓存多少帧，清理一下前cache_size个缓存
	int clean_cache_size = 6;
	//检测结束标志变量
	bool over;
	//检测结束时，缓存数据是否为空的标志变量
	bool flag;
	//车牌识别会话
	XHLPR_SESS sess_pr;
	//预留接口，发送数据
	void send(std::string &str) {
		std::cout << "---------------" << str << std::endl;
	}
public:
	threadsafe_queue() {
		last_plate = "";
        last_port = "";
		over = false;
		flag = true;
	}
	/*车牌号过滤接口
		如果车牌首字母不为汉字，或者车牌号中间有汉字则返回false
	*/
	bool plateFilter(const char *str) {
		if (str[0] == '\0')return false;
		int s1 = int(str[0]);
		if (s1 >= 48 && s1 <= 90)return false;
		
		s1 = int(str[2]);
		if(s1 >=48 && s1 <= 90){
			if(s1 >= 48 && s1 <= 57)return false;
		}else{
			s1 = int(str[3]);
			if(s1 >= 48 && s1 <= 57)return false;
		}
		
		int str_len = 0;
		int c = -1;
		for (int i = 3; i < 20; i++) {
			if (str[i] == '\0') {
				str_len = i;
				break;
			}
			if (c == -1) {
				s1 = int(str[i]);
				if (s1 < 48 || s1 > 90) c = i;
			}
		}

		if (c == -1) return true;
		if (c < str_len - 3)return false;
		return true;
	}
	//创建车牌识别会话接口
	int create_sess() {
		return PRCreate(&sess_pr);
	}
	/*车牌识别推理接口
	1、从缓存的检测数据中读取检测图片与检测信息
	2、如果检测已结束并且队列为空就返回
	3、把读取的检测的数据送入识别接口
	4、分数大于0，并且过滤接口为true，把数据放入cache_port_plates
	*/
	int PRInference() {
		xhlpr_run_data run_data;
		wait_and_pop(run_data);
		if (!flag) {
            return LPR_OK;
        }

		uint8 logbuffer[256]={0};
		static int lognum = 0;
		struct timeval start, end;
	    double runtime_vd;       
        gettimeofday(&start, NULL);
        if(run_data.vdCount == 0 || run_data.pdCount == 0){
            cache_port_plates.push_back(run_data);
            return LPR_OK;
        }
		int ret = PlateOCR(sess_pr, run_data.img.data, run_data.img.cols, run_data.img.rows, &(run_data.plate));
        if (ret != LPR_OK) {
			return ret;
		}
        printf("%f %f %s %d %d \n",run_data.plate.points_score,run_data.plate.number_score,run_data.plate.plateNumber,run_data.plate.color,run_data.plate.type);
		gettimeofday(&end, NULL);
#if 0
        runtime_vd = ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec)) / 1000 + 0.5;		
		if(lognum <= 100)
		{
			sprintf(logbuffer,"Plate recognize Run time is: %lf ms \n" ,runtime_vd);
			savelog(logbuffer);
			lognum++;
		}
#endif
		
		if (run_data.plate.number_score > 0) {
			if (plateFilter(run_data.plate.plateNumber)) {
                cache_port_plates.push_back(run_data);
            }
		}
		return LPR_OK;
	}
	//入队操作
	void push(xhlpr_run_data new_value)
	{
		std::lock_guard<std::mutex> lk(mut);
		data_queue.push(new_value);
		data_cond.notify_one();
	}
	/*pop data_queue的数据，如果data_queue为空就一直等待
	over   data_queue.empty()      返回
	!over  data_queue.empty()      等待
	over   !data_queue.empty()     返回
	!over  !data_queue.empty()     返回
	*/
	void wait_and_pop(xhlpr_run_data& value)
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {
            return ((!(data_queue.empty() ^ over)) || over); 
        });
		if (data_queue.empty()) {
            flag = false; 
            return; 
        };
		value = data_queue.front();
		data_queue.pop();
	}
	//重载wait_and_pop，与上相同，本demo中未使用
	std::shared_ptr<xhlpr_run_data> wait_and_pop()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {
            return ((!(data_queue.empty() ^ over)) || over); 
        });
		if (data_queue.empty()) { 
            flag = false; 
            return std::shared_ptr<xhlpr_run_data>(); 
        }
		std::shared_ptr<xhlpr_run_data> res(std::make_shared<xhlpr_run_data>(data_queue.front()));
		data_queue.pop();
		return res;
	}
	//不管有没有队首元素，如果队列有元素就返回元素，并返回true，否则返回false，本demo中未使用
	bool try_pop(xhlpr_run_data& value)
	{
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty()) {
			return false;
        }
		std::cout << data_queue.size() << std::endl;
		value = data_queue.front();
		data_queue.pop();
		return true;
	}
	//重载try_pop，与上相同，本demo中未使用
	std::shared_ptr<xhlpr_run_data> try_pop()
	{
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty()) {
			return std::shared_ptr<xhlpr_run_data>();
        }
		std::shared_ptr<xhlpr_run_data> res(std::make_shared<xhlpr_run_data>(data_queue.front()));
		data_queue.pop();
		return res;
	}
	//查看队列是否为空
	bool empty() const
	{
		std::lock_guard<std::mutex> lk(mut);
		return data_queue.empty();
	}
	//检测主线程是否结束
	bool is_over() {
		std::unique_lock<std::mutex> lk(mut);
		return over;
	}
	//检测主线程结束，over设置为true
	void set_over(bool state) {
		std::unique_lock<std::mutex> lk(mut);
		over = state;
		data_cond.notify_one();
	}


	void try_send() {
        //cout << "cache_port_plates size: " << cache_port_plates.size() << endl;
        if(cache_port_plates.size() > 5){
#if 1
            vector<vector<xhlpr_run_data>> portPlates;
            vector<xhlpr_run_data> uploadVec;
            int i;
            string portId;
            //1.将缓存的记录按照车位号进行二次分组，每个车位号对应多个车牌记录。
            for(i=0;i<cache_port_plates.size();i++){
                std::string str = std::string(cache_port_plates[i].portInfo.portId);
                if(portId != str){
                    //新车位的记录
                    vector<xhlpr_run_data> ppvec;
                    ppvec.push_back(cache_port_plates[i]);
                    portPlates.push_back(ppvec);
                }else{
                    if(portPlates.size() == 0){
                        vector<xhlpr_run_data> ppvec;
                        ppvec.push_back(cache_port_plates[i]);
                        portPlates.push_back(ppvec);
                    }else{
                        //同一个车位的记录
                        portPlates[portPlates.size()-1].push_back(cache_port_plates[i]);
                    }
                }
                portId = str;
            }
            //2.对分组后车位号（1）-车牌（N）进行遍历
            for(i=0;i<portPlates.size();i++){
                vector<xhlpr_run_data> ppvec = portPlates[i];
                if(i == portPlates.size()-1){
                    //最后的数据是未匹配到，直接清空
                    if(ppvec[0].portInfo.portId[0] == '\0'){
                        cache_port_plates.erase(cache_port_plates.begin() + (cache_port_plates.size()- ppvec.size()),cache_port_plates.end());
                        ppvec.clear();
                        break;
                    }
                }

                //3.同车位号下车牌记录进行数据清理，1.将同一个车位重复的数据剔除 2.同一个车位无出牌/有出牌情况处理
                if(ppvec.size() > 15 || (i != portPlates.size() -1)){
                    for(int j=0;j<ppvec.size();j++){
                        int upFlag = -1;
						
                        if(ppvec[j].portInfo.portId[0] == '\0'){
                        	break;
                        }
                        std::string plateNumber = std::string(ppvec[j].plate.plateNumber);
                        std::string portId = std::string(ppvec[j].portInfo.portId);
                        float number_score = ppvec[j].plate.number_score;
                        if(j==0){
                            uploadVec.push_back(ppvec[j]);
                            continue;
                        }
                        for(int k=0;k<uploadVec.size();k++){
                            std::string plstr = std::string(uploadVec[k].plate.plateNumber);
                            std::string ptstr = std::string(uploadVec[k].portInfo.portId);
                            if(portId != ptstr){
                                continue;
                            }
                            if(plateNumber == plstr || (plateNumber.length()== 0 && plstr.length() != 0)){
                                upFlag = k;
                                break;
                            }
                        }
                        //printf("~~~~ vdCount : %d pdCount : %d portId:%s plateNumber:%s %.3f\n",ppvec[j].vdCount,ppvec[j].pdCount,portId.c_str(),plateNumber.c_str(),number_score);
                        if(plateNumber.length() != 0 ){
                            string prePlateNum = string(uploadVec[uploadVec.size() -1].plate.plateNumber);
                            if(prePlateNum.length() == 0){
                                uploadVec.erase(uploadVec.end());
                            }  
                        }

                        if(upFlag < 0){
                            uploadVec.push_back(ppvec[j]);
                        }else{
                            //printf("---- %d %d %d %0.8f %0.8f \n", upFlag,ppvec[j].vdCount,uploadVec[upFlag].vdCount,number_score,uploadVec[upFlag].plate.number_score);
                            //同车位 取概率最大的记录上传
                            if(number_score > uploadVec[upFlag].plate.number_score){
                                //printf("######### %d %0.8f %0.8f \n",upFlag,number_score,uploadVec[upFlag].plate.number_score);
                                uploadVec.erase(uploadVec.begin()+upFlag);
                                uploadVec.push_back(ppvec[j]);
                            }else if(number_score == 0 && uploadVec[upFlag].plate.number_score == 0){
                                if(ppvec[j].vdCount > 0 ){
                                    //未检测到车牌，根据摄像头角度 选择最早或者最晚的记录上传
                                    if(cameraType == 1){
                                    //最晚的记录上传
                                    uploadVec.erase(uploadVec.begin()+upFlag);
                                    uploadVec.push_back(ppvec[j]);
                                    }
                                }else if(ppvec[j].vdCount == 0 && uploadVec[upFlag].vdCount == 0){
                                    if(cameraType == 0 /*|| cameraType == 2*/){
                                        uploadVec.erase(uploadVec.begin()+upFlag);
                                        uploadVec.push_back(ppvec[j]);
                                    }
                                }
                            }
                        }
                    }
                    cache_port_plates.erase(cache_port_plates.begin(),cache_port_plates.begin()+ppvec.size());
                }
                
                ppvec.clear();
            }
            portPlates.clear();
#endif
			//int i;
			//vector<xhlpr_run_data> uploadVec = cache_port_plates;
            //cache_port_plates.clear();
                     
            //cout << "uploadVec size: " << uploadVec.size() << endl;
            //4.数据上传 
            for(i = 0 ;i < uploadVec.size(); i++){
				char mkdirCMD[100];
				char carplate_gbk[50] = {0};
                Mat image = uploadVec[i].img;
                GPSData gpsData = uploadVec[i].gpsData;
                CarportInfo portInfo = uploadVec[i].portInfo;
                PlateInfo plate = uploadVec[i].plate;
                std::string port = std::string(portInfo.portId);
                std::string plateStr = std::string(plate.plateNumber);
                int prerr = 0;
                std::string prResult;

                //printf("$$$$ %d %s_%s %s[%.3f] C:%d T:%d \n",i,uploadVec[i].portInfo.areaId,uploadVec[i].portInfo.portId,uploadVec[i].plate.plateNumber,plate.number_score,uploadVec[i].vdCount,uploadVec[i].pdCount);

                if(image.empty()){
                    continue;
                }
                if(plateStr.length() >0 && plate.number_score < carnum_score){
                    //车牌置信度过低情况忽略
                    continue;
                }
                if(last_port == port && (last_plate == plateStr || (last_plate.length() > 0 && plateStr.length() == 0))){
                    //连续相同车位号，1.连续相同的车牌记录忽略 2.上一次传输车牌不为空，当前识别记录为空的忽略
                    continue;
                }
                last_port = port;
                last_plate = plateStr;
                
                cv::Mat layroi = image(cv::Rect(0, FRAME_HEIGHT - overlay.rows, FRAME_WIDTH, overlay.rows));
        		addWeighted(layroi, 0.5, overlay, 0.5, 0, layroi);

                if (uploadVec[i].pdCount > 0){
                /*    
                if(isRTSPEnable){
                    cv::line(image, cv::Point(plate.points[0].x,plate.points[0].y),cv::Point(plate.points[1].x,plate.points[1].y) , SCALAR_WHITE, 2,LINE_AA);
                    cv::line(image, cv::Point(plate.points[1].x,plate.points[1].y),cv::Point(plate.points[2].x,plate.points[2].y) , SCALAR_WHITE, 2,LINE_AA);
                    cv::line(image, cv::Point(plate.points[2].x,plate.points[2].y),cv::Point(plate.points[3].x,plate.points[3].y) , SCALAR_WHITE, 2,LINE_AA);
                    cv::line(image, cv::Point(plate.points[3].x,plate.points[3].y),cv::Point(plate.points[0].x,plate.points[0].y) , SCALAR_WHITE, 2,LINE_AA);
               	}
                */
                }

                //putText(image, portInfo.portId, cv::Point(10, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                if(uploadVec[i].vdCount == 0){
                    prerr = 1;
                    prResult = "Empty";
                }else if(uploadVec[i].vdCount != 0 && plateStr.length() == 0){
                    //有车，但是没有检测到车牌
                    if(uploadVec[i].pdCount == 0){
                        //未检测到车牌
                        /*
                        if(isRTSPEnable){
                        putText(image, "PD Failed", cv::Point(320,650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                    	}
                        */
                        prerr = 2;
                        prResult="ERROR_DETECT_PLATE";
                    }else{
                        /*
                        if(isRTSPEnable){
                        //未识别出车牌
                        putText(image, "PR Failed", cv::Point(320,650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                    	}
                        */
                        prerr = 3;
                        prResult="ERROR_RECOGNIZE_PLATE";
                    }
                }else if(plateStr.length() > 0){
                    /*
                    if(isRTSPEnable){
                    wchar_t plateNumber[64];
                    UTF2Unicode(plateNumber,64,plate.plateNumber);
                    cvxText.putText(image, plateNumber, cv::Point(320,650));

                    char text[256];
                    sprintf(text,"[%.3f]",plate.number_score);
                    putText(image, text, cv::Point(435, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                	}
                    */
                }

                // add watermark here
                if (cameraType == 0) {
                    putText(image, "RF", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                } else if (cameraType == 1) {
                    putText(image, "RB", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                } else if (cameraType == 2) {
                    putText(image, "RC", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
                }
                
                // char text[256];
                // sprintf(text,"[%.8f,%.8f] [%.3f km/h]",gpsData.latlon.lat,gpsData.latlon.lon,gpsData.speed);
                // putText(image, text, cv::Point(10, 680),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);

                // memset(text, 0, sizeof(text));
                // sprintf(text,"20%02d-%02d-%02d %02d:%02d:%02d.%02d%d" , 
                //     gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],gpsData.gpsTime[6],gpsData.gpsTime[7]);
                // putText(image, text, cv::Point(10, 710),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);


				sprintf(mkdirCMD, "mkdir -p /mnt/sdcard/carplateimg/20%02d-%02d-%02d",gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2]);
				system(mkdirCMD);

                struct timeval tv;
                gettimeofday(&tv, NULL);
                char tmp[128];
                sprintf(tmp,"/mnt/sdcard/carplateimg/20%02d-%02d-%02d/%02d-%02d-%02d_%s_%s_%lld.jpg" , 
                    gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],
                    uploadVec[i].portInfo.portId,
                    prerr == 0 ? uploadVec[i].plate.plateNumber:prResult.c_str(),tv.tv_sec * 1000000LL + tv.tv_usec);


				//vector<int> compression_params;       
				//compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
				//compression_params.push_back(15); 
                imwrite(tmp, image);
                printf("@@@@ %d %s_%s %s \n",i,uploadVec[i].portInfo.areaId,portInfo.portId,uploadVec[i].plate.plateNumber);	
                plateupload_run_data upload_data;
                memset(&upload_data,0,sizeof(plateupload_run_data));

                memset(carplate_gbk, 0, sizeof(carplate_gbk));
				UtfToGbk(uploadVec[i].plate.plateNumber,strlen(uploadVec[i].plate.plateNumber),carplate_gbk);
                memcpy(upload_data.plate_number,carplate_gbk,strlen(carplate_gbk));
                upload_data.plate_number_score = (uploadVec[i].plate.number_score*100);
                upload_data.plate_type = uploadVec[i].plate.type;
                upload_data.plate_color = uploadVec[i].plate.color;
                memcpy(upload_data.port_number,portInfo.portId,strlen(portInfo.portId));
                upload_data.port_number_len=strlen(portInfo.portId);

                memcpy(upload_data.area_id,portInfo.areaId,strlen(portInfo.areaId));
                ParseGpsDataToJTTGPS(&upload_data.sGpsInfo,&gpsData);
                memcpy(upload_data.path,tmp,strlen(tmp));

                if(prerr == 0){
                    upload_data.plate_number_len = strlen(carplate_gbk);
                }else if(prerr == 1){            	
		            upload_data.plate_number_len = 0xFF;
                }else{					
					upload_data.plate_number_len = 0x0;
				}

                plateupload_queue.push(upload_data);
             
            }
            uploadVec.clear();  
        }
	}
    
	//返回缓存检测车牌数量，本demo中未使用
	int size() {
		std::unique_lock<std::mutex> lk(mut);
		return data_queue.size();
	}
	//析构函数，释放内存
	~threadsafe_queue() {
		//调用车牌识别会话销毁接口
		int ret = PRDestroy(&sess_pr);
		if (ret != LPR_OK) {
			std::cout << "PRDestroy ERROR:" << ret << std::endl;
		}
	}
};

//车牌识别线程队列
threadsafe_queue lpr_queue;

static void *GetVencBuffer(void *arg) {
  printf("#Start %s thread, arg:%p\n", __func__, arg);
  // init rtsp
  rtsp_demo_handle rtsplive = NULL;
  rtsp_session_handle session;
  //if(isRTSPEnable){
  if(1){
    rtsplive = create_rtsp_demo(554);
    session = rtsp_new_session(rtsplive, "/live/main_stream");
    rtsp_set_video(session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(session, rtsp_get_reltime(), rtsp_get_ntptime());
  }
  MEDIA_BUFFER mb = NULL;
  while (!quit) {
    mb = RK_MPI_SYS_GetMediaBuffer(g_stVencChn.enModId, g_stVencChn.s32ChnId,
                                   -1);
    if (mb) {
        if(isRTSPEnable){
            rtsp_tx_video(session, (uint8_t *)RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
                    RK_MPI_MB_GetTimestamp(mb));
        }
        if (g_output_file) {
            fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), g_output_file);
        } 
        RK_MPI_MB_ReleaseBuffer(mb);
    }
    if(isRTSPEnable){
        rtsp_do_event(rtsplive);
    }
  }
  // release rtsp
  if(isRTSPEnable){
    rtsp_del_demo(rtsplive);
  }
  return NULL;
}

//车牌识别线程函数
void *XHLPR_run(void *threadarg)
{
	//车牌检测结束，并且队列待识别的数据为0则结束识别
	for (; !lpr_queue.is_over() || !lpr_queue.empty();) {
		lpr_queue.PRInference();
	    lpr_queue.try_send();
	}
	std::cout << "End PRInference" << std::endl;
	pthread_exit(NULL);
}

static void *GetMediaBuffer(void *arg) {
    printf("#Start %s thread, arg:%p\n", __func__, arg);
    uint8 logbuffer[256]={0};
	int lognum = 0;
    MEDIA_BUFFER src_mb = NULL;

	
    while (!quit) {		    
        src_mb =
            RK_MPI_SYS_GetMediaBuffer(g_stViChn.enModId, g_stViChn.s32ChnId, -1);
        if (!src_mb) {
        printf("ERROR: RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
        break;
        }

        frameErr = 0;

        Mat algoFrame;
        nv12Frame.data = (uchar*)RK_MPI_MB_GetPtr(src_mb);
        

        cv::cvtColor(nv12Frame,algoFrame,CV_YUV2BGR_NV12);
        
        GPSData gpsData;
        pthread_mutex_lock(&pth_lock_gps);
        memcpy(&gpsData,&g_gpsData,sizeof(GPSData));
        pthread_mutex_unlock(&pth_lock_gps);

        CarportInfo portInfo;
        pthread_mutex_lock(&pth_lock_port);
        memcpy(&portInfo,&g_portInfo,sizeof(CarportInfo));
        pthread_mutex_unlock(&pth_lock_port);
        
        if(sess_vd != 0){
        cv::rectangle(nv12Frame, vehicle_det_rect, Scalar(0,0,0), 2);
		int vdcountlog=0;
		//detFlag = 1;
        if(detFlag > 0){
            xhlpr_run_data data;
            
            VehicleInfo *vehicle;
            int vd_count = 0;
            int vdCount = 0;

            int pd_count = 0;
            PlateInfo *plate;

            Mat vehicleImg = algoFrame(vehicle_det_rect).clone();
            int ret = VehicleDetect(sess_vd, vehicleImg.data, vehicleImg.cols, vehicleImg.rows, &vehicle, &vd_count);
            vehicleImg.release();
            if(vd_count > 0){
                for (int i = 0; i < vd_count; i++) {
                    vehicle[i].vehicle_rect.x += vehicle_det_rect.x;

                    float area = calcRectArea(vehicle[i].vehicle_rect);
                    //printf("%d %f %d %f %f %f %f %f \n",i,area, vehicle_det_rect.width*vehicle_det_rect.height / 3 * 2,vehicle[i].vehicle_rect.x,vehicle[i].vehicle_rect.y,vehicle[i].vehicle_rect.width,vehicle[i].vehicle_rect.height,vehicle[i].score);
                    cv::Rect rect(vehicle[i].vehicle_rect.x,vehicle[i].vehicle_rect.y,vehicle[i].vehicle_rect.width,vehicle[i].vehicle_rect.height);
                    if(isRTSPEnable){
                        cv::rectangle(nv12Frame, rect , Scalar(0, 0, 0), 2);
                	}
					if(vehicle[i].score < car_score)
					{
						continue;
					}
                    if(area > vehicle_det_rect.width*vehicle_det_rect.height / 3 * 2 || area < vehicle_det_rect.width*vehicle_det_rect.height / 8){
                        continue;
                    }
                    if(cameraType == 0){
                        if(rect.x+rect.width > (vehicle_det_rect.x + vehicle_det_rect.width - 5) || (rect.x + rect.width) < vehicle_det_rect.width / 3 + vehicle_det_rect.x){
                            continue;
                        }
                    } else if(cameraType == 1){
                        if(rect.x < vehicle_det_rect.x + 5 || rect.x  > (vehicle_det_rect.width / 3 * 2 + vehicle_det_rect.x)){
                            continue;
                        }
                    } else if(cameraType == 2){
                        if((rect.x < vehicle_det_rect.x + 10 && rect.width < vehicle_det_rect.width / 2) || (rect.x+rect.width > (vehicle_det_rect.width + vehicle_det_rect.x - 10) && rect.width < vehicle_det_rect.width / 2)){
                            continue;
                        }
                    }
                    
                    if(isRTSPEnable){
                        cv::rectangle(nv12Frame, rect, SCALAR_WHITE, 2);
                    }
                    vdCount++;
                    ret = PlateDetect(sess_pd, algoFrame.data, algoFrame.cols, algoFrame.rows, &(plate), &pd_count, vehicle+i);
                    if (pd_count > 0){
                        for (int j = 0; j < pd_count; j++) {
                            if(isRTSPEnable){
                            cv::line(nv12Frame, cv::Point(plate[j].points[0].x,plate[j].points[0].y),cv::Point(plate[j].points[1].x,plate[j].points[1].y) , SCALAR_WHITE, 2,LINE_AA);
                            cv::line(nv12Frame, cv::Point(plate[j].points[1].x,plate[j].points[1].y),cv::Point(plate[j].points[2].x,plate[j].points[2].y) , SCALAR_WHITE, 2,LINE_AA);
                            cv::line(nv12Frame, cv::Point(plate[j].points[2].x,plate[j].points[2].y),cv::Point(plate[j].points[3].x,plate[j].points[3].y) , SCALAR_WHITE, 2,LINE_AA);
                            cv::line(nv12Frame, cv::Point(plate[j].points[3].x,plate[j].points[3].y),cv::Point(plate[j].points[0].x,plate[j].points[0].y) , SCALAR_WHITE, 2,LINE_AA);
                            }
                        }
                        data.plate = *plate;
                        break;
                    }
                }
            }
            data.vdCount = vdCount;
            data.pdCount = pd_count;
            data.img = algoFrame;
            data.portInfo = portInfo;
            data.gpsData = gpsData;
            lpr_queue.push(data); 
			vdcountlog=vdCount;
        }
        }
	
        if(isRTSPEnable){
            for(int i=625;i<FRAME_HEIGHT;i++){
                for(int j = 0;j<nv12Frame.cols;j++){
                    nv12Frame.at<uchar>(i,j) = nv12Frame.at<uchar>(i,j)  / 2; 
                }
            }
            if(sess_vd == 0)
            {
                putText(nv12Frame, "XHLPR_Init Error", cv::Point(480, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }

            if(portInfo.portId[0] != '\0'){
                putText(nv12Frame, g_portInfo.portId, cv::Point(10, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }else{
                putText(nv12Frame, "Matching...", cv::Point(10, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }
            
            if(cameraType == 0){
                putText(nv12Frame, "RF", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }else if(cameraType == 1){
                putText(nv12Frame, "RB", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }else if(cameraType == 2){
                putText(nv12Frame, "RC", cv::Point(1200, 650),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }
            
            char text[256];
            if(gpsData.available){
                sprintf(text,"[%.8f,%.8f] [%.3f] [%.1f]",gpsData.latlon.lat,gpsData.latlon.lon,gpsData.speed / 3.6,gpsData.direct);
                putText(nv12Frame, text, cv::Point(10, 680),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);

                memset(text, 0, sizeof(text));
                sprintf(text,"Predict:[%.8f,%.8f]",g_pre_latlon.lat,g_pre_latlon.lon);
                putText(nv12Frame, text, cv::Point(640, 680),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);

                memset(text, 0, sizeof(text));
                sprintf(text,"20%02d-%02d-%02d %02d:%02d:%02d.%02d%d" , gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],gpsData.gpsTime[6],gpsData.gpsTime[7]);
                putText(nv12Frame, text, cv::Point(10, 710),CV_FONT_HERSHEY_COMPLEX_SMALL, 1.2, CV_RGB(255, 255, 255), 2);
            }
        }
        if(takePhotoFlag == 1){  // no use
            takePhotoFlag = 0;
            char mkdirCMD[100];
            sprintf(mkdirCMD, "mkdir -p /mnt/sdcard/carplateimg/20%02d-%02d-%02d",gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2]);
			system(mkdirCMD);
            char tmp[128];
            sprintf(tmp,"/mnt/sdcard/carplateimg/20%02d-%02d-%02d/%02d-%02d-%02d.%02d%d.jpg" , 
            gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],gpsData.gpsTime[6],gpsData.gpsTime[7]);

            Mat image;
            cv::cvtColor(nv12Frame,image,CV_YUV2BGR_NV12);
            imwrite(tmp,image);

            BB_PhotoUpload(tmp);
        }
        RK_MPI_SYS_SendMediaBuffer(g_stVencChn.enModId, g_stVencChn.s32ChnId,src_mb);
        RK_MPI_MB_ReleaseBuffer(src_mb);
        src_mb = NULL;
    }

    if (src_mb)
        RK_MPI_MB_ReleaseBuffer(src_mb);

    return NULL;
}

int readFileList(char *basePath,vector<string> *fileList,vector<string> *filenameList = NULL){
    DIR *dir;
    struct dirent *ptr;

    char base[1024];

    if((dir = opendir(basePath)) == NULL){
        perror("Open dir error !!!");
        return -1;
    }

    while((ptr = readdir(dir)) != NULL){
        if(strcmp(ptr->d_name,".") == 0 || strcmp(ptr->d_name,"..") == 0){
            continue;
        }else if(ptr->d_type == 8){ //file
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            fileList->push_back(base);
            if(filenameList != NULL){
                filenameList->push_back(ptr->d_name);
            }
        }else if(ptr->d_type == 4){ //dir
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            readFileList(base,fileList);
        }

    }
    closedir(dir);
    return 1;
}

int initCarportList(char *basePath){
    pthread_mutex_lock(&pth_lock_port_init);
    
    printf("Carport root path : %s \n",basePath);
    //读取车位目录下所有车位文件
    vector<string> fileVecs,fileNameVecs;
    readFileList(basePath,&fileVecs,&fileNameVecs);

    for(int index = 0;index < fileVecs.size(); index ++){
        std::string areaName = fileNameVecs[index];
        std::ifstream carport_file;
        carport_file.open(fileVecs[index]);
        if (!carport_file)
        {
            printf("Carport file %s not found\n",fileVecs[index]);
            continue;
        }

        carport_file.seekg(0, std::ios::beg);
        //char skipbuf[256];
        //carport_file.getline(skipbuf, sizeof(skipbuf)); //跳過第一行
        char delims[] = ",";
        int i;
        while(carport_file.peek() != EOF)
        {
            char buf[256] = {0};
            carport_file.getline(buf, sizeof(buf));
            //printf("%s \n",buf);

            CarportInfo carport;
            memset(&carport,0,sizeof(CarportInfo));

            strcpy(carport.areaId,areaName.c_str());
            char *substr= strtok(buf, delims);
            i = 0;
            while (substr != NULL) { 
                if (i == 0)  {
                    //编号
                    strcpy(carport.portId,substr);
                } else if ( i == 1 ) {
                    //起始位置纬度
                    carport.bLatLon.lat = atof(substr);
                } else if ( i == 2 ) {
                    //起始位置经度
                    carport.bLatLon.lon = atof(substr);
                } else if ( i == 3 ) {
                    //结束位置经度
                    carport.eLatLon.lat = atof(substr);
                } else if ( i == 4 ) {
                    //结束位置经度
                    carport.eLatLon.lon = atof(substr);
                }
                i++;
                substr = strtok(NULL,delims);
            }

            //数据有效，加入队列
            if (i == 5){
                carportList.push_back(carport);
            }  
        }
        carport_file.close();
    }
    fileVecs.clear();
    fileNameVecs.clear();
    
    pthread_mutex_unlock(&pth_lock_port_init);

    return 0;
}



int init_parmeter(char *parafile)
{	
    char line[128] = {0}; 
    int ret;
    FILE * fd = fopen(parafile, "r");
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
            if(cameraType == 2)   // center
            {
                angleRegion[0] = 85; //最小夹角的余弦值 默认85°
                angleRegion[1] = 85;

                vehicle_det_rect = cv::Rect(280,0,720,720);
            }
            else if(cameraType == 0)  // front
            {
                angleRegion[0] = 105; //最小夹角的余弦值 默认105°
                angleRegion[1] = 160; //最大夹角的余弦值 默认160°

                vehicle_det_rect = cv::Rect(120,0,840,720);
            }
            else if(cameraType == 1)  // back
            {
                angleRegion[0] = 105; //最小夹角的余弦值 默认105°
                angleRegion[1] = 160; //最大夹角的余弦值 默认160°

                vehicle_det_rect = cv::Rect(320,0,840,720);
            }
            cosAngleMin = cos(radian(angleRegion[0]));
            cosAngleMax = cos(radian(angleRegion[1]));
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
            int angleMax,angleMin;
			ret = sscanf(line, "%*[^=]=[%d,%d]", &angleMin,&angleMax); 
            if(ret == 2)
            {   
                angleRegion[0] = angleMin;
                angleRegion[1] = angleMax;
                cosAngleMin = cos(radian(angleRegion[0]));
                cosAngleMax = cos(radian(angleRegion[1]));
            }
		}
        if (memcmp(line, "car_role", strlen("car_role")) == 0) { // check master server or slave client
            sscanf(line, "%*[^=]=%d", &car_role); 
            printf("current role: %d\n", car_role);
        }
        if (memcmp(line, "master_server_ip", strlen("master_server_ip")) == 0) {
            sscanf(line, "%*[^=]=%s", &masterServerIP); 
            int len = strlen(masterServerIP);
            masterServerIP[len - 1] = '\0';
            printf("masterServerIP = %s\n", masterServerIP);
        }
    }
	printf("##### The carnum_score is %f,the cameraType is %d,car_score is %f,activacode is %s,rtspenable is %d,gpsoffset is %f \
                ,angle_region is [%f,%f], vehicle_roi is [%d,%d,%d,%d], car_role = %d, master_server_ip = %s \n\n",\
                carnum_score,cameraType,car_score,activacode,isRTSPEnable,gpsoffset,cosAngleMin,cosAngleMax,vehicle_det_rect.x,\
                vehicle_det_rect.y,vehicle_det_rect.width,vehicle_det_rect.height, car_role, masterServerIP);
    fclose(fd);	
	return 0;
}

static void TFshell(const char* command, char* buffer) {
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

void* TFdetectThread(void* arg) {
	char tmp[1024] = { 0 };
	int available =0;

    sleep(30);
	while(!quit)
	{
		TFshell("df |grep mmcblk2p1 | awk '{ print $4}'", tmp);
		available = atoi(tmp);
		// printf("available %d GB \n",available / 1024 / 1024);
		if(strlen(tmp) == 0)
		{
			printf("not found disk\n");
		}
		else if(available<1000000)//1gb  
		{
			printf("space not enough start delete \n");

			chdir("/mnt/sdcard/carplateimg/");

			system(" ls -1 |awk '{print i$0}' i=`pwd`'/'|head -n 1 | xargs rm -rf");
			system("sync");

			printf("available space: %d GB\n", available / 1024 / 1024);
		}

		sleep(30 * 60);
	}
	return NULL;
}

void TF_dectect()
{
    pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thTF;
	pthread_create(&thTF, &attr, TFdetectThread, NULL);
}

void checkLoggerFile() {
    struct stat file_stat;
    time_t current_time;
    double difference;

    int ret = stat(LOGGER_PATH, &file_stat);
    printf("stat ret = %d\n");
    if (stat(LOGGER_PATH, &file_stat) == 0) {
        time(&current_time);

        difference = difftime(current_time, file_stat.st_mtime);
        printf("difference = %f\n", difference);

        if (difference > 24 * 60 * 60) {
            if (remove(LOGGER_PATH) != 0) {
                printf("logger file expired, deleting it!\n");
            }
        }
    }
}

int main(int argc, char *argv[]) 
{
    printf("Carport match process \n");

    system("v4l2-ctl -d /dev/v4l-subdev4 --set-ctrl \"band_stop_filter=0\"");

    char* carport_file_root = "/data/carports";
    memset(&g_pre_latlon,0,sizeof(LatLon));
    memset(&g_gpsData,0,sizeof(GPSData));
    RK_CHAR *pOutPath = NULL;
    RK_CHAR *pGpsLogPath = "/mnt/sdcard/gps.txt";
    RK_CHAR *pPortLogPath = NULL;
    int c;

	BB_ReadDevSeriNum();

	BB_RecSndServInit();
	BB_LocTaskStart();
	BB_KeepAlive();

	TF_dectect();
    checkLoggerFile();
	
    while ((c = getopt_long(argc, argv, optstr, long_options, NULL)) != -1) {
        const char *tmp_optarg = optarg;
        switch (c) {
        case 'f':
            if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
                tmp_optarg = argv[optind++];
            }
            if (tmp_optarg) {
                carport_file_root = (char *)tmp_optarg;
            } 
        break;
        case 'g':
            if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
                tmp_optarg = argv[optind++];
            }
            if (tmp_optarg) {
                gpsLog = atoi(tmp_optarg);
            } 
        break;
        case 'G':
            if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
                tmp_optarg = argv[optind++];
            }
            if (tmp_optarg) {
                strcpy(debugMsg,(char *)tmp_optarg);
            } 
        break;
        case 'o':
            pOutPath = optarg;
        break;
        case 'l':
            pPortLogPath = optarg;
        break;
        case 'L':
            pGpsLogPath = optarg;
        break;
        case 'R':
            isRTSPEnable = atoi(tmp_optarg);
        break;
        case 'b':
            baudrate = atoi(tmp_optarg);
        break;
        case 't':
            cameraType = atoi(tmp_optarg);
        break;
        case '?':
        default:
        print_usage(argv[0]);
        return 0;
        }
    }


	init_parmeter("/root/app/conf/car_para.txt");
    if (pPortLogPath) {
        g_port_log_file = fopen(pPortLogPath, "w");
    }

    if (pGpsLogPath) {
        g_gps_log_file = fopen(pGpsLogPath, "w");
    }

    initCarportList(carport_file_root);
   
    pthread_t thGps;
	pthread_create(&thGps, NULL, GpsThread, NULL);

    usleep(100000);

    pthread_t thPortDet;
	pthread_create(&thPortDet, NULL, CarPortDetThread, NULL);


    if (car_role == 1) { // server
        initBubiaoServer();    
        startUploadService();
    } else { // client
        initClientSocket();
    }

    if (pOutPath) {
        g_output_file = fopen(pOutPath, "w");
        if (!g_output_file) {
            printf("ERROR: open file: %s fail, exit\n", pOutPath);
            return 0;
        }
    }

    RK_S32 ret;

    RK_U32 u32Width = FRAME_WIDTH;
	RK_U32 u32Height = FRAME_HEIGHT;
    nv12Frame = Mat::zeros(u32Height + u32Height / 2, u32Width, CV_8UC1);
	int frameCnt = -1;
	RK_CHAR *device_name = "rkispp_scale0";
	RK_CHAR *iq_file_dir = "/oem/etc/iqfiles";
	RK_S32 s32CamId = 0;
#ifdef RKAIQ
    RK_BOOL bMultictx = RK_FALSE;

    printf("#Rkaiq XML DirPath: %s\n", iq_file_dir);
    printf("#bMultictx: %d\n\n", bMultictx);
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;//RK_AIQ_WORKING_MODE_NORMAL;//RK_AIQ_WORKING_MODE_ISP_HDR2;//RK_AIQ_WORKING_MODE_ISP_HDR3;
    int fps = 30;
    SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, iq_file_dir);
    SAMPLE_COMM_ISP_Run(s32CamId);
    SAMPLE_COMM_ISP_SET_mirror(s32CamId,0);
    SAMPLE_COMM_ISP_SetFrameRate(s32CamId, fps);
#endif

    RK_MPI_SYS_Init();
    g_stViChn.enModId = RK_ID_VI;
    g_stViChn.s32DevId = s32CamId;
    g_stViChn.s32ChnId = 1;
    g_stVencChn.enModId = RK_ID_VENC;
    g_stVencChn.s32DevId = 0;
    g_stVencChn.s32ChnId = 0;

    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = device_name;
    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = u32Width;
    vi_chn_attr.u32Height = u32Height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    ret = RK_MPI_VI_SetChnAttr(g_stViChn.s32DevId, g_stViChn.s32ChnId,
                                &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(g_stViChn.s32DevId, g_stViChn.s32ChnId);
    if (ret) {
        printf("ERROR: Create vi[0] failed! ret=%d\n", ret);
        return -1;
    }

    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
    venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
    venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
    venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
    venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
    venc_chn_attr.stVencAttr.u32Profile = 77;
    venc_chn_attr.stVencAttr.enRotation = (VENC_ROTATION_E)0;

#if 1
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 15;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = u32Width * u32Height * 5;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
#else
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
    venc_chn_attr.stRcAttr.stH264Vbr.u32Gop = 15;
    venc_chn_attr.stRcAttr.stH264Vbr.u32MaxBitRate = u32Width * u32Height * 5;
    venc_chn_attr.stRcAttr.stH264Vbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Vbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH264Vbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Vbr.u32SrcFrameRateNum = 30;
#endif

    RK_MPI_VENC_CreateChn(g_stVencChn.s32ChnId, &venc_chn_attr);

    pthread_t read_thread;
    pthread_create(&read_thread, NULL, GetMediaBuffer, NULL);
    pthread_t venc_thread;
    pthread_create(&venc_thread, NULL, GetVencBuffer, NULL);

    usleep(1000); // waite for thread ready.
    ret = RK_MPI_VI_StartStream(g_stViChn.s32DevId, g_stViChn.s32ChnId);
    if (ret) {
        printf("ERROR: Start Vi[0] failed! ret=%d\n", ret);
        return -1;
    }

    printf("%s initial finish\n", __func__);
    signal(SIGINT, sigterm_handler);

    //初始化车牌识别
	/*std::ifstream activation_file;
	activation_file.open("./licSever/activation.conf");
	if (!activation_file)
	{
		std::cout << "activation.conf not found" << std::endl;
		return -1;
	}
	std::string activation_code;
	getline(activation_file, activation_code);
	activation_file.close();
	std::cout << "activation_code: " << (char*)activation_code.data() << std::endl;
    */

    float version;
    XHLPRAPI_Version(&version);

	static int activetimes = 0;
	//程序开始调用初始化接口初始化激活
	while(!quit)
	{
		sleep(1);
		ret = XHLPRInit("./licSever", activacode);
		if (ret != LPR_OK) {
			std::cout << "XHLPRInit ERROR:" << ret << std::endl;
			activetimes++;
			if( activetimes>5)
			{
				printf(">>>>>>XHLPRInit ERROR:>>>>>>>Trigger activation code failure alarm\n");
				alarmflag = 1;
				activetimes = 0;
			}
            XHLPRFinal();
			continue;
		}

		//新建车辆检测会话
		ret = VDCreate(&sess_vd,"best");
		if (ret != LPR_OK) {
			std::cout << "VDCreate ERROR:" << ret << std::endl;
			continue;
		}

		//新建车牌检测会话
		ret = PDCreate(&sess_pd);
		if (ret != LPR_OK) {
			std::cout << "PDCreate ERROR:" << ret << std::endl;
			continue;
		}

	    ret = lpr_queue.create_sess();
		if (ret != LPR_OK) {
			std::cout << "PRCreate ERROR:" << ret << std::endl;
			continue;
		}
		alarmflag = 2;
		break;
	}

    pthread_t pthPUP;
    pthread_create(&pthPUP, NULL, PlateUpload_run, NULL);

    pthread_t pthPR;
    pthread_create(&pthPR, NULL, XHLPR_run, NULL);

    while (!quit) {
        frameErr++;
        if(frameErr > 10){
            system("killall carplate_main");
        }
        usleep(500000);
    }

    if (car_role == 1) {
        setMasterServerTerminate();
    }

    printf("%s exit!\n", __func__);
    pthread_join(read_thread, NULL);
    pthread_join(venc_thread, NULL);

    RK_MPI_VENC_DestroyChn(g_stVencChn.s32ChnId);
    RK_MPI_VI_DisableChn(g_stViChn.s32DevId, g_stViChn.s32ChnId);

    pthread_join(thGps, NULL);
    pthread_join(thPortDet, NULL);
    
#ifdef RKAIQ
    SAMPLE_COMM_ISP_Stop(s32CamId);
#endif

    //检测结束，调用set_over
	lpr_queue.set_over(true);
	//等待识别线程结束
	pthread_join(pthPR, NULL);

    plateupload_queue.set_over();
    pthread_join(pthPUP, NULL);

    VDDestroy(&sess_vd);
    PDDestroy(&sess_pd);
    XHLPRFinal();

    GPSData gpsData;
    pthread_mutex_lock(&pth_lock_gps);
    memcpy(&gpsData,&g_gpsData,sizeof(GPSData));
    pthread_mutex_unlock(&pth_lock_gps);

    char date[64];
    sprintf(date,"20%02d-%02d-%02d_%02d-%02d-%02d" , gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],pOutPath);
        
    if (g_output_file) {
        fclose(g_output_file);
        char tmp[256];
        sprintf(tmp,"%s_%s" , date,pOutPath);
        rename(pOutPath,tmp);
    }

    if(g_port_log_file){
        fclose(g_port_log_file);
        char tmp[256];
        sprintf(tmp,"%s_%s" , date,pPortLogPath);
        rename(pPortLogPath,tmp);
    }

    if(g_gps_log_file){
        fclose(g_gps_log_file);
        char tmp[256];
        sprintf(tmp,"%s_%s" , date,pGpsLogPath);
        rename(pGpsLogPath,tmp);
    }
    return 0;
}

//#define L_MAX -sqrt(2) / 2
#define DIS_Port_THRESHOLD 4 

static void* CarPortDetThread(void* arg) 
{
    LatLon preLatlon;
    while (!quit)
    {
        usleep(30000);

        GPSData gpsData;
        pthread_mutex_lock(&pth_lock_gps);
        memcpy(&gpsData,&g_gpsData,sizeof(GPSData));
        pthread_mutex_unlock(&pth_lock_gps);

        if(gpsData.available != 1){
			memset(&g_portInfo, 0, sizeof(CarportInfo));
            continue;
        }

        LatLon latlon = gpsData.latlon;
        if(preLatlon.lat ==latlon.lat && preLatlon.lon == latlon.lon){
            usleep(30000);
            continue;
        }
        preLatlon = latlon;

        //printf("LatLon : %.8lf,%.8lf %.3f \n",latlon.lat,latlon.lon,gpsData.speed);
        if(g_port_log_file){
            char buffer[256];
            sprintf(buffer,"20%02d-%02d-%02d %02d:%02d:%02d.%02d%d \nLatLon : %.8lf,%.8lf %.3f \n" , gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],gpsData.gpsTime[6],gpsData.gpsTime[7],latlon.lat,latlon.lon,gpsData.speed);
            fwrite(buffer,1,strlen (buffer),g_port_log_file);
        }

        //经纬度预测
        double angle = radian(gpsData.direct);
        double speed = gpsData.speed / 3.6;
      
        //经度方向对应1M的度数。  所在纬度周长 cos(radian(gpsData.latlon.lat)) * EARTH_CIRCUMFERNCE
        double lon_per_m = 360 / (cos(radian(gpsData.latlon.lat)) * EARTH_CIRCUMFERNCE ); 

        double lat_ms = cos(angle)  * PER_LON_METER * gpsoffset; //纬度方向移动度数
        double lon_ms = sin(angle)  * lon_per_m * gpsoffset; //经度方向移动度数

        //预测移动后经纬度
        double latPre = gpsData.latlon.lat + lat_ms; 
        double lonPre = gpsData.latlon.lon + lon_ms;

        g_pre_latlon.lat = latPre;
        g_pre_latlon.lon = lonPre;

        double dis = get_distance( gpsData.latlon.lat, gpsData.latlon.lon,latPre,lonPre);
        //printf("############### Direct:%.1f Speed:%.3f Lat_Pre:%.8f Lon_Pre:%.8f Dis_Pre:%.5f\n",gpsData.direct,speed,latPre,lonPre,dis);
        if(g_port_log_file){
            char buffer[256];
            sprintf(buffer,"############### Direct:%.1f Speed:%.3f Lat_Pre:%.8f Lon_Pre:%.8f Dis_Pre:%.5f\n",gpsData.direct,speed,latPre,lonPre,dis);
            fwrite(buffer,1,strlen (buffer),g_port_log_file);
        }
        latlon.lat = latPre;
        latlon.lon = lonPre;

        pthread_mutex_lock(&pth_lock_port_init);

        int id_matched = -1;

        double cosMax = -1;
        
        int matching = 0;
        int i;
        for(i=0;i<carportList.size();i++){
            LatLon bLatLon = carportList[i].bLatLon;
            LatLon eLatLon = carportList[i].eLatLon;
            if(cameraType == 0){
                if(!(bLatLon.lat  > latlon.lat - 0.0001 && bLatLon.lat < latlon.lat + 0.0001)){
                    continue;
                }
                if(!(bLatLon.lon > latlon.lon - 0.0001 && bLatLon.lon < latlon.lon + 0.0001)){
                    continue;
                }
            } else if(cameraType == 1){
                if(!(eLatLon.lat  > latlon.lat - 0.0001 && eLatLon.lat < latlon.lat + 0.0001)){
                    continue;
                }
                if(!(eLatLon.lon > latlon.lon - 0.0001 && eLatLon.lon < latlon.lon + 0.0001)){
                    continue;
                }
            } else if(cameraType == 2){
                if(!((bLatLon.lat + eLatLon.lat) / 2  > latlon.lat - 0.0001 && (bLatLon.lat + eLatLon.lat) / 2 < latlon.lat + 0.0001)){
                    continue;
                }
                if(!((bLatLon.lon +  eLatLon.lon) / 2 > latlon.lon - 0.0001 && (bLatLon.lon +  eLatLon.lon) / 2 < latlon.lon + 0.0001)){
                    continue;
                }
            }

            //计算所有点的距离
            //车位起始点距离
            double dis_b = get_distance( latlon.lat, latlon.lon,bLatLon.lat,bLatLon.lon);
            //车位结束点距离
            double dis_e = get_distance( latlon.lat, latlon.lon,eLatLon.lat,eLatLon.lon);
            //车位长度
            double dis_port = get_distance( bLatLon.lat,bLatLon.lon,eLatLon.lat,eLatLon.lon);

            double l = (dis_b+dis_e+dis_port)/2;     //周长的一半   
            double s = sqrt(l*(l-dis_b)*(l-dis_e)*(l-dis_port));  //海伦公式求面积 
            double h = 2*s/dis_port; //到车位的垂直距离

            //车位起始点所在角的余弦值
            //cosb = (a*a+c*c-b*b) / (2 * a * c);
            double cosLB = (dis_b * dis_b + dis_port * dis_port - dis_e * dis_e ) / ( 2 * dis_b * dis_port);
            double cosLE = (dis_e * dis_e + dis_port * dis_port - dis_b * dis_b ) / ( 2 * dis_e * dis_port);
            
#ifdef PREDICT_ENABLE
            double l_max = (dis_port/2) * SQRT2;//距离车位起始点的最大距离
#else
            double l_max = (dis_port/2) * SQRT3;//距离车位起始点的最大距离
#endif
            
            double h_max = dis_port / 2;
            if(dis_port < DIS_Port_THRESHOLD){
                //小于4米判定为纵向车位，前后摄像头不进行匹配检测
                if(cameraType == 2 ){
                    h_max = dis_port * 2;
                    l_max = dis_port * 2;
                }else {
                    continue;
                }
            }else {
                if(cameraType == 2 ){
                    continue;
                }
            }
            matching++;

            //printf("%s lB:%.3lf lE:%.3f h:%.3lf LB:%.3lf LE:%.3lf l_max:%.3f h_max:%.3f \n",carportList[i].portId,dis_b,dis_e,h,acos(cosLB) / PI * 180,acos(cosLE) / PI * 180,l_max,h_max);
            if(g_port_log_file){
                char buffer[256];
                sprintf(buffer,"%s lB:%.3lf lE:%.3f h:%.3lf LB:%.3lf LE:%.3lf l_max:%.3f h_max:%.3f \n",carportList[i].portId,dis_b,dis_e,h,acos(cosLB) / PI * 180,acos(cosLE) / PI * 180,l_max,h_max);
                fwrite(buffer,1,strlen (buffer),g_port_log_file);
            }

            //垂直距离 > 1/2车位宽度
            if(h > h_max){
                continue;
            }

            if(cameraType == 0){ //RF Camera
                if(dis_b > l_max){
                    continue;
                }
                //过滤锐角情况
                if(cosLB > 0){
                    continue;
                }
            
                if((cosLB < cosAngleMin && cosLB > cosAngleMax) && cosMax < cosLB){ //获取最小角度
                    cosMax = cosLB;
                    id_matched = i;
                }
            } else if(cameraType == 1){ //RB Camera
                if(dis_e > l_max){
                    continue;
                }
                //过滤锐角情况
                if(cosLE > 0){
                    continue;
                }
            
                if((cosLE < cosAngleMin && cosLE > cosAngleMax) && cosMax < cosLE){ //获取最小角度
                    cosMax = cosLE;
                    id_matched = i;
                }
            } else if(cameraType == 2){ //RC Camera
                if(cosLB < cosAngleMin || cosLE < cosAngleMin){
                    continue;
                }
                id_matched = i;
            } else {
                continue;
            }
            
        }

        
        //id_matched = 2;  // todo 
        //matching = 1;
        cout << "id_matched: " << id_matched << endl;
        if(id_matched != -1){
            if(g_port_log_file){
                char buffer[256];
                sprintf(buffer,"@@@ Probability carport : %s \n\n",carportList[id_matched].portId ); 
                fwrite(buffer,1,strlen (buffer),g_port_log_file);
            }
            pthread_mutex_lock(&pth_lock_port);
            memset(&g_portInfo, 0, sizeof(CarportInfo));
            memcpy(&g_portInfo,&carportList[id_matched],sizeof(CarportInfo));
            pthread_mutex_unlock(&pth_lock_port);
            //printf("Fresh carport result ,Press anykey to continue \n"); 
            //getchar();
            printf("@@@ Probability carport : %s \n",carportList[id_matched].portId); 
            //printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");  
           
            //printf("Press anykey to continue \n\n"); 
            //getchar();
        } else {
            pthread_mutex_lock(&pth_lock_port);
            memset(&g_portInfo, 0, sizeof(CarportInfo));
            pthread_mutex_unlock(&pth_lock_port);
        }

        detFlag = matching > 0 ? 1 : 0;
        pthread_mutex_unlock(&pth_lock_port_init);
    }
    return NULL;
}

static int GetComma(int num, char *str) {
	int i, j = 0;
	int len = strlen(str);

	for (i = 0; i < len; i++) {
		if (i > 1023)	// 防止溢出
			return 0;

		if (str[i] == ',') {
			j++;
		}
		if (j == num) {
			return i + 1;
		}
	}
	return 0;
}

static float GetSpeed(char* src) {
	int i;
	float speed_f = 0;
	char buf[20];

	i = GetComma(1, src);
	if (i < 2)
		return 0;
	strncpy(buf, src, i);
	return atof(buf) * 1.85;
}

static float GetData(char* src) {
	int i;
	char buf[20];

	i = GetComma(1, src);
	if (i < 2)
		return 0;
	strncpy(buf, src, i);
	return atof(buf);
}

static double GetTitude(char* src) {
    int i;
	char* ttdUp = NULL;
	char* ttdLow = NULL;
	char buf[20];

	i = GetComma(1, src);
	if (i < 2)
		return 0 ;
	strncpy(buf, src, i);
	buf[i - 1] = '\0';

	int dataUp = 0;
	int dataLow = 0;
	//分别将国标与部标的整数与小数部分分离

	if (i > 6) {
		ttdUp = strtok(buf, ".");
		ttdLow = strtok(NULL, ".");
	}

	dataUp = atoi(ttdUp);
	dataLow = atoi(ttdLow);

    return (dataUp / 100 + (dataUp % 100 + dataLow / 100000000.0) / 60);
}

static void UTC2BTC(volatile uint8* data){
	data[5]++;
	if(data[5]>59){
		data[5]=0;
		data[4]++;
		if(data[4]>59){
			data[4]=0;
			data[3]++;
		}
	}
	data[3]+=8;
	if(data[3]>23){
		data[3]=0;
		data[2]+=1;
	}
	if(data[1]==2){
		if(data[0]%100==0){
			if(data[0]%400==0){
				if(data[2]>29)
				{
					data[2]=1;
					data[1]++;
				}
			}else{
				if(data[2]>28){
					data[2]=1;
					data[1]++;
				}
			}
		}else{
			if(data[0]%4==0){
				if(data[2]>29)
				{
					data[2]=1;
					data[1]++;
				}
			}
			else{
				if(data[2]>28){
					data[2]=1;
				     data[1]++;
			    }
			}
		}
	}else if(data[1]==4||data[1]==6||data[1]==9||data[1]==11){
		if(data[2]>30){
			data[2]=1;
			data[1]++;
		}
	}else if(data[1]==1||data[1]==3||data[1]==5||data[1]==7||data[1]==8||data[1]==10||data[1]==12){
		if(data[2]>31){
			data[2]=1;
			data[1]++;
		}
	}
	if(data[1]>12){
		data[1]=1;
		data[0]++;
	}
}


static void Utc2Btc(volatile uint8* data) {
	data[5]++;
	if (data[5] > 59) {
		data[5] = 0;
		data[4]++;
		if (data[4] > 59) {
			data[4] = 0;
			data[3]++;
		}
	}

	data[3] = data[3] + 8;
	if (data[3] > 23) {
		data[3] -= 24;
		data[2] += 1;
		if (data[1] == 2 || data[1] == 4 || data[1] == 6 || data[1] == 9
				|| data[1] == 11) {
			if (data[2] > 30) {
				data[2] = 1;
				data[1]++;
			}
		} else {
			if (data[2] > 31) {
				data[2] = 1;
				data[1]++;
			}
		}
		if (data[0] % 4 == 0) {
			if (data[2] > 29 && data[1] == 2) {
				data[2] = 1;
				data[1]++;
			}
		} else {
			if (data[2] > 28 && data[1] == 2) {
				data[2] = 1;
				data[1]++;
			}
		}
		if (data[1] > 12) {
			data[1] -= 12;
			data[0]++;
		}
	}

	int i = 0;
	for (i = 0; i < 6; i++) {
		data[i] = DecToBCD(data[i]);
	}
}
static void GpsGet2Data(volatile uint8* direct, char* src) {
	int i;
	float speed_f = 0;
	int speed_i = 0;
	char buf[20];

	i = GetComma(1, src);
	if (i < 2)
		return;
	strncpy(buf, src, i);
	speed_f = atof(buf);
	speed_i = (int) speed_f;
	direct[0] = speed_i / 256;
	direct[1] = speed_i % 256;

	return;
}

static void Get2Speed(volatile uint8* speed, char* src) {
	int i;
	float speed_f = 0;
	int speed_i = 0;
	char buf[20];

	i = GetComma(1, src);
	if (i < 2)
		return;
	strncpy(buf, src, i);
	speed_f = atof(buf) * 1.85 * 10;
	speed_i = (int) speed_f;
	speed[0] = speed_i / 256;
	speed[1] = speed_i % 256;
	return;
}


static void IntToDWord(volatile uint8* dst, int src) {
	dst[0] = src / (256 * 256 * 256);
	src = src % (256 * 256 * 256);
	dst[1] = src / (256 * 256);
	src = src % (256 * 256);
	dst[2] = src / (256);
	src = src % (256);
	dst[3] = src;
}

void ParseGpsDataToJTTGPS(stGpsInformat* pGpsInfo,GPSData* gpsData)
{
    if(pGpsInfo == NULL || gpsData == NULL){
        return;
    }
    pGpsInfo->gpsTime[0] = gpsData->gpsTime[0];
    pGpsInfo->gpsTime[1] = gpsData->gpsTime[1];
    pGpsInfo->gpsTime[2] = gpsData->gpsTime[2];
    pGpsInfo->gpsTime[3] = gpsData->gpsTime[3];
    pGpsInfo->gpsTime[4] = gpsData->gpsTime[4];
    pGpsInfo->gpsTime[5] = gpsData->gpsTime[5];
    for (int i = 0; i < 6; i++) {
		pGpsInfo->gpsTime[i] = DecToBCD(pGpsInfo->gpsTime[i]);
	}
	IntToDWord(pGpsInfo->bbLat,(int)(abs(gpsData->latlon.lat)*1000000));
	IntToDWord(pGpsInfo->bbLng,(int)(abs(gpsData->latlon.lon)*1000000));
    pGpsInfo->speed[0] = (int)(gpsData->speed*10) / 256;
    pGpsInfo->speed[1] = (int)(gpsData->speed*10) % 256;

    pGpsInfo->direct[0] = (int)gpsData->direct / 256;
    pGpsInfo->direct[1] = (int)gpsData->direct % 256;

    // 经度信息
	if (gpsData->latlon.lon < 0)
		pGpsInfo->statFlag[3] |= 0x08;
	// 纬度信息
	if (gpsData->latlon.lat < 0)
		pGpsInfo->statFlag[3] |= 0x04;

	pGpsInfo->statFlag[3] |= 0x03;
}

static void GPRMC_Parse(void) {
    static int gpsStat = 0;
	static int gpsOffCnt = 0;


    GPSData gpsData;
    memset(&gpsData, 0, sizeof(GPSData));
	// 定位不成功
	if (uGpsBuff[GetComma(2, (char*) uGpsBuff)] != 'A') 
	{

		printf("GPS Location unreliable !!!! = %s\n",uGpsBuff);
        gpsData.available = 0;
        pthread_mutex_lock(&pth_lock_gps);
        memcpy(&g_gpsData,&gpsData,sizeof(GPSData));
        pthread_mutex_unlock(&pth_lock_gps);
		return;
	}
    gpsData.available = 1;
    
    int tmPos = 0;
    gpsData.gpsTime[3] = (uGpsBuff[7] - '0') * 10 + (uGpsBuff[8] - '0');
	gpsData.gpsTime[4] = (uGpsBuff[9] - '0') * 10 + (uGpsBuff[10] - '0');
	gpsData.gpsTime[5] = (uGpsBuff[11] - '0') * 10 + (uGpsBuff[12] - '0');

    gpsData.gpsTime[6] = (uGpsBuff[14] - '0') * 10 + (uGpsBuff[15] - '0');
    gpsData.gpsTime[7] = 0;

    tmPos = GetComma(9, (char*) uGpsBuff);
	gpsData.gpsTime[2] = (uGpsBuff[tmPos + 0] - '0') * 10 + (uGpsBuff[tmPos + 1] - '0');
	gpsData.gpsTime[1] = (uGpsBuff[tmPos + 2] - '0') * 10 + (uGpsBuff[tmPos + 3] - '0');
	gpsData.gpsTime[0] = (uGpsBuff[tmPos + 4] - '0') * 10 + (uGpsBuff[tmPos + 5] - '0');

    // 系统时间转换
	UTC2BTC(gpsData.gpsTime);

    gpsData.speed = GetSpeed((char*) &uGpsBuff[GetComma(7, (char*) uGpsBuff)]);

    gpsData.direct = GetData((char*) &uGpsBuff[GetComma(8, (char*) uGpsBuff)]);

    gpsData.latlon.lat = GetTitude((char*) &uGpsBuff[GetComma(3, (char*) uGpsBuff)]);
    gpsData.latlon.lon = GetTitude((char*) &uGpsBuff[GetComma(5, (char*) uGpsBuff)]);
    if (uGpsBuff[GetComma(6, (char*) uGpsBuff)] != 'E')
		gpsData.latlon.lat = 0 - gpsData.latlon.lat;
    
	// 纬度信息
	if (uGpsBuff[GetComma(4, (char*) uGpsBuff)] != 'N')
		gpsData.latlon.lon = 0- gpsData.latlon.lon;

    //printf("20%d-%d-%d %02d:%d:%d.%d%d \n" , gpsData.gpsTime[0],gpsData.gpsTime[1],gpsData.gpsTime[2],gpsData.gpsTime[3],gpsData.gpsTime[4],gpsData.gpsTime[5],gpsData.gpsTime[6],gpsData.gpsTime[7]);
    //printf("%.1f %.1f %.8f %.8f\n",gpsData.speed,gpsData.direct,gpsData.latlon.lat,gpsData.latlon.lon);

#if 1
    ParseGpsDataToJTTGPS(pGpsInfo,&gpsData);
#endif


    pthread_mutex_lock(&pth_lock_gps);
    memcpy(&g_gpsData,&gpsData,sizeof(GPSData));
    pthread_mutex_unlock(&pth_lock_gps);
}

static void* GpsThread(void* arg) 
{
    pGpsInfo = &stGpsInfo;
#if 0
	memcpy(uGpsBuff,debugMsg,sizeof(debugMsg));
	while(!quit)
	{
        GPRMC_Parse();
		usleep(1000*1000);
	}
	
#endif

#if 1
    int gpsFd = -1;

	gpsFd = Seri_Initial(GPS_DEV, baudrate, 8, 1, 'N');

    uint8 tmp[1024];
	int nread = 0;
	while (!quit) 
	{
		read(gpsFd, &tmp[nread++], 1);
		if (tmp[nread - 1] == '\n') 
		{
            char * gprmc = strstr((char* ) tmp , "$GPRMC");
            
            if(gprmc != NULL){
                memset(uGpsBuff, 0, sizeof(uGpsBuff));
                strncpy((char*)uGpsBuff, gprmc, strlen(gprmc) + 1);
                GPRMC_Parse();
                if(gpsLog == 1){
                    printf("%s",uGpsBuff);
                }
            }

            if(gpsLog == 2){
                printf("%s",tmp);
            }
            if(g_gps_log_file){
                fwrite(tmp,1,strlen((char *)tmp),g_gps_log_file);
            }
            memset(tmp, 0, sizeof(tmp));
			nread = 0;
		}
    }
#endif
	return NULL;
}