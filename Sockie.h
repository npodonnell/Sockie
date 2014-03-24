#pragma once

#ifdef WIN32
//-----------------------------
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
//-----------------------------
#else
//-----------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
//-----------------------------
#endif


#define IPADDRESS_MAXLENGTH 80


#ifndef WIN32
// POSIX does not define SOCKET
typedef int SOCKET;

// These are windows defines
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#endif

typedef struct sockie
{
	char* server;
	unsigned int port;
	SOCKET sockfd;
	unsigned int recieveTimeout;
	unsigned int sendTimeout;
}sockie;


sockie* sockieConnect(const char* server,unsigned int port,unsigned int connectTimeout,unsigned int recieveTimeout,unsigned int sendTimeout);
void sockieDisconnect(sockie** s);
sockie* sockieAccept(unsigned int port,unsigned int acceptTimeout,unsigned int recieveTimeout,unsigned int sendTimeout);
bool sockieSend(sockie* s,void* data,unsigned int dataLen);
bool sockieRecieve(sockie* s,void* data,unsigned int dataLen);
int sockieSendUntil(sockie* s,void* data,void* pattern,unsigned int patternLen);
int sockieRecieveUntil(sockie* s,void* data,void* pattern,unsigned int patternLen);

