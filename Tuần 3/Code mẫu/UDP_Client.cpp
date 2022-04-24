// UDP_Client.cpp : Defines the entry point for the console application.
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
    WORD wVersion MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData))
    {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET client;
    client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client == INVALID_SOCKET)
    {
        printf("Error %d: cannot create server socket\n", WSAGetLastError());
    }
    printf("Client started!\n");

    // (optional) Set time-out for receiving
    int tv = 10000; // Time-out interval: 10000ms
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&tv), sizeof(int));

    // Step 3: Specify server address
    char serverAddrStr[BUFF_SIZE] = SERVER_ADDR;
    int serverPort = SERVER_PORT;
    if (argc != 3)
    {
        printf("Usage: UDP_Client <server_addr> <server_port>\n");
    }
    else
    {
        strcpy_s(serverAddrStr, BUFF_SIZE, argv[1]);
        serverPort = atoi(argv[2]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverAddrStr, &serverAddr.sin_addr);

    // Step 4: Communicate with server
    char buff[BUFF_SIZE];
    int ret, serverAddrLen = sizeof(serverAddr);
    while (1)
    {
        // Send message
        printf("Send to server: ");
        gets_s(buff, BUFF_SIZE);
        int messageLen = strlen(buff);
        if (messageLen == 0)
        {
            break;
        }
        // TODO: Add delimiter to message
        ret = sendto(client, buff, messageLen, 0, (sockaddr *)&serverAddr, serverAddrLen);
        if (ret == SOCKET_ERROR)
        {
            printf("Error %d: cannot send mesage\n", WSAGetLastError());
        }

        // Receive echo message
        ret = recvfrom(client, buff, BUFF_SIZE, 0, (sockaddr *)&serverAddr, &serverAddrLen);
        if (ret == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT)
            {
                printf("Time-out!\n");
            }
            else
            {
                printf("Error %d: cannot receive message\n", WSAGetLastError());
            }
        }
        else if (strlen(buff) > 0)
        {
            buff[ret] = '\0';
            printf("Receive from server: %s\n", buff);
        }
    } // end while

    // Step 5: Close socket
    closesocket(client);

    // Step 6: Terminate Winsock
    WSACleanup();

    return 0;
}
