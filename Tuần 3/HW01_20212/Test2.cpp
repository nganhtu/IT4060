#include "stdio.h"
#include "conio.h"
#include "string.h"
#include "ws2tcpip.h"
#include "winsock2.h"
#include "process.h"
#define BUFF_SIZE 1024
#define SERVER_PORT 5500
#define SERVER_ADDR "127.0.0.1"

#pragma comment(lib, "Ws2_32.lib")

int main()
{
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData))
		printf("Version is not supported \n");

	char buff[BUFF_SIZE];
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &(serverAddr.sin_addr.s_addr));

	SOCKET connSock;
	connSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(connSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		printf("\nError: %d", WSAGetLastError());
		closesocket(connSock);
		return 0;
	}

	char sBuff[BUFF_SIZE], rBuff[BUFF_SIZE];
	int ret;

	sBuff[0] = 0;
	sBuff[1] = 0;
	sBuff[2] = 0;
	sBuff[3] = 3;
	sBuff[4] ='1';
	sBuff[5] = '2';
	sBuff[6] = '3';

	sBuff[7] = 0;
	sBuff[8] = 0;
	sBuff[9] = 0;
	sBuff[10] = 3;
	sBuff[11] = 'a';
	sBuff[12] = '@';
	sBuff[13] = 'c';

	sBuff[14] = 0;
	sBuff[15] = 0;
	sBuff[16] = 0;
	sBuff[17] = 2;
	sBuff[18] = '5';
	sBuff[19] = '6';
	
	ret = send(connSock, sBuff, 20, 0); Sleep(100);
	ret = recv(connSock, rBuff, BUFF_SIZE, 0);
	if (ret < 0)
		printf("Stream test fail!\n");
	else {
		rBuff[ret] = 0;
		printf("Main: %s-->%s\n", sBuff, rBuff);
	}

	ret = recv(connSock, rBuff, BUFF_SIZE, 0);
	if (ret < 0)
		printf("Stream test fail!\n");
	else {
		rBuff[ret] = 0;
		printf("Main: %s-->%s\n", sBuff, rBuff);
	}

	ret = recv(connSock, rBuff, BUFF_SIZE, 0);
	if (ret < 0)
		printf("Stream test fail!\n");
	else {
		rBuff[ret] = 0;
		printf("Main: %s-->%s\n", sBuff, rBuff);
	}

	closesocket(connSock);
	WSACleanup();

	return 0;
}

