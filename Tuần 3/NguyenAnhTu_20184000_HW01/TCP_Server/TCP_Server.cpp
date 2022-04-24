// TCP_Server.cpp : Defines the entry point for the console application.
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
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData))
    {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET listenSock;
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        printf("Error %d: cannot create server socket\n", WSAGetLastError());
    }

    // Step 3: Bind address to socket
    int serverPort = SERVER_PORT;
    if (argc != 2)
    {
        printf("Usage: UDP_Server <port>\n");
    }
    else
    {
        serverPort = atoi(argv[1]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
    }
    printf("Server started!\n");

    // Step 4: Listen request from client
    if (listen(listenSock, 10))
    {
        printf("Error %d: cannot place server socket in state LISTEN\n", WSAGetLastError());
    }

    // Step 5: Communicate with client
    sockaddr_in clientAddr;
    char buff[SMALL_BUFF_SIZE], clientIP[INET_ADDRSTRLEN];
    int clientAddrLen = sizeof(clientAddr), clientPort;

    SOCKET connSock;
    while (1)
    {
        while (1)
        {
            connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
            if (connSock == SOCKET_ERROR)
            {
                printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
            }
            else
            {
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
                clientPort = ntohs(clientAddr.sin_port);
                printf("Accept incoming connection from [%s:%d]\n", clientIP, clientPort);
                break;
            }
        }

        while (1)
        {
            // Receive request
            bool continueCommunicate = true;
            char reqMess[LARGE_BUFF_SIZE] = "";
            int reqMessLen = 0;
            while (continueCommunicate && (reqMessLen < 2 || reqMess[reqMessLen - 2] != ENDING_DELIMITER[0] || reqMess[reqMessLen - 1] != ENDING_DELIMITER[1]))
            {
                int ret = recv(connSock, buff, SMALL_BUFF_SIZE, 0);
                if (ret == SOCKET_ERROR)
                {
                    printf("Error %d: cannot receive data\n", WSAGetLastError());
                    continueCommunicate = false;
                }
                else if (ret == 0)
                {
                    printf("Client [%s:%d] disconnected!\n", clientIP, clientPort);
                    continueCommunicate = false;
                }
                else
                {
                    buff[ret] = '\0';
                    strcat_s(reqMess, LARGE_BUFF_SIZE, buff);
                    reqMessLen = strlen(reqMess);
                }
            }
            if (!continueCommunicate)
            {
                break;
            }

            // Solve request
            char *reqMessPtr = reqMess;
            while (strlen(reqMessPtr) > 0)
            {
                char reqMessSub[LARGE_BUFF_SIZE];
                for (size_t i = 0; i < strlen(reqMessPtr) - 1; ++i)
                {
                    if (reqMessPtr[i] == ENDING_DELIMITER[0] && reqMessPtr[i + 1] == ENDING_DELIMITER[1])
                    {
                        strncpy_s(reqMessSub, LARGE_BUFF_SIZE, reqMessPtr, i);
                        reqMessSub[i] = '\0';
                        reqMessPtr += i + 2;
                        break;
                    }
                }
                printf("Receive from client [%s:%d]: %s\n", clientIP, clientPort, reqMessSub);
                bool validReq = true;
                int sum = 0;
                for (size_t i = 0; i < strlen(reqMessSub); ++i)
                {
                    if (reqMessSub[i] >= '0' && reqMessSub[i] <= '9')
                    {
                        sum += reqMessSub[i] - '0';
                    }
                    else
                    {
                        validReq = false;
                        break;
                    }
                }
                char resMess[LARGE_BUFF_SIZE] = "";
                if (validReq)
                {
                    char sumStr[LARGE_BUFF_SIZE];
                    sprintf_s(sumStr, LARGE_BUFF_SIZE, "%d", sum);
                    char prefix[1];
                    prefix[0] = RES_PREFIX_SUCCESS;
                    prefix[1] = '\0';
                    strcat_s(resMess, LARGE_BUFF_SIZE, prefix);
                    strcat_s(resMess, LARGE_BUFF_SIZE, sumStr);
                }
                else
                {
                    char prefix[1];
                    prefix[0] = RES_PREFIX_FAIL;
                    prefix[1] = '\0';
                    strcat_s(resMess, LARGE_BUFF_SIZE, prefix);
                    strcat_s(resMess, LARGE_BUFF_SIZE, "NaN");
                }
                strcat_s(resMess, LARGE_BUFF_SIZE, ENDING_DELIMITER);

                // Send response
                char *resMessBegin = resMess;
                while (strlen(resMessBegin) >= SMALL_BUFF_SIZE - 1)
                {
                    strncpy_s(buff, SMALL_BUFF_SIZE, resMessBegin, SMALL_BUFF_SIZE - 1);
                    if (send(connSock, buff, strlen(buff), 0) == SOCKET_ERROR)
                    {
                        printf("Error %d: cannot send response to client\n", WSAGetLastError());
                    }
                    resMessBegin += SMALL_BUFF_SIZE - 1;
                }
                strncpy_s(buff, SMALL_BUFF_SIZE, resMessBegin, strlen(resMessBegin));
                if (send(connSock, buff, strlen(buff), 0) == SOCKET_ERROR)
                {
                    printf("Error %d: cannot send response to client\n", WSAGetLastError());
                }
            }

        } // end communicating
        closesocket(connSock);
    } // end while

    // Step 6: Close socket
    closesocket(listenSock);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
