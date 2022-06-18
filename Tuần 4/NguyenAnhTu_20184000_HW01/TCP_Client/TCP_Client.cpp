// TCP_Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define LARGE_BUFF_SIZE 2048
#define SMALL_BUFF_SIZE 1024
#define RES_PREFIX_SUCCESS '+'
#define RES_PREFIX_FAIL '-'
#define ENDING_DELIMITER "\r\n"

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData))
    {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET client;
    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET)
    {
        printf("Error %d: cannot create client socket\n", WSAGetLastError());
    }
    printf("Client started!\n");

    // (optional) Set time-out for receiving
    int tv = 10000; // Time-out interval: 10000ms
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&tv), sizeof(int));

    // Step 3: Specify server address
    char serverAddrStr[LARGE_BUFF_SIZE] = SERVER_ADDR;
    int serverPort = SERVER_PORT;
    if (argc != 3)
    {
        printf("Usage: UDP_Client <server_addr> <server_port>\n");
    }
    else
    {
        strcpy_s(serverAddrStr, LARGE_BUFF_SIZE, argv[1]);
        serverPort = atoi(argv[2]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverAddrStr, &serverAddr.sin_addr);

    // Step 4: Connect to server
    if (connect(client, (sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        printf("Error %d: cannot connect to server\n", WSAGetLastError());
    }
    printf("Connected to server!\n");

    // Step 5: Communicate with server
    char buff[SMALL_BUFF_SIZE];
    while (1)
    {
        // Send request
        char reqMess[LARGE_BUFF_SIZE];
        printf("Send to server: ");
        gets_s(reqMess, LARGE_BUFF_SIZE);
        if (strlen(reqMess) == 0)
        {
            break;
        }
        strcat_s(reqMess, LARGE_BUFF_SIZE, ENDING_DELIMITER);
        char *reqMessBegin = reqMess;
        while (strlen(reqMessBegin) >= SMALL_BUFF_SIZE - 1)
        {
            strncpy_s(buff, SMALL_BUFF_SIZE, reqMessBegin, SMALL_BUFF_SIZE - 1);
            if (send(client, buff, strlen(buff), 0) == SOCKET_ERROR)
            {
                printf("Error %d: cannot send message to server\n", WSAGetLastError());
            }
            else
            {
                printf("Sent to server: \"%s\"\n", buff);
            }
            reqMessBegin += SMALL_BUFF_SIZE - 1;
        }
        strncpy_s(buff, SMALL_BUFF_SIZE, reqMessBegin, strlen(reqMessBegin));
        if (send(client, buff, strlen(buff), 0) == SOCKET_ERROR)
        {
            printf("Error %d: cannot send message to server\n", WSAGetLastError());
        }
        else
        {
            printf("Sent to server: \"%s\"\n", buff);
        }

        // Receive response
        char resMess[LARGE_BUFF_SIZE] = "";
        int resMessLen = 0;
        while (resMessLen < 2 || resMess[resMessLen - 2] != ENDING_DELIMITER[0] || resMess[resMessLen - 1] != ENDING_DELIMITER[1])
        {
            int ret = recv(client, buff, SMALL_BUFF_SIZE, 0);
            if (ret == SOCKET_ERROR)
            {
                if (WSAGetLastError() == WSAETIMEDOUT)
                {
                    printf("Error %d: time-out\n", WSAGetLastError());
                }
                else
                {
                    printf("Error %d: cannot receive message from server\n", WSAGetLastError());
                    break;
                }
            }
            else if (ret > 0)
            {
                buff[ret] = '\0';
                strcat_s(resMess, LARGE_BUFF_SIZE, buff);
                resMessLen = strlen(resMess);
            }
        }

        // Resolve response
        char *resMessPtr = resMess;
        while (strlen(resMessPtr) > 0)
        {
            char resMessSub[LARGE_BUFF_SIZE];
            for (size_t i = 0; i < strlen(resMessPtr) - 1; ++i)
            {
                if (resMessPtr[i] == ENDING_DELIMITER[0] && resMessPtr[i + 1] == ENDING_DELIMITER[1])
                {
                    strncpy_s(resMessSub, LARGE_BUFF_SIZE, resMessPtr, i);
                    resMessSub[i] = '\0';
                    resMessPtr += i + 2;
                    break;
                }
            }
            if (resMessSub[0] != RES_PREFIX_SUCCESS && resMessSub[0] != RES_PREFIX_FAIL)
            {
                printf("Error: invalid prefix from server\n");
            }
            else if (resMessSub[0] == RES_PREFIX_FAIL)
            {
                if (strcmp(resMessSub + 1, "NaN") == 0)
                {
                    printf("Failed: String contains non-number character\n");
                }
                else
                {
                    printf("Error: %s\n", resMessSub + 1);
                }
            }
            else
            {
                printf("Sum of all digits: %s\n", resMessSub + 1);
            }
        }
    } // end while

    // Step 6: Close socket
    closesocket(client);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
