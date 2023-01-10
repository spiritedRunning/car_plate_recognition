#include "carplate_main.h"





#define CHUNK 16384

int inflate_read(char *source,int len,char **dest,int gzip)

{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char out[CHUNK];

	int totalsize = 0;

	if((*source != 0x1f)||(*(source+1) != 0x8b))
	{
		printf("uncompressGzip non Gzip\n");
	 	return -1;
	}


	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	if(gzip)
		ret = inflateInit2(&strm, 47);
	else
		ret = inflateInit(&strm);

	if (ret != Z_OK)
		return ret;

	strm.avail_in = len;
	strm.next_in = source;
		/* run inflate() on input until output buffer not full */

	do {
		strm.avail_out = CHUNK;
		strm.next_out = out;
		ret = inflate(&strm, Z_NO_FLUSH);
		
		assert(ret != Z_STREAM_ERROR); 	/* state not clobbered */
		
		switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR; 	/* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&strm);
				return ret;
		}
		have = CHUNK - strm.avail_out;
		totalsize += have;
		*dest = realloc(*dest,totalsize);
		memcpy(*dest + totalsize - have,out,have);
	} while (strm.avail_out == 0);
	printf("==============================>>>the totalsize is %d\n",totalsize);
		/* clean up and return */
	(void)inflateEnd(&strm);	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;

}

int get_https_data(char *ip,char *name,int portnumber,char *get_str,char *rsp_str,int rsp_buf_len)
{
	int sockfd=0;
	int ret;
	char buffer[1024*500];
	int  nbytes;
	char host_addr[256];
	char request[1024];
	int send, totalsend;
	int i;
	SSL *ssl;
	SSL_CTX *ctx;
	char server_ip[20]={0};
	struct hostent *host;
	struct in_addr addr;
    struct sockaddr_in servaddr;


	
	if(ip==NULL||strlen(ip)<7)
	{
		if((host=gethostbyname(name)) == NULL)
		{
			printf("gethostbyname fail..............\n");
			return -1;
		}
		memcpy(&addr.s_addr,(unsigned long*)host->h_addr_list[0],sizeof(addr.s_addr));
		strcpy(server_ip,(char *)inet_ntoa(addr));
		printf("gethostbyname %s ==> %s ..............\n",name,server_ip);
	}
	else
	{
		strcpy(server_ip,ip);
	}
	/* 客户程序开始建立 sockfd描述符 */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{ 	   /*建立SOCKET连接 */
		printf("socket fail..............\n");
		return -1;
	}

	/* 客户程序填充服务端的资料 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portnumber);
    if(inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0)
    {
        printf("inet_pton error for %s\n",server_ip);
        close(sockfd);
		return -1;
    }
	/* 客户程序发起连接请求 */
	if (connect(sockfd, (struct sockaddr *) (&servaddr), sizeof(struct sockaddr)) == -1)
	{		 
		printf( "Connect Error:%s\a\n", strerror(errno));
		close(sockfd);
		return -1;
	}
	/* SSL初始化 */
	SSL_library_init();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv23_client_method());
	if (ctx == NULL)
	{
		close(sockfd);
		return -1;
	}
	ssl = SSL_new(ctx);
	if (ssl == NULL)
	{
		close(sockfd);
		return -1;
	}
	/* 把socket和SSL关联 */
	ret = SSL_set_fd(ssl, sockfd);
	if (ret == 0)
	{
		close(sockfd);
		return -1;
	}

	RAND_poll();
	while (RAND_status() == 0)
	{
		unsigned short rand_ret = rand() % 65536;
		RAND_seed(&rand_ret, sizeof(rand_ret));
	}
	ret = SSL_connect(ssl);
	if (ret != 1)
	{
		close(sockfd);
		return -1;
	}
#if 0
	sprintf(request, "GET /%s HTTP/1.1\r\nAccept: */*\r\nAccept-Language: zh-cn\r\n\
User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n\
Host: %s:%d\r\nConnection: Close\r\n\r\n", get_str, name,portnumber);
#endif

#if 1
		sprintf(request, "GET /%s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:64.0) Gecko/20100101 Firefox/64.0\r\n"
		"Accept: text/html;charset=utf-8,application/xhtml+xml,application/xml;charset=utf-8;q=0.9,*/*;q=0.8\r\n"
		"Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2\r\n"
		"Accept-Encoding: gzip, deflate\r\n"
		"Connection: keep-alive\r\n"
		"Upgrade-Insecure-Requests: 1\r\n"
		"Cache-Control: max-age=0\r\n\r\n",
		get_str,name,portnumber);	
#endif	


	//printf( "%s", request);		  /*准备request，将要发送给主机 */
    //printf("request======>:\n[%s]\n",get_str);

	/*发送https请求request */
	send = 0;
	totalsend = 0;
	nbytes = strlen(request);
	while (totalsend < nbytes) 
	{
		send = SSL_write(ssl, request + totalsend, nbytes - totalsend);
		if (send == -1) 
		{
			close(sockfd);
			return -1;
		}
		totalsend += send;
		printf("%d bytes send OK!\n", totalsend);
	}

	//printf("\nThe following is the response header:\n");
	i = 0;
	/* 连接成功了，接收https响应，response */
	while((nbytes = SSL_read(ssl, buffer, 1)) == 1) 
	{
		if(i < 4) 
		{
			if (buffer[0] == '\r' || buffer[0] == '\n')
			{
				i++;
				if(i>=4)
				{
					break;
				}
			}
			else
			{
				i = 0;
			}	
			//printf("the >>>>>>>>>>>>>>>buffer is %c", buffer[0]);		/*把https头信息打印在屏幕上 */
		} 
	}
	memset(rsp_str,0,rsp_buf_len);
	ret = SSL_read(ssl, rsp_str, rsp_buf_len);
	if(ret<0)
	{
		printf("response ret =%d=============\n",ret);
		close(sockfd);
		return -1;
	}
	printf("rsp_buf_len = %d response ret =%d=============>[rsp_str[0]=%x,rsp_str[1]=%x]\n",rsp_buf_len,ret,rsp_str[0],rsp_str[1]); 

	/* 结束通讯 */
	SSL_shutdown(ssl);
	close(sockfd);
	SSL_free(ssl);
	SSL_CTX_free(ctx);
	ERR_free_strings();

	return ret;
}


unsigned char *MD5_file (unsigned char *path, int md5_len)
{
 	FILE *fp = fopen (path, "rb");
 	MD5_CTX mdContext;
 	int bytes;
 	unsigned char data[1024];
 	unsigned char *file_md5;
 	int i;
 	if (fp == NULL) 
	{
  		fprintf (stderr, "fopen %s failed\n", path);
  		return NULL;
 	}
 	MD5Init (&mdContext);
 	while ((bytes = fread (data, 1, 1024, fp)) != 0)
	 {
  		MD5Update (&mdContext, data, bytes);
 	}
 	MD5Final (&mdContext);

 	file_md5 = (char *)malloc((md5_len + 1) * sizeof(char));
 	if(file_md5 == NULL)
 	{
  		fprintf(stderr, "malloc failed.\n");
  		return NULL;
 	}
	
 	memset(file_md5, 0, (md5_len + 1));
 	if(md5_len == 16)
 	{
  		for(i=4; i<12; i++)
 		 {
  			sprintf(&file_md5[(i-4)*2], "%02x", mdContext.digest[i]);
 		 }
 	}
 	else if(md5_len == 32)
 	{
  		for(i=0; i<16; i++)
 		 {
   			sprintf(&file_md5[i*2], "%02x", mdContext.digest[i]);
 		 }
 	}
 	else
 	{
 		 fclose(fp);
  		free(file_md5);
  		return NULL;
 	}

 	fclose (fp);
 	return file_md5;
}


