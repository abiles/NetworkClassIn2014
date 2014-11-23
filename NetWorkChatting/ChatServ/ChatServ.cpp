// NetWork.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <WinSock2.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")
#define BUF_SIZE 100
#define READ 3
#define WRITE 5
#define QUEUESIZE 5
#define MAXCLNTSOCKET 10
//#define TEST

//����� Ŭ���̾�Ʈ ���Ͽ� ���� ������ ������ �ִ�.
typedef struct
{
	SOCKET		hClntSock;
	SOCKADDR_IN clntAdr;

} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

//Overlapped IO�� ���� OVERLAPPED ����ü�� ���۵��� ������ �ִ�.
typedef struct
{
	OVERLAPPED overlapped;
	WSABUF	   wsaBuf;
	char	   buffer[BUF_SIZE];
	int		   rwMode;
} PER_IO_DATA, *LPPER_IO_DATA;

CRITICAL_SECTION globalCriticalSection;
SOCKET clntSocketes[MAXCLNTSOCKET] = { 0, };
int clntSocketsNum = 0;


unsigned int WINAPI ThreadMain(LPVOID pComport);
void		 ErrorHandling(char* message);



int main(int argc, char* argv[])
{
	WSADATA wsaData;
	HANDLE  hComport;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;
	unsigned short port;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	DWORD recvBytes, flags = 0;
	InitializeCriticalSection(&globalCriticalSection);
	int i = 0;

	if (argc < 2)
	{
#ifdef TEST
		printf("Usage : %s <port> \n", argv[0]);
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
	{
		ErrorHandling("WSAStartup() error!");
	}

	//CP������Ʈ�� �����ϴ� �Լ�
	hComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//�������� �ý��� ������ ���
	GetSystemInfo(&sysInfo);

	//sysInfo�� ����� CPU���� ��ŭ �����带 �����Ѵ�
	//������ ������ CP������Ʈ�� �ڵ��� �����Ѵ�
	//������� �� �ڵ��� ������� CP������Ʈ�� �����Ѵ�
	for (i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
		_beginthreadex(NULL, 0, ThreadMain, (LPVOID)hComport, 0, NULL);

	//Overlapped IO�� ������ ���� ������ �����Ѵ�
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	//������ �����ϴ� ��ǻ���� IP�ּҰ� �ڵ����� �Ҵ�
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(port);

	if (bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("bind() error!");
	if (listen(hServSock, QUEUESIZE) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	while (true)
	{
		SOCKET hClntSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);

		//���� ��û�� Ŭ���̾�Ʈ�� �ּҿ� �ּ��� ���̸� ��ƿ´�
		//������ ������ ������ �ڵ� ��ȯ
		printf("accept possilbe\n");
		//�׷� ������� �갡 ������ �� �� �����ϱ� �긦 ������ �� �־�ߵ� 
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &addrLen);

		//accept�޾Ƶ��ΰ� ���� ����
		if (hClntSock != INVALID_SOCKET)
		{
			EnterCriticalSection(&globalCriticalSection);
			if (clntSocketsNum < MAXCLNTSOCKET)
			{
				for (i = 0; i < MAXCLNTSOCKET; ++i)
				{
					if (clntSocketes[i] == 0)
					{
						clntSocketes[i] = hClntSock;
						clntSocketsNum++;
						break;
					}
				}

			}
			else
				printf("TooMuch Client, Please wait\n");
			LeaveCriticalSection(&globalCriticalSection);

			handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));

			//������ Ŭ���̾�Ʈ ������ �ڵ��� handleInfo�� �ִ´�
			handleInfo->hClntSock = hClntSock;

			//���� ��û�� Ŭ���̾�Ʈ�� �ּҰ��� handleInfo�� �ִ´�
			memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

			//������ Ŭ���̾�Ʈ ������ ������ ������ CP������Ʈ�� ���� ��Ų��
			CreateIoCompletionPort((HANDLE)hClntSock, hComport, (DWORD)handleInfo, 0);

			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;

			//SOKET_ERROR��� ���� ���� ������ ���� ���� ���� ����
			if (WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1,
					&recvBytes, &flags, &(ioInfo->overlapped), NULL) == SOCKET_ERROR)
			{
				//������ ������ �Ϸ���� �ʾ����� �������� ����
				if (WSAGetLastError() == WSA_IO_PENDING)
				{
					WSAWaitForMultipleEvents(1, &(ioInfo->overlapped).hEvent, TRUE, WSA_INFINITE, FALSE);
					WSAGetOverlappedResult(handleInfo->hClntSock, &(ioInfo->overlapped), &recvBytes,
										   FALSE, NULL);
				}
				else
				{
					ErrorHandling("WSARecv() error!");
				}
			}
		}

	}

	closesocket(hServSock);
	WSACleanup();

	return 0;
}

unsigned int WINAPI ThreadMain(LPVOID pComport)
{
	HANDLE hComport = (HANDLE)pComport;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;
	DWORD flags = 0;
	int i = 0;

	while (true)
	{
		//GetQueued�Լ��� ȣ���ϴ� �����带 CP������Ʈ�� �Ҵ�� �������� �Ѵ�
		//IO�� �Ϸ�ǰ� �̿� ���� ������ ��ϵǾ��� �� ��ȯ �Ѵ�. ������ �������ڰ� INFINITE�̱� ������
		//�̷��� ��ȯ�� �� ����° �������ڸ� ���ؼ� Ŭ���̾�Ʈ�� ����� ���� ���� �ڵ�, Ŭ���̾�Ʈ �ּҸ�
		//�׹�° �������ڸ� ���� ���������� flag�������� �˼� �ְ� �ȴ�
		GetQueuedCompletionStatus(hComport, &bytesTrans,
								  (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		//���� ������ sock�� �Ҵ��Ѵ�
		//���⼭ ������ �Ǵ±���.
		sock = handleInfo->hClntSock;

		if (ioInfo->rwMode == READ)
		{
			puts("message received!");
			//EOF ���۽�
			if (bytesTrans == 0)
			{
				for (i = 0; i < MAXCLNTSOCKET; ++i)
				{
					if (clntSocketes[i] == sock)
					{
						clntSocketes[i] = 0;
						--clntSocketsNum;
						break;
					}
				}
				closesocket(sock);
				free(handleInfo);
				free(ioInfo);
				continue;
			}

			//���� �����ϱ� �޾ƿ°� �״�� ������ �����ְ�
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = bytesTrans;
			ioInfo->rwMode = WRITE;
			EnterCriticalSection(&globalCriticalSection);
			for (i = 0; i < MAXCLNTSOCKET; ++i)
			{
				if (clntSocketes[i] != 0 && clntSocketes[i] != sock)
				{

					if (WSASend(clntSocketes[i], &(ioInfo->wsaBuf), 1, NULL, 0,
							&(ioInfo->overlapped), NULL) == SOCKET_ERROR)
					{
						if (WSAGetLastError() == WSA_IO_PENDING)
						{
							WSAWaitForMultipleEvents(1, &(ioInfo->overlapped).hEvent, TRUE, WSA_INFINITE, FALSE);
							WSAGetOverlappedResult(handleInfo->hClntSock, &(ioInfo->overlapped), NULL,
												   FALSE, NULL);
						}
						else
						{
							ErrorHandling("WSARecv() error!");
						}
					}
				}
			}
			LeaveCriticalSection(&globalCriticalSection);

			//�ٽ� �д� ���� �ٲ۴�. 
			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL,
					&flags, &(ioInfo->overlapped), NULL);


		}
		else
		{
			puts("message sent!");
			free(ioInfo);
		}
	}
}

void ErrorHandling(char* message)
{
	sprintf(message, " error : %d", GetLastError());
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}