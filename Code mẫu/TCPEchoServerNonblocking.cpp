// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048
#define MAX_CLIENT 3

#pragma comment(lib, "Ws2_32.lib")

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
    unsigned long ul = 1;
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        printf("Error %d: Cannot create server socket.", WSAGetLastError());
    }

    if (ioctlsocket(listenSock, FIONBIO, (unsigned long *)&ul))
    {
        printf("Error! Cannot change to non-blocking mode.");
        return 0;
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
    char buff[BUFF_SIZE];
    int ret, clientAddrLen = sizeof(clientAddr);
    SOCKET client[MAX_CLIENT];
    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        client[i] = 0;
    }

    SOCKET connSock;
    while (1)
    {
        connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
        if (connSock != SOCKET_ERROR)
        {
            printf("Accept incoming connection from [%s:%d]\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            bool unaccepted = true;
            for (int i = 0; i < MAX_CLIENT && unaccepted; ++i)
            {
                if (client[i] == 0)
                {
                    client[i] = connSock;
                    unaccepted = false;
                }
            }
            if (unaccepted)
            {
                printf("Error: cannot response more client\n");
                closesocket(connSock);
            }
        }

        for (int i = 0; i < MAX_CLIENT; ++i)
        {
            if (client[i] == 0)
            {
                continue;
            }

            // receive message from client
            ret = recv(client[i], buff, BUFF_SIZE, 0);
            if (ret == SOCKET_ERROR && WSAGetLastError() != WSAECONNRESET)
            {
                if (WSAGetLastError() != WSAEWOULDBLOCK)
                {
                    printf("Error %d: cannot receive message from client\n", WSAGetLastError());
                    closesocket(client[i]);
                    client[i] = 0;
                }
            }
            else if (ret == 0 || WSAGetLastError() == WSAECONNRESET)
            {
                buff[0] = '\0';
                closesocket(client[i]);
                client[i] = 0;
            }
            else
            {
                buff[ret] = 0;
                printf("Receive from client[%s:%d] %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff);

                // echo to client
                ret = send(client[i], buff, strlen(buff), 0);
                if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
                {
                    printf("Error %d: cannot send message to client\n", WSAGetLastError());
                    closesocket(client[i]);
                    client[i] = 0;
                }
            }
        }
    } // end accepting

    // Step 5: Close socket
    closesocket(listenSock);

    // Step 6: Terminate Winsock
    WSACleanup();

    return 0;
}
