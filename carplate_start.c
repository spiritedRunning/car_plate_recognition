#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>



static char appName[50] = {"/root/app/carplate_main"};

static int pidGrp[20] = { 0 };


int main(int argc, char** argv) 
{
#if 1
	system("rm /data/core*");
	system("chmod +x /root/app/carplate_main");    
#endif

	printf("\n\n Carplate Start.........\n");
	
	// 启动进程
	char *path[]={"carplate_main",NULL};
	pid_t pid, rePid;
	pid = getpid();

	int index = 0;
	// 启动所有的进程
	pidGrp[index] = fork();
	if (pidGrp[index] == 0) {
		printf("%s\n", appName);
		execv(appName, path);
		
	}
	if(index == 0)
	{
	   sleep(3);
	}

	int reStartFlag = 0;
	while(1)
	{
		// 等待异常结束的进程
		rePid = wait(NULL);

		if (rePid == pidGrp[index]){
			reStartFlag = 0x01;
		}

//		printf("rePid=%d\n", rePid);

		if (reStartFlag == 0x01) {
			pidGrp[index] = fork();
			if (pidGrp[index] == 0) {
				printf("%s\n", appName);

				execv(appName, path);
				sleep(10);
			}

			reStartFlag = 0;
		}
		sleep(1);
	}
	return 0;
}


