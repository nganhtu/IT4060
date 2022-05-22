// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048

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
    SOCKET server;
    server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server == INVALID_SOCKET)
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
    if (bind(server, (sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        printf("Error %d: cannot bind this addres\n", WSAGetLastError());
    }
    printf("Server started!\n");

    // Step 4: Communicate with client
    sockaddr_in clientAddr;
    char buff[BUFF_SIZE];
    int ret, clientAddrLen = sizeof(clientAddr);

    while (1)
    {
        // Receive message
        ret = recvfrom(server, buff, BUFF_SIZE, 0, (sockaddr *)&clientAddr, &clientAddrLen);
        if (ret == SOCKET_ERROR && WSAGetLastError() != WSAECONNRESET)
        {
            printf("Error %d: cannot receive data\n", WSAGetLastError());
            break;
        }
        else if (ret == 0 || WSAGetLastError() == WSAECONNRESET)
        {
            buff[0] = '\0';
            break;
        }
        else
        {
            buff[ret] = '\0';
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
            int clientPort = ntohs(clientAddr.sin_port);
            printf("Receive from client [%s:%d]: %s\n", clientIP, clientPort, buff);

            // Echo to client
            ret = sendto(server, buff, strlen(buff), 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
            if (ret == SOCKET_ERROR)
            {
                printf("Error %d: cannot send data\n", WSAGetLastError());
            }
        }
    } // end while

    // Step 5: Close socket
    closesocket(server);

    // Step 6: Terminate Winsock
    WSACleanup();

    return 0;
}
