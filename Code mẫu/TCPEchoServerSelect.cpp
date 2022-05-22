// SelectTCPServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
    // Step 1: Initiate WinSock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData))
    {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

    // Step 5: Communicate with clients
    SOCKET client[FD_SETSIZE], connSock;
    for (int i = 0; i < FD_SETSIZE; ++i)
    {
        client[i] = 0;
    }

    fd_set readfds, initfds;
    FD_ZERO(&initfds);
    FD_SET(listenSock, &initfds);

    sockaddr_in clientAddr;
    int ret, nEvents, clientAddrLen;
    char recvBuff[BUFF_SIZE], sendBuff[BUFF_SIZE];

    while (1)
    {
        readfds = initfds; // structure assignment
        nEvents = select(0, &readfds, 0, 0, 0);
        if (nEvents < 0)
        {
            printf("Error %d: cannot poll sockets\n", WSAGetLastError());
            break;
        }

        // new client connection
        if (FD_ISSET(listenSock, &readfds))
        {
            clientAddrLen = sizeof(clientAddr);
            if ((connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen)) < 0)
            {
                printf("Error %d: cannot accept new connection\n", WSAGetLastError());
                break;
            }
            else
            {
                printf("Accept incoming connection from [%s:%d]\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

                bool unaccepted = true;
                for (int i = 0; i < FD_SETSIZE && unaccepted; ++i)
                {
                    if (client[i] == 0)
                    {
                        client[i] = connSock;
                        FD_SET(client[i], &initfds);
                        unaccepted = false;
                    }
                }
                if (unaccepted)
                {
                    printf("Error: cannot response more client\n");
                    closesocket(connSock);
                }

                if (--nEvents == 0)
                {
                    continue;
                }
            }
        }

        // receive data from clients
        for (int i = 0; i < FD_SETSIZE; ++i)
        {
            if (client[i] == 0)
            {
                continue;
            }

            if (FD_ISSET(client[i], &readfds))
            {
                // Receive request
                ret = recv(client[i], recvBuff, BUFF_SIZE, 0);
                if (ret <= 0)
                {
                    if (ret == SOCKET_ERROR && WSAGetLastError() != WSAECONNRESET)
                    {
                        printf("Error %d: cannot receive data\n", WSAGetLastError());
                    }
                    FD_CLR(client[i], &initfds);
                    closesocket(client[i]);
                    client[i] = 0;
                }
                else
                {
                    // strcpy_s(sendBuff, BUFF_SIZE, recvBuff);
                    memcpy_s(sendBuff, BUFF_SIZE, recvBuff, BUFF_SIZE);

                    // Send response
                    ret = send(client[i], sendBuff, ret, 0);
                    if (ret == SOCKET_ERROR)
                    {
                        printf("Error %d: cannot send data\n", WSAGetLastError());
                    }
                }
            }

            if (--nEvents <= 0)
            {
                continue;
            }
        }
    }

    // Step 6: Close socket
    closesocket(listenSock);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
