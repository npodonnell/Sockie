//
// SOCKIE! Write Less, do more
//


#include "stdafx.h" // contains nothing; only needed for compilation

#ifdef WIN32

// This turns off the error message and enables compilation
// on visual studio which doesn't consider strcpy() safe.
#pragma warning (disable:4996)

// Kink to WinSock2 library
#pragma comment (lib, "Ws2_32.lib")


#endif


#include "sockie.h"


bool setBlocking(SOCKET sockfd,bool block)
{
#ifdef WIN32
	unsigned long mode=0;
	// Set socket to non-blocking
	if (!block) mode=1;
	if (ioctlsocket(sockfd,FIONBIO,&mode) != 0){
		return false;
	}
	return true;
#else
	int flags=fcntl(sockfd, F_GETFL, 0);
	if(flags == -1){
		return false;
	}
		
	/*Set the blocking flag. */
	if (block) {
		flags &= ~O_NONBLOCK;
	}else{
		flags |= O_NONBLOCK;
	}
	
	return (fcntl(sockfd, F_SETFL, flags) != -1);	
#endif
}


sockie* sockieConnect(const char* server,unsigned int port,unsigned int connectTimeout,unsigned int recieveTimeout,unsigned int sendTimeout)
{
	sockie* s;
	SOCKET sockfd;
	struct timeval tv;
	struct timeval* ptv;
	fd_set fdSet;
	int so_error;
	int so_error_len = sizeof(so_error);
	bool connected=false;
	struct addrinfo *adinf,*adnxt;
	struct addrinfo hints;
	int res;
	char portstr[50];

	memset(&hints,'\0',sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Print the port number as a string
	sprintf(portstr,"%u",port);

	// Resolve the server address and port
	res=getaddrinfo(server,portstr,&hints,&adinf);
	if (res!=0){
	    return (sockie*)0;
	}

	// set the next addr info to the original one.
	// adnxt will continually be updated to the ->ad_next
	// member until a NULL is encountered
	adnxt=adinf;

	do{
		struct sockaddr_in *ska= (sockaddr_in*)adnxt->ai_addr;
		unsigned char c1=*(((unsigned char*)&(ska->sin_addr))+0);
		unsigned char c2=*(((unsigned char*)&(ska->sin_addr))+1);
		unsigned char c3=*(((unsigned char*)&(ska->sin_addr))+2);
		unsigned char c4=*(((unsigned char*)&(ska->sin_addr))+3);
		printf("SERVER:%s PORT:%s Add:%u.%u.%u.%u\r\n",server,portstr,c1,c2,c3,c4);

		// Create a socket for connecting to server
		sockfd=socket(adnxt->ai_family,adnxt->ai_socktype,adnxt->ai_protocol);
		if (sockfd==INVALID_SOCKET){
			// Failed to create socket
			freeaddrinfo(adinf);
			return (sockie*)0;
		}

		// Set socket to non-blocking
		if (!setBlocking(sockfd,false)){
			return (sockie*)0;
		}


		// Connect
		connect(sockfd,adnxt->ai_addr,(int)(adnxt->ai_addrlen));

		// Set file descriptor set
		FD_ZERO(&fdSet);
		FD_SET(sockfd, &fdSet);

		if (connectTimeout>0){
			tv.tv_usec=0;
			tv.tv_sec=connectTimeout;
			ptv=&tv;
		}else{
			ptv=(struct timeval*)0;
		}

		if (select(sockfd+1,NULL,&fdSet,NULL,ptv) == 1){
			getsockopt(sockfd,SOL_SOCKET,SO_ERROR,(char*)&so_error,(socklen_t*)&so_error_len);
			if (so_error == 0){
				// Connected!
				connected=true;
				break;
			}
		}

		// Not connected. Close and invalidate the socket
		shutdown(sockfd,2);
		sockfd=INVALID_SOCKET;

		// Try the next one
		adnxt=adnxt->ai_next;

	}while(adnxt!=(struct addrinfo*)0);

	if (!connected){
		// Connection attempt failed.
		freeaddrinfo(adinf);
		return (sockie*)0;
	}

	// We're connected.
	freeaddrinfo(adinf);
	s=(sockie*)malloc(sizeof(sockie));
	s->server=(char*)malloc(strlen(server)+1);
	strcpy(s->server,server);
	s->port=port;
	s->recieveTimeout=recieveTimeout;
	s->sendTimeout=sendTimeout;
	s->sockfd=sockfd;

	return s;
}

void sockieDisconnect(sockie** s)
{
	if (*s==(sockie*)0){
		return;
	}

	if ((*s)->server != (char*)0){
		free((*s)->server);
	}

	// Shut down the socket
	shutdown((*s)->sockfd,2);

	// Free the structure
	free(*s);
	*s=(sockie*)0;
}

sockie* sockieAccept(unsigned int port,unsigned int acceptTimeout,unsigned int recieveTimeout,unsigned int sendTimeout)
{
	SOCKET sockfd,cs;
	struct timeval tv;
	struct timeval* ptv;
	fd_set fdSet;
	struct addrinfo *adinf=0;
	struct addrinfo hints;
	struct sockaddr_in peer;
	socklen_t peerSize;
	int res;
	char portstr[50];
	int optval;

	memset(&hints,'\0',sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Print the port number as a string
	sprintf(portstr,"%u",port);

	// Resolve the server address and port
	res=getaddrinfo(NULL,portstr,&hints,&adinf);
	if (res!=0){
	    return (sockie*)0;
	}

	// Create a SOCKET for connecting to server
	sockfd=socket(adinf->ai_family,adinf->ai_socktype,adinf->ai_protocol);
	if (sockfd==INVALID_SOCKET){
	    freeaddrinfo(adinf);
	    return (sockie*)0;
	}

	// Set socket to non-blocking
	if (!setBlocking(sockfd,false)){
		return (sockie*)0;
	}


	// set the SO_REUSEADDR flag
	optval = 1;
	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char*)&optval,sizeof(optval));

	// Setup the TCP listening socket
	res = bind(sockfd, adinf->ai_addr, (int)adinf->ai_addrlen);
	if (res == SOCKET_ERROR) {
	    freeaddrinfo(adinf);
#ifdef WIN32
	    closesocket(sockfd);
#else
	    shutdown(sockfd,SHUT_RDWR);
#endif
	    return (sockie*)0;
	}

	// Don't need this anymore
	freeaddrinfo(adinf);

	// Start listening
	res = listen(sockfd,1);
	if (res == SOCKET_ERROR) {
#ifdef WIN32
	    closesocket(sockfd);
#else
	    shutdown(sockfd,SHUT_RDWR);
#endif
	    return (sockie*)0;
	}

	// Set the peer size for the accept function
	peerSize=sizeof(struct sockaddr);

	// Begin the wait
	FD_ZERO(&fdSet);
	FD_SET(sockfd,&fdSet);

	if (acceptTimeout>0){
		tv.tv_usec=0;
		tv.tv_sec=acceptTimeout;
		ptv=&tv;
	}else{
		ptv=(struct timeval*)0;
	}

	if (select(sockfd+1,&fdSet,NULL,NULL,ptv) != 1){
#ifdef WIN32
		closesocket(sockfd);
#else
		shutdown(sockfd,SHUT_RDWR);
#endif
		return (sockie*)0;
	}

	// Accept a client socket
	cs = accept(sockfd,(struct sockaddr*)&peer,&peerSize);

	if (cs == INVALID_SOCKET) {
#ifdef WIN32
		closesocket(sockfd);
#else
		shutdown(sockfd,SHUT_RDWR);
#endif
		return (sockie*)0;
	}
	
	// Set the new socket to non-blocking
	if (!setBlocking(cs,false)){
		return (sockie*)0;
	}

	// Create the new Sockie
	sockie* ss=(sockie*)malloc(sizeof(sockie));
	ss->port=peer.sin_port;
	ss->recieveTimeout=recieveTimeout;
	ss->sendTimeout=sendTimeout;
	ss->sockfd=cs;
	
	// Allocate memory to hold the remote IP address (as a string)
	ss->server=(char*)malloc(IPADDRESS_MAXLENGTH+1);
	if (!ss->server){
		free(ss);
#ifdef WIN32
		closesocket(sockfd);
		closesocket(cs);
#else
		shutdown(sockfd,SHUT_RDWR);
		shutdown(cs,SHUT_RDWR);
#endif
		return (sockie*)0;
	}
	
	// Extract the IP and convert to a string using inet_ntop()
	if (inet_ntop(peer.sin_family,&(peer.sin_addr),ss->server,IPADDRESS_MAXLENGTH) == 0){
		free(ss->server);
		free(ss);
#ifdef WIN32
		closesocket(sockfd);
		closesocket(cs);
#else
		shutdown(sockfd,SHUT_RDWR);
		shutdown(cs,SHUT_RDWR);
#endif
		return (sockie*)0;
	}

	// close the sockfd socket as we don't need it anymore
#ifdef WIN32
	closesocket(sockfd);
#else
	shutdown(sockfd,SHUT_RDWR);
#endif

	return ss;
}

bool sockieSend(sockie* s,void* data,unsigned int dataLen)
{
	unsigned int bytes=0;
	int ret;
	struct timeval tv;
	struct timeval* ptv;
	fd_set writeFds;
	
	// set the timeout
	if (s->sendTimeout > 0){
		tv.tv_usec=0;
		tv.tv_sec=s->sendTimeout;
		ptv=&tv;
	}else{
		ptv=(struct timeval*)0;
	}

	FD_ZERO(&writeFds);
	FD_SET(s->sockfd,&writeFds);


	while(true){
		select(s->sockfd+1,NULL,&writeFds,NULL,ptv);

		if (!FD_ISSET(s->sockfd,&writeFds)){
			// timeout reached
			return false;
		}

		ret=send(s->sockfd,(const char*)data,dataLen-bytes,0);

		if (ret<1){
			return false;
		}

		bytes+=ret;

		if (bytes==dataLen){
			return true;
		}
	}

	return false;
}

bool sockieRecieve(sockie* s,void* data,unsigned int dataLen)
{
	unsigned int bytes=0;
	int ret;
	struct timeval tv;
	struct timeval* ptv;
	fd_set readFds;
	
	// set the timeout
	if (s->recieveTimeout > 0){
		tv.tv_usec=0;
		tv.tv_sec=s->recieveTimeout;
		ptv=&tv;
	}else{
		ptv=(struct timeval*)0;
	}

	FD_ZERO(&readFds);
	FD_SET(s->sockfd,&readFds);

	while(true){
		select(s->sockfd+1,&readFds,NULL,NULL,ptv);

		if (!FD_ISSET(s->sockfd,&readFds)){
			// timeout reached
			return false;
		}

		ret=recv(s->sockfd,(char*)data,dataLen-bytes,0);
		if (ret<1){
			return false;
		}

		bytes+=ret;

		if (bytes==dataLen){
			return true;
		}
	}

	return true;
}

int sockieSendUntil(sockie* s,void* data,void* pattern,unsigned int patternLen)
{
	unsigned int bytes=0;
	int ret;
	struct timeval tv;
	struct timeval* ptv;
	fd_set writeFds;
	int btg=-1; // Bytes To Go

	if (pattern==0 || patternLen==0){
		return -1;
	}
	
	// set the timeout
	if (s->sendTimeout > 0){
		tv.tv_usec=0;
		tv.tv_sec=s->sendTimeout;
		ptv=&tv;
	}else{
		ptv=(struct timeval*)0;
	}

	FD_ZERO(&writeFds);
	FD_SET(s->sockfd,&writeFds);

	while(true){
		if (btg<0 && strncmp((char*)data+bytes,(char*)pattern,patternLen)==0){
			btg=(int)patternLen;
		}

		select(s->sockfd+1,NULL,&writeFds,NULL,ptv);
		if (!FD_ISSET(s->sockfd,&writeFds)){
			// timeout reached
			return -1;
		}

		// Do the send (one byte)
		ret=send(s->sockfd,(const char*)data+bytes,1,0);
		if (ret!=1){
			return -1;
		}

		bytes+=ret;
		if (btg>0) btg-=ret;

		if (btg==0){
			return bytes;
		}
	}
	return -1;
}

int sockieRecieveUntil(sockie* s,void* data,void* pattern,unsigned int patternLen)
{
	unsigned int bytes=0;
	int ret;
	struct timeval tv;
	struct timeval* ptv;
	fd_set readFds;

	if (pattern==0 || patternLen==0){
		return -1;
	}
	
	// set the timeout
	if (s->recieveTimeout > 0){
		tv.tv_usec=0;
		tv.tv_sec=s->recieveTimeout;
		ptv=&tv;
	}else{
		ptv=(struct timeval*)0;
	}

	FD_ZERO(&readFds);
	FD_SET(s->sockfd,&readFds);

	while(true){
		select(s->sockfd+1,&readFds,NULL,NULL,ptv);
		if (!FD_ISSET(s->sockfd,&readFds)){
			// timeout reached
			return -1;
		}

		// Do the recv (one byte)
		ret=recv(s->sockfd,(char*)data+bytes,1,0);

		if (ret<1){
			return -1;
		}

		bytes+=ret;

		// return if we've matched the pattern
		if (bytes>=patternLen && strncmp(((char*)data+bytes)-patternLen,(char*)pattern,patternLen)==0){
			return bytes;
		}
	}

	return -1;
}




