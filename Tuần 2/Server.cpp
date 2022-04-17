// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "stdio.h"
#pragma comment(lib, "Ws2_32.lib")

#define BUFF_SIZE 2048
#define REQ_PREFIX_FORWARD 'F'
#define REQ_PREFIX_REVERSE 'R'
#define RES_PREFIX_SUCCESS '+'
#define RES_PREFIX_FAIL '-'
#define RES_INFIX_SPLIT '\a'
#define ERROR_REQ_PREFIXNOTRECOGNIZED 12001

int main(int argc, char *argv[])
{
	// Initiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData) == SOCKET_ERROR)
	{
		printf("WinSock startup failed with error code %d", WSAGetLastError());
		return 0;
	}

	// Create request message
	char mode = '\0';
	while (mode != REQ_PREFIX_FORWARD && mode != REQ_PREFIX_REVERSE)
	{
		printf("How do you want to resolve? [%c/%c]\n", REQ_PREFIX_FORWARD, REQ_PREFIX_REVERSE);
		scanf_s("%c", &mode, 1);
	}
	char inputStr[BUFF_SIZE] = "", requestMess[BUFF_SIZE] = "";
	if (mode == REQ_PREFIX_FORWARD)
	{
		printf("Enter host name: ");
	}
	else
	{
		printf("Enter IP address: ");
	}
	scanf_s("%s", inputStr, BUFF_SIZE);
	requestMess[0] = mode;
	strcat_s(requestMess, BUFF_SIZE, inputStr);

	// Handle message
	char responseMess[BUFF_SIZE] = "";
	if (strlen(requestMess) == 1)
	{
		// TODO handle "client shut down" behavior
	}
	else if (requestMess[0] == REQ_PREFIX_FORWARD)
	{
		// Resolve host name
		addrinfo hints;
		hints.ai_family = AF_INET;
		memset(&hints, 0, sizeof(hints));
		addrinfo *result;
		if (getaddrinfo(requestMess + 1, NULL, &hints, &result) != 0)
		{
			printf("Get address info failed with error code %d\n", WSAGetLastError());
			char errorCodeStr[BUFF_SIZE] = "";
			sprintf_s(errorCodeStr, BUFF_SIZE, "%d", WSAGetLastError());
			responseMess[0] = RES_PREFIX_FAIL;
			strcat_s(responseMess, BUFF_SIZE, errorCodeStr);
		}
		else
		{
			responseMess[0] = RES_PREFIX_SUCCESS;
			sockaddr_in *address;
			while (result != NULL)
			{
				address = (struct sockaddr_in *)result->ai_addr;
				char ipStr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &address->sin_addr, ipStr, sizeof(ipStr));
				strcat_s(responseMess, BUFF_SIZE, ipStr);
				char split = RES_INFIX_SPLIT;
				strncat_s(responseMess, BUFF_SIZE, &split, 1);
				result = result->ai_next;
			}
			responseMess[strlen(responseMess) - 1] = '\0';
		}
		freeaddrinfo(result);
	}
	else if (requestMess[0] == REQ_PREFIX_REVERSE)
	{
		// Resolve IP address
		struct sockaddr_in addr;
		char hostname[NI_MAXHOST], serverInfo[NI_MAXSERV];
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, requestMess + 1, &addr.sin_addr);
		if (getnameinfo((struct sockaddr *)&addr, sizeof(struct sockaddr), hostname, NI_MAXHOST, serverInfo, NI_MAXSERV, NI_NUMERICSERV) != 0)
		{
			printf("Get host name info failed with error code %d\n", WSAGetLastError());
			char errorCodeStr[BUFF_SIZE] = "";
			sprintf_s(errorCodeStr, BUFF_SIZE, "%d", WSAGetLastError());
			responseMess[0] = RES_PREFIX_FAIL;
			strcat_s(responseMess, BUFF_SIZE, errorCodeStr);
		}
		else
		{
			responseMess[0] = RES_PREFIX_SUCCESS;
			strcat_s(responseMess, BUFF_SIZE, hostname);
		}
	}
	else
	{
		printf("Unexpected request \"%s\"\n", requestMess);
		char errorCodeStr[BUFF_SIZE] = "";
		sprintf_s(errorCodeStr, BUFF_SIZE, "%d", ERROR_REQ_PREFIXNOTRECOGNIZED);
		responseMess[0] = RES_PREFIX_FAIL;
		strcat_s(responseMess, BUFF_SIZE, errorCodeStr);
	}

	// Handle response message
	if (responseMess[0] != RES_PREFIX_FAIL && responseMess[0] != RES_PREFIX_SUCCESS)
	{
		printf("Response prefix not recognized in message \"%s\"\n", requestMess);
	}
	else if (responseMess[0] == RES_PREFIX_FAIL)
	{
		long errorCode = strtol(responseMess + 1, NULL, 10);
		if (errorCode == WSAHOST_NOT_FOUND)
		{
			printf("Not found information\n");
		}
		else if (errorCode == ERROR_REQ_PREFIXNOTRECOGNIZED)
		{
			printf("Resquest prefix not recognized\n");
		}
		else
		{
			printf("Handle request failed with error code %ld\n", errorCode);
		}
	}
	else
	{
		char responseMessWithoutPrefix[BUFF_SIZE] = "";
		strcat_s(responseMessWithoutPrefix, BUFF_SIZE, responseMess + 1);
		if (mode == REQ_PREFIX_FORWARD)
		{
			for (int i = 0; i < strlen(responseMessWithoutPrefix); ++i)
			{
				if (responseMessWithoutPrefix[i] == RES_INFIX_SPLIT)
				{
					responseMessWithoutPrefix[i] = '\n';
				}
			}
			printf("IP address(es):\n%s\n", responseMessWithoutPrefix);
		}
		else
		{
			printf("Host name:\n%s\n", responseMessWithoutPrefix);
		}
	}

	WSACleanup();

	return 0;
}
