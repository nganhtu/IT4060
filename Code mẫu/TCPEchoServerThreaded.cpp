// Server.cpp : Defines the entry point for the console application.
//

#include "process.h"
#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

/* commThread - Thread to receive request from client, solve, and respond to client */
unsigned __stdcall commThread(void *param) {
    SOCKET connSock = (SOCKET)param;
    char buff[BUFF_SIZE];
    int ret;

    while (1) {
        ret = recv(connSock, buff, BUFF_SIZE, 0);
        if (ret == SOCKET_ERROR && WSAGetLastError() != WSAECONNRESET) {
            printf("Error %d: cannot receive data\n", WSAGetLastError());
            break;
        } else if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
            buff[0] = '\0';
            break;
        } else {
            buff[ret] = '\0';
            printf("Receive from client [%s:%d]: %s\n", "unknown", 404, buff);

            // Send response
            ret = send(connSock, buff, strlen(buff), 0);
            if (ret == SOCKET_ERROR) {
                printf("Error %d: cannot send data\n", WSAGetLastError());
            }
        }
    }  // end communicating

    printf("Client [%s:%d] disconnected!\n", "unknown", 404);
    closesocket(connSock);

    return 0;
}

int main(int argc, char *argv[]) {
    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("Error %d: cannot create server socket\n", WSAGetLastError());
    }

    // Step 3: Bind address to socket
    int serverPort = SERVER_PORT;
    if (argc != 2) {
        printf("Usage: UDP_Server <port>\n");
    } else {
        serverPort = atoi(argv[1]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
    }
    printf("Server started!\n");

    // Step 4: Listen request from client
    if (listen(listenSock, 10)) {
        printf("Error %d: cannot place server socket in state LISTEN\n", WSAGetLastError());
    }

    // Step 5: Communicate with client
    SOCKET connSock;
    sockaddr_in clientAddr;
    char clientIP[INET_ADDRSTRLEN];
    int clientAddrLen = sizeof(clientAddr), clientPort;
    while (1) {
        connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
        if (connSock == SOCKET_ERROR) {
            printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
        } else {
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
            clientPort = ntohs(clientAddr.sin_port);
            printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);

            _beginthreadex(0, 0, commThread, (void *)connSock, 0, 0);
        }
    }

    // Step 6: Close socket
    closesocket(listenSock);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
