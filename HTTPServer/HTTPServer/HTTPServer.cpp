#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WinSock2.h>
#include <process.h>

#pragma comment (lib, "Ws2_32.lib")

#define BUF_SIZE 2048
#define BUF_SMALL 100
#define ERROR_MSG_SIZE 200
//#define TEST

enum ErrorCode
{
	ERROR_NONE,
	ERROR_400_BAD_REQUEST,
	ERROR_404_NOT_FOUND,
	ERROR_END,

};


unsigned WINAPI RequestHandler(void* arg);
char* ContentType(char* file);
void SendData(SOCKET sock, char* ct, char* fileName);
void SendErrorMSG(SOCKET sock, ErrorCode error);
void ErrorHandling(char* message);



int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;

	HANDLE hThread;
	DWORD dwThreadID;
	int clntAdrSize;
	int port;

	if (argc != 2)
	{
#ifdef TEST
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
#else
		port = 14000;
#endif
	}
	else
	{
		port = atoi(argv[1]);
		
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error");

	hServSock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(port);

	if (bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	while (true)
	{
		clntAdrSize = sizeof(clntAdr);
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &clntAdrSize);
		printf("Connection Request: %s:%d\n", inet_ntoa(clntAdr.sin_addr), ntohs(clntAdr.sin_port));
		hThread = (HANDLE)_beginthreadex(NULL, 0, RequestHandler, (void*)hClntSock, 0, (unsigned *)&dwThreadID);
	}

	closesocket(hServSock);
	WSACleanup();
	return 0;
}

unsigned WINAPI RequestHandler(void* arg)
{
	SOCKET hClntSock = (SOCKET)arg;
	char buf[BUF_SIZE];
	char method[BUF_SMALL];
	char ct[BUF_SMALL];
	char fileName[BUF_SMALL];

	recv(hClntSock, buf, BUF_SIZE, 0);
	if (strstr(buf, "HTTP/") == NULL)
	{
		SendErrorMSG(hClntSock, ERROR_400_BAD_REQUEST);
		closesocket(hClntSock);
		return 1;
	}

	strcpy(method, strtok(buf, " /"));
	
	if (strcmp(method, "GET"))
		SendErrorMSG(hClntSock, ERROR_400_BAD_REQUEST);

	strcpy(fileName, strtok(NULL, " /"));
	strcpy(ct, ContentType(fileName));
	SendData(hClntSock, ct, fileName);

	return 0;
}

void SendData(SOCKET sock, char* ct, char* fileName)
{
	char protocol[] = "HTTP/1.1 200 OK\r\n";
	char servName[] = "Server:SY webserver\r\n";
	char cntLen[] = "Content-length:2048\r/n";
	char cntType[BUF_SMALL];
	char buf[BUF_SIZE];
	FILE* sendFile;

	sprintf(cntType, "Content-type:%s\r\n\r\n", ct);
	if ((sendFile = fopen(fileName, "rb")) == NULL)
	{
		SendErrorMSG(sock, ERROR_404_NOT_FOUND);
		return;
	}

	send(sock, protocol, strlen(protocol), 0);
	send(sock, servName, strlen(servName), 0);
	send(sock, cntLen, strlen(cntLen), 0);
	send(sock, cntType, strlen(cntType), 0);

	while (fgets(buf, BUF_SIZE, sendFile) != NULL)
		send(sock, buf, strlen(buf), 0);

	closesocket(sock);
}

void SendErrorMSG(SOCKET sock, ErrorCode error)
{
	char protocol[ERROR_MSG_SIZE] = { 0, };
	char* servName = nullptr;
	char* cntLen = nullptr;
	char* cntType = nullptr;
	char content[ERROR_MSG_SIZE] = { 0, };
	char* errorMSG = nullptr;

	switch (error)
	{
	case ERROR_400_BAD_REQUEST:
		errorMSG = "ERROR_400_BAD_REQUEST";
		break;
	case ERROR_404_NOT_FOUND:
		errorMSG = "ERROR_404_NOT_FOUND";
		break;
	default:
		errorMSG = "UNDEFINED_ERROR";
		break;
	}

	sprintf(protocol, "HTTP/1.1 %s\r\n", errorMSG);
	servName = "Server:SY webserver\r\n";
	cntLen = "Content-length:2048\r/n";
	cntType = "Content-type:text/html\r\n\r\n";
	sprintf(content, "<html><head><title>NETWORK</title></head>"
		"<body><font size =+5><br>%s"
		"</font></body></html>", errorMSG);

	send(sock, protocol, strlen(protocol), 0);
	send(sock, servName, strlen(servName), 0);
	send(sock, cntLen, strlen(cntLen), 0);
	send(sock, cntType, strlen(cntType), 0);
	send(sock, content, strlen(content), 0);

	closesocket(sock);
}

char* ContentType(char* file)
{
	char extension[BUF_SMALL];
	char fileName[BUF_SMALL];
	strcpy(fileName, file);
	strtok(fileName, ".");
	strcpy(extension, strtok(NULL, "."));
	if (!strcmp(extension, "html") || !strcmp(extension, "htm"))
		return "text/html";
	else
		return "text/plain";
}

void ErrorHandling(char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

