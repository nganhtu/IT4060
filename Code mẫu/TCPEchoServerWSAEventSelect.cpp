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

int main(int argc, char *argv[]) {
    DWORD nEvents = 0, index;
    SOCKET socks[WSA_MAXIMUM_WAIT_EVENTS];
    WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
    WSANETWORKEVENTS sockEvent;

    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct LISTEN socket
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

    socks[0] = listenSock;
    events[0] = WSACreateEvent();
    nEvents++;

    WSAEventSelect(socks[0], events[0], FD_ACCEPT | FD_CLOSE);

    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
        return 0;
    }
    printf("Server started!\n");

    // Step 4: Listen request from client
    if (listen(listenSock, 10)) {
        printf("Error %d: cannot place server socket in state LISTEN\n", WSAGetLastError());
    }

    // Step 5: Communicate with client
    char sendBuff[BUFF_SIZE], recvBuff[BUFF_SIZE];
    SOCKET connSock;
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    int ret, i;

    for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        socks[i] = 0;
    }
    while (1) {
        // wait for network events on all socket
        index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
        if (index == WSA_WAIT_FAILED) {
            printf("Error %d: WSAWaitForMultipleEvents() failed\n", WSAGetLastError());
            break;
        }

        index = index - WSA_WAIT_EVENT_0;
        WSAEnumNetworkEvents(socks[index], events[index], &sockEvent);

        if (sockEvent.lNetworkEvents & FD_ACCEPT) {
            if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
                printf("FD_ACCEPT failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
                break;
            }

            if ((connSock = accept(socks[index], (sockaddr *)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
                printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
                break;
            }

            int i;
            if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
                printf("Too many clients\n");
                closesocket(connSock);
            } else {
                for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++)
                    if (socks[i] == 0) {
                        socks[i] = connSock;
                        events[i] = WSACreateEvent();
                        WSAEventSelect(socks[i], events[i], FD_READ | FD_CLOSE);
                        nEvents++;
                        break;
                    }
            }

            WSAResetEvent(events[index]);
        }

        if (sockEvent.lNetworkEvents & FD_READ) {
            if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
                printf("FD_READ failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
                break;
            }

            // Receive message
            ret = recv(socks[index], recvBuff, BUFF_SIZE, 0);
            if (ret == SOCKET_ERROR || ret == 0) {
                printf("Error %d: cannot receive data from client\n", WSAGetLastError());
                closesocket(socks[index]);
                socks[index] = 0;
                WSACloseEvent(events[index]);
                nEvents--;
            } else {
                // Solve message
                memcpy(sendBuff, recvBuff, ret);

                // Send response
                ret = send(socks[index], sendBuff, ret, 0);
                if (ret == SOCKET_ERROR) {
                    printf("Error %d: cannot send data\n", WSAGetLastError());
                }

                WSAResetEvent(events[index]);
            }
        }

        if (sockEvent.lNetworkEvents & FD_CLOSE) {
            if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
                if (sockEvent.iErrorCode[FD_CLOSE_BIT] == WSAECONNABORTED) {
                    printf("Connection reset by peer\n");
                } else {
                    printf("FD_CLOSE failed with error %d\n", sockEvent.iErrorCode[FD_CLOSE_BIT]);
                    break;
                }
            }
            // Release socket and event
            sockEvent.iErrorCode[FD_CLOSE_BIT] = 0;
            closesocket(socks[index]);
            socks[index] = 0;
            WSACloseEvent(events[index]);
            nEvents--;
        }
    }

    // Step 6: Close socket
    closesocket(listenSock);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
