#include "carplate_main.h"



static int speed_arr[] = { B115200, B38400, B19200, B9600, B4800 };
static int name_arr[] = { 115200, 38400, 19200, 9600, 4800 };


	

static int Seri_Initial(char* dev, int speed, int databits, int stopbits,int parity) 
{
		
	int fd = -1;
	//打开设备节点
	fd = OpenTtyDev(dev);

	//设置波特率
	SetTtySpeed(fd, speed);

	//设置传输属性
	SetTtyParity(fd, databits, stopbits, parity);

	return fd;
}

static int OpenTtyDev(const char *Dev) {
	int fd = open(Dev, O_RDWR);         //| O_NOCTTY | O_NDELAY
	if (-1 == fd) {
		return 0;
	}
	return fd;
}

static int SetTtySpeed(int fd, int speed) {
	int i;
	int status;
	struct termios Opt;
	tcgetattr(fd, &Opt);

	for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++) {
		if (speed == name_arr[i]) {
			tcflush(fd, TCIOFLUSH);

			cfsetispeed(&Opt, speed_arr[i]);
			cfsetospeed(&Opt, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &Opt);
			if (status != 0) {
				return 0;
			}
			tcflush(fd, TCIOFLUSH);
		}
	}
	return 1;
}

static int SetTtyParity(int fd, int databits, int stopbits, int parity) {
	struct termios options;
	if (tcgetattr(fd, &options) != 0) {
		return 0;
	}
	options.c_cflag &= ~CSIZE;

	// 数据位数
	switch (databits) {
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Unsupported data size\n");
		return 0;
	}

	//
	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB; /* Clear parity enable */
		options.c_iflag |= INPCK; /* Disnable parity checking */
		options.c_lflag = ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH);
		options.c_lflag &= ~(ICANON | ISIG);
		options.c_iflag &= ~(ICRNL | IGNCR);
		options.c_iflag &= ~(ICRNL | IXON);
		options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
		options.c_oflag &= ~(ONLCR | OCRNL | ONOCR | ONLRET);
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);
		options.c_iflag |= INPCK; /* Disnable parity checking */
		options.c_lflag = ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH);
		options.c_lflag &= ~(ICANON | ISIG);
		options.c_iflag &= ~(ICRNL | IGNCR);
		options.c_iflag &= ~(ICRNL | IXON);
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB; /* Enable parity */
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK; /* Disnable parity checking */
		break;
	case 'S':
	case 's': /*as no parity*/
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported parity\n");
		return 0;
	}

	//停止位
	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported stop bits\n");
		return 0;
	}

	if (parity != 'n') {
		options.c_iflag |= INPCK;
	}
	options.c_cc[VTIME] = 150; // 15 seconds
	options.c_cc[VMIN] = 0;

	tcflush(fd, TCIFLUSH); /* Update the options and do it NOW */
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		perror("SetupSerial 3");
		return 0;
	}
	return 1;
}
