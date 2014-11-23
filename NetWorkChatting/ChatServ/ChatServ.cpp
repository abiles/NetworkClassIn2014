// NetWork.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
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

//연결된 클라이언트 소켓에 대한 정보를 가지고 있다.
typedef struct
{
	SOCKET		hClntSock;
	SOCKADDR_IN clntAdr;

} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

//Overlapped IO를 위한 OVERLAPPED 구조체와 버퍼등을 가지고 있다.
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

	//CP오브젝트를 생성하는 함수
	hComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//실행중인 시스템 정보를 얻기
	GetSystemInfo(&sysInfo);

	//sysInfo에 저장된 CPU수를 만큼 쓰레드를 생성한다
	//쓰레드 생성시 CP오브젝트의 핸들을 전달한다
	//쓰레드는 이 핸들을 대상으로 CP오브젝트에 접근한다
	for (i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
		_beginthreadex(NULL, 0, ThreadMain, (LPVOID)hComport, 0, NULL);

	//Overlapped IO에 적합한 서버 소켓을 생성한다
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	//소켓이 동작하는 컴퓨터의 IP주소가 자동으로 할당
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

		//연결 요청한 클라이언트의 주소와 주소의 길이를 담아온다
		//성공시 생성된 소켓의 핸들 반환
		printf("accept possilbe\n");
		//그럼 만들어진 얘가 여러개 일 수 있으니까 얘를 저장할 수 있어야돼 
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &addrLen);

		//accept받아들인게 있을 때만
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

			//생성된 클라이언트 소켓의 핸들을 handleInfo에 넣는다
			handleInfo->hClntSock = hClntSock;

			//연결 요청한 클라이언트의 주소값을 handleInfo에 넣는다
			memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

			//생성된 클라이언트 소켓을 위에서 생성한 CP오브젝트와 연결 시킨다
			CreateIoCompletionPort((HANDLE)hClntSock, hComport, (DWORD)handleInfo, 0);

			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;

			//SOKET_ERROR라면 아직 보낸 파일을 전부 받지 못한 상태
			if (WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1,
					&recvBytes, &flags, &(ioInfo->overlapped), NULL) == SOCKET_ERROR)
			{
				//데이터 전송이 완료되지 않았지만 진행중인 상태
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
		//GetQueued함수를 호출하는 쓰레드를 CP오브젝트에 할당된 쓰레드라고 한다
		//IO가 완료되고 이에 대한 정보가 등록되었을 때 반환 한다. 마지막 전달인자가 INFINITE이기 때문에
		//이렇게 반환할 때 세번째 전달인자를 통해서 클라이언트와 연결된 내부 소켓 핸들, 클라이언트 주소를
		//네번째 전달인자를 통해 버퍼정보와 flag정보등을 알수 있게 된다
		GetQueuedCompletionStatus(hComport, &bytesTrans,
								  (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		//얻어온 정보를 sock에 할당한다
		//여기서 구분이 되는구나.
		sock = handleInfo->hClntSock;

		if (ioInfo->rwMode == READ)
		{
			puts("message received!");
			//EOF 전송시
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

			//에코 서버니까 받아온거 그대로 돌려서 보내주고
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

			//다시 읽는 모드로 바꾼다. 
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