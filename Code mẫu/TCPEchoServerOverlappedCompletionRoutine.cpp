// Server.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>

#include "stdafx.h"
#include "ws2tcpip.h"

#pragma comment(lib, "Ws2_32.lib")

#include <process.h>

#define RECEIVE 0
#define SEND 1
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6600
#define BUFF_SIZE 8192
#define MAX_CLIENT 1024

typedef struct SocketInfo {
    WSAOVERLAPPED overlapped;
    SOCKET socket;
    WSABUF dataBuf;
    char buff[BUFF_SIZE];
    int operation;
    int sentBytes;
    int bytesNeedToSend;
} SocketInfo;

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags);
unsigned __stdcall IoThread(LPVOID lpParameter);

SOCKET acceptSocket;
SocketInfo *clients[MAX_CLIENT];
int nClients = 0;
CRITICAL_SECTION criticalSection;

int main(int argc, char *argv[]) {
    SOCKET listenSocket;
    SOCKADDR_IN clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    WSAEVENT acceptEvent;

    InitializeCriticalSection(&criticalSection);

    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        printf("Winsock 2.2 is not supported\n");
    }

    if ((listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
        printf("Failed to get a socket %d\n", WSAGetLastError());
        return 1;
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
    if (bind(listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
    }

    if (listen(listenSocket, 20)) {
        printf("listen() failed with error %d\n", WSAGetLastError());
        return 1;
    }

    printf("Server started!\n");

    if ((acceptEvent = WSACreateEvent()) == WSA_INVALID_EVENT) {
        printf("WSACreateEvent() failed with error %d\n", WSAGetLastError());
        return 1;
    }

    // Create a worker thread to service completed I/O requests
    _beginthreadex(0, 0, IoThread, (LPVOID)acceptEvent, 0, 0);

    while (TRUE) {
        if ((acceptSocket = accept(listenSocket, (PSOCKADDR)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
            printf("accept() failed with error %d\n", WSAGetLastError());
            return 1;
        }

        if (WSASetEvent(acceptEvent) == FALSE) {
            printf("WSASetEvent() failed with error %d\n", WSAGetLastError());
            return 1;
        }
    }
    return 0;
}

unsigned __stdcall IoThread(LPVOID lpParameter) {
    DWORD flags;
    WSAEVENT events[1];
    DWORD index;
    DWORD bytesNeedToSend;

    // Save the accept event in the event array
    events[0] = (WSAEVENT)lpParameter;
    while (TRUE) {
        // Wait for accept() to signal an event and also process workerRoutine() returns
        while (TRUE) {
            index = WSAWaitForMultipleEvents(1, events, FALSE, WSA_INFINITE, TRUE);
            if (index == WSA_WAIT_FAILED) {
                printf("WSAWaitForMultipleEvents() failed with error %d\n", WSAGetLastError());
                return 1;
            }

            if (index != WAIT_IO_COMPLETION) {
                // An accept() call event is ready - break the wait loop
                break;
            }
        }

        WSAResetEvent(events[index - WSA_WAIT_EVENT_0]);

        EnterCriticalSection(&criticalSection);

        if (nClients == MAX_CLIENT) {
            printf("Too many clients.\n");
            closesocket(acceptSocket);
            continue;
        }

        // Create a socket information structure to associate with the accepted socket
        if ((clients[nClients] = (SocketInfo *)GlobalAlloc(GPTR, sizeof(SocketInfo))) == NULL) {
            printf("GlobalAlloc() failed with error %d\n", GetLastError());
            return 1;
        }

        // Fill in the details of our accepted socket
        clients[nClients]->socket = acceptSocket;
        memset(&clients[nClients]->overlapped, 0, sizeof(WSAOVERLAPPED));
        clients[nClients]->sentBytes = 0;
        clients[nClients]->bytesNeedToSend = 0;
        clients[nClients]->dataBuf.len = BUFF_SIZE;
        clients[nClients]->dataBuf.buf = clients[nClients]->buff;
        clients[nClients]->operation = RECEIVE;
        flags = 0;

        if (WSARecv(clients[nClients]->socket, &(clients[nClients]->dataBuf), 1, &bytesNeedToSend,
                    &flags, &(clients[nClients]->overlapped), workerRoutine) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                printf("WSARecv() failed with error %d\n", WSAGetLastError());
                return 1;
            }
        }

        printf("Socket %d got connected...\n", acceptSocket);
        nClients++;
        LeaveCriticalSection(&criticalSection);
    }

    return 0;
}

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags) {
    DWORD sendBytes, bytesNeedToSend;
    DWORD flags;

    // Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
    SocketInfo *sockInfo = (SocketInfo *)overlapped;

    if (error != 0)
        printf("I/O operation failed with error %d\n", error);

    else if (transferredBytes == 0)
        printf("Closing socket %d\n\n", sockInfo->socket);

    if (error != 0 || transferredBytes == 0) {
        // Find and remove socket
        EnterCriticalSection(&criticalSection);

        int index;
        for (index = 0; index < nClients; index++)
            if (clients[index]->socket == sockInfo->socket)
                break;

        closesocket(clients[index]->socket);
        GlobalFree(clients[index]);
        clients[index] = 0;

        for (int i = index; i < nClients - 1; i++)
            clients[i] = clients[i + 1];
        nClients--;

        LeaveCriticalSection(&criticalSection);

        return;
    }

    // Check to see if the bytesNeedToSend field equals zero. If this is so, then
    // this means a WSARecv call just completed so update the bytesNeedToSend field
    // with the transferredBytes value from the completed WSARecv() call
    if (sockInfo->operation == RECEIVE) {
        sockInfo->bytesNeedToSend = transferredBytes;
        sockInfo->sentBytes = 0;
        sockInfo->operation = SEND;
    } else {
        sockInfo->sentBytes += transferredBytes;
    }

    if (sockInfo->bytesNeedToSend > sockInfo->sentBytes) {
        // Post another WSASend() request.
        // Since WSASend() is not guaranteed to send all of the bytes requested,
        // continue posting WSASend() calls until all received bytes are sent
        ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
        sockInfo->dataBuf.buf = sockInfo->buff + sockInfo->sentBytes;
        sockInfo->dataBuf.len = sockInfo->bytesNeedToSend - sockInfo->sentBytes;
        sockInfo->operation = SEND;
        if (WSASend(sockInfo->socket, &(sockInfo->dataBuf), 1, &sendBytes, 0, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                printf("WSASend() failed with error %d\n", WSAGetLastError());
                return;
            }
        }
    } else {
        // Now that there are no more bytes to send post another WSARecv() request
        sockInfo->bytesNeedToSend = 0;
        flags = 0;
        ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
        sockInfo->dataBuf.len = BUFF_SIZE;
        sockInfo->dataBuf.buf = sockInfo->buff;
        sockInfo->operation = RECEIVE;
        if (WSARecv(sockInfo->socket, &(sockInfo->dataBuf), 1, &bytesNeedToSend, &flags, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                printf("WSARecv() failed with error %d\n", WSAGetLastError());
                return;
            }
        }
    }
}
