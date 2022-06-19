// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6600
#define BUFF_SIZE 8192
#define RECEIVE 0
#define SEND 1

#pragma comment(lib, "Ws2_32.lib")

typedef struct SocketInfo {
    SOCKET socket;
    WSAOVERLAPPED overlapped;
    WSABUF dataBuf;
    char buff[BUFF_SIZE];
    int operation;
    int sentBytes;
    int bytesNeedToSend;
} SocketInfo;

void freeSocketInfo(SocketInfo *siArray[], int n);
void closeEventInArray(WSAEVENT eventArr[], int n);

int main(int argc, char *argv[]) {
    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket == INVALID_SOCKET) {
        printf("Error %d: cannot create server socket", WSAGetLastError());
        return 1;
    }

    SocketInfo *socks[WSA_MAXIMUM_WAIT_EVENTS];
    WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
    for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; ++i) {
        socks[i] = 0;
        events[i] = 0;
    }

    events[0] = WSACreateEvent();
    int nEvents = 1;
    WSAEventSelect(listenSocket, events[0], FD_ACCEPT | FD_CLOSE);

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
    if (bind(listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
    }

    // Step 4: Listen request from client
    if (listen(listenSocket, 10)) {
        printf("Error %d: cannot place server socket in state LISTEN", WSAGetLastError());
        return 0;
    }

    printf("Server started!\n");

    // Step 5: Communicate with client
    int index;
    SOCKET connSock;
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (1) {
        // Wait for network events on all socket
        index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
        if (index == WSA_WAIT_FAILED) {
            printf("Error %d: WSAWaitForMultipleEvents() failed\n", WSAGetLastError());
            return 0;
        }

        index = index - WSA_WAIT_EVENT_0;
        DWORD flags, transferredBytes;

        if (index == 0) {  // A connection attempt was made on listening socket
            WSAResetEvent(events[0]);
            if ((connSock = accept(listenSocket, (sockaddr *)&clientAddr, &clientAddrLen)) == INVALID_SOCKET) {
                printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
                return 0;
            }

            if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
                printf("\nToo many clients");
                closesocket(connSock);
            } else {
                // Disassociate connected socket with any event object
                WSAEventSelect(connSock, NULL, 0);

                // Append connected socket to the array of SocketInfo
                int i = nEvents;
                events[i] = WSACreateEvent();
                socks[i] = (SocketInfo *)malloc(sizeof(SocketInfo));
                socks[i]->socket = connSock;
                memset(&socks[i]->overlapped, 0, sizeof(WSAOVERLAPPED));
                socks[i]->overlapped.hEvent = events[i];
                socks[i]->dataBuf.buf = socks[i]->buff;
                socks[i]->dataBuf.len = BUFF_SIZE;
                socks[i]->operation = RECEIVE;
                socks[i]->sentBytes = 0;
                socks[i]->bytesNeedToSend = 0;

                nEvents++;

                // Post an overlapped I/O request to begin receiving data on the socket
                flags = 0;
                if (WSARecv(socks[i]->socket, &(socks[i]->dataBuf), 1, &transferredBytes, &flags, &(socks[i]->overlapped), NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != WSA_IO_PENDING) {
                        printf("WSARecv() failed with error %d\n", WSAGetLastError());
                        closeEventInArray(events, i);
                        freeSocketInfo(socks, i);
                        nEvents--;
                    }
                }
            }
        } else {  // An I/O request is completed
            SocketInfo *client;
            client = socks[index];
            WSAResetEvent(events[index]);
            BOOL result = WSAGetOverlappedResult(client->socket, &(client->overlapped), &transferredBytes, FALSE, &flags);
            if (result == FALSE || transferredBytes == 0) {
                closesocket(client->socket);
                closeEventInArray(events, index);
                freeSocketInfo(socks, index);
                client = 0;
                nEvents--;
                continue;
            }

            // Check to see if a WSARecv() call just completed
            if (client->operation == RECEIVE) {
                client->bytesNeedToSend = transferredBytes;
                client->sentBytes = 0;
                client->operation = SEND;
            } else {
                client->sentBytes += transferredBytes;
            }

            // Post another I/O operation
            // Since WSASend() is not guaranteed to send all of the bytes requested,
            // continue posting WSASend() calls until all received bytes are sent.
            if (client->sentBytes < client->bytesNeedToSend) {
                client->dataBuf.buf = client->buff + client->sentBytes;
                client->dataBuf.len = client->bytesNeedToSend - client->sentBytes;
                client->operation = SEND;
                if (WSASend(client->socket, &(client->dataBuf), 1, &transferredBytes, flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != WSA_IO_PENDING) {
                        printf("WSASend() failed with error %d\n", WSAGetLastError());
                        closesocket(client->socket);
                        closeEventInArray(events, index);
                        freeSocketInfo(socks, index);
                        client = 0;
                        nEvents--;
                        continue;
                    }
                }
            } else {
                // No more bytes to send post another WSARecv() request
                memset(&(client->overlapped), 0, sizeof(WSAOVERLAPPED));
                client->overlapped.hEvent = events[index];
                client->bytesNeedToSend = 0;
                client->operation = RECEIVE;
                client->dataBuf.buf = client->buff;
                client->dataBuf.len = BUFF_SIZE;
                flags = 0;
                if (WSARecv(client->socket, &(client->dataBuf), 1, &transferredBytes, &flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != WSA_IO_PENDING) {
                        printf("WSARecv() failed with error %d\n", WSAGetLastError());
                        closesocket(client->socket);
                        closeEventInArray(events, index);
                        freeSocketInfo(socks, index);
                        client = 0;
                        nEvents--;
                    }
                }
            }
        }
    }

    // Step 6: Close sockets
    closesocket(listenSocket);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}

/**
 * The freeSocketInfo function remove a socket from array
 * @param siArray An array of pointers of socket information struct
 * @param n Index of the removed socket
 */

void freeSocketInfo(SocketInfo *siArray[], int n) {
    closesocket(siArray[n]->socket);
    free(siArray[n]);
    siArray[n] = 0;
    for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; ++i) {
        siArray[i] = siArray[i + 1];
    }
}

/**
 * The closeEventInArray function release an event and remove it from an array
 * @param eventArr An array of event object handles
 * @param n Index of the removed event object
 */
void closeEventInArray(WSAEVENT eventArr[], int n) {
    WSACloseEvent(eventArr[n]);

    for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; ++i) {
        eventArr[i] = eventArr[i + 1];
    }
}
