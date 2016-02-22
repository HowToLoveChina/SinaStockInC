/*
	@author tomac
	@email tomac_2g@sina.com
	@qq    5379823
	@desc       新浪实时行情 参数  股票代码  等待时间
	@usage       ./sina  "sh600000,sh600770" 1 
	@return     返回数据含义见最后
	@compile    gcc sinadata.c -o sinadata
	@support    yum install gcc  /  apt-get install gcc 
	@example   
		        fp = popen("./sinadata  \"sh600000,sh600770\"  1 ","r");
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <strings.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

int ConnectServer( char * ServerName , int ServerPort );
int parse_sina_network(char * req , int sock);
int parse_sina_data   (char * req , int sock );

const char * header = "HTTP/1.1\r\n"
						"Accept: text/html, application/xhtml+xml, */*\r\n"
						"Accept-Language: zh-CN\r\n"
						"User-Agent: Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)\r\n"
						"Accept-Encoding: text\r\n"
						"Host: hq.sinajs.cn\r\n"
						"Connection: KeepAlive\r\n\r\n";

//						"Accept-Encoding: gzip, deflate\r\n"


int main(int argc ,char ** argv ){
  	char request[409600];
  	memset(request,0,sizeof(request));
 	int sock = -1 ;
 	int slp = atoi(argv[2]) ;
  	while(1){
 	 	sprintf(request,"GET /list=%s %s",argv[1],header);
  		if( sock == -1 ){
  			sock = ConnectServer( (char*)"hq.sinajs.cn",80);
			int x;
			x=fcntl(sock,F_GETFL,0);
			fcntl(sock,F_SETFL,x | O_NONBLOCK);
  		}
		if( parse_sina_data(request,sock) < 0 ){
			break;
		}
  		memset(request,0,sizeof(request));
  		if( slp>0 ){
	  		sleep(slp);
	  	}
  	}
  	return 0;
}

int wait_data(int mSock , int uSec){
	struct timeval tv={0,0};
	fd_set tst_fds;
	int n;
	if( mSock < 0 ) return -1;
	if( uSec < 0 ) uSec = - uSec;
	if( uSec > 0 ){
		tv.tv_sec=uSec/1000000;
		tv.tv_usec=uSec%1000000;
	}
	FD_ZERO(&tst_fds);
	FD_SET(mSock,&tst_fds);
	n = select ( mSock+1 ,&tst_fds,NULL,NULL,&tv);
	return FD_ISSET(mSock,&tst_fds)?0:-1;
}


int parse_sina_data(char * req , int sock ){
	if( 0 != parse_sina_network(req,sock) ){
		return - __LINE__;
	}
	
	char * s ;
	char * f[80];
	while( req != NULL){
		s = strstr( req , "\n");
		if( s != NULL ){
			*s = 0 ;
		}else{
			break;
		}
		char * stock = strstr(req,"=");
		if( stock == NULL ){
			req = (s+1);
			continue;
		}
		stock = stock - 6;
		stock[6]=0; 
		req = stock + 7;
		
		//printf("stock:%s\n",stock);
		//var hq_str_sz000001="平安银行,10.03,10.01,10.15,10.22,9.99,10.14,10.15,58516706,590538918.78,19301,10.14,87680,10.13,55100,10.12,117400,10.11,102700,10.10,35905,10.15,369800,10.16,477800,10.17,670639,10.18,421349,10.19,2016-02-17,15:05:57,00";
		char * beg = strstr(req , "\"");
		if( beg != NULL ){
			beg ++ ;
			char * end = strstr(beg,"\"");
			if( end != NULL ){
				*end = ',';
				end++;
				*end=0;
				printf("%s,",stock);
				puts(beg);
			}
		}
		//
		req = (s+1);
	}
}	

int parse_sina_network(char * req,int sock){
	int size = strlen(req);
	int r;
	char *s = req;
	char *p;

	while( size > 0 ){
		r = send(sock,req,((size>1024)?1024:size),0);
		if( r <= 0 ){
			printf("send error");
			break;
		}
		req += r ;
		size -=r ;
	}
	if ( wait_data(sock,800000) != 0 ){
		puts("timeout");
		return - __LINE__ ;
	}
	r = recv(sock,s,1024,0);
	if( r <= 0 ){
		printf("%d:recv fail",__LINE__);
		return - __LINE__ ;
	}
	s[r]=0;
	p =strstr(s,"Content-Length: ");
	if( p == NULL ){
		printf("%d:no length",__LINE__);
		return - __LINE__ ;
	}
	p+=16;
	size = atoi(p);
	if( size <= 0 ){
		printf("%d:error size",__LINE__);
		return - __LINE__ ;
	}
	p = strstr(s,"\r\n\r\n");
	if( p == NULL ){
		printf("%d: no double br",__LINE__);
		return - __LINE__ ;
	}
	p+=4;
	int hsize = (p-s);
	//头长  p-s
	memmove(s,p,r-hsize);
	size -= (r-hsize);
	p=&s[r-hsize];
	s=p;
	while(size>0){
		if ( wait_data(sock,8000000) != 0 ){
			printf("timeout size=%d\n",size);
			break;
		}
		r = recv(sock,s,(size>1024)?1024:size,0);
		if( r<= 0 )break;
		size-=r;
		s[r]=0;
		s+=r;
	}
	return 0;
}

int ConnectServer( char * ServerName , int ServerPort ){
	struct sockaddr_in sa;
	struct hostent * hp;
	int opt;
	int s;
	if(ServerName== NULL ){
		return - __LINE__;
	}
	if(ServerPort<=0){
		ServerPort=80;
	}
	hp=(struct hostent*)gethostbyname(ServerName);
	if(hp==NULL){
		return - __LINE__;
	}
	memcpy((char*)&sa.sin_addr,(char*)hp->h_addr,hp->h_length);
	sa.sin_family=hp->h_addrtype;
	sa.sin_port=htons(ServerPort);
	if((s=socket(hp->h_addrtype,SOCK_STREAM,0))<0){
		return -1;
	}
	if( connect(s,(struct sockaddr *) &sa,sizeof(sa)) != 0 ){
		close(s);
		return - __LINE__ ;
	}
	return s;
}

/*
stock char(6),
open float,   开
close float,  昨收
`now` float,  现
high float,   高
low float,    低
buy float,    现买
sell float,   现卖
volume float, 成交量
amount float, 成交额
b1v float,    买一量
b1 float,     买一价
b2v float,
b2 float,
b3v float,
b3 float,
b4v float,
b4 float,
b5v float,
b5 float,
s1v float,   卖一量
s1 float,    卖一价
s2v float,
s2 float,
s3v float,
s3 float,
s4v float,
s4 float,
s5v float,
s5 float,
date timestamp,  时间
*/