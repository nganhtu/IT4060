// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#pragma comment(lib, "Ws2_32.lib")

#include "process.h"

#define RECEIVE 0
#define SEND 1
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6600
#define MAX_CLIENT 1024

#define OPCODE_SIZE 1
#define LENGTH_SIZE 4
#define PAYLOAD_SIZE 9999
#define BUFF_SIZE OPCODE_SIZE + LENGTH_SIZE + PAYLOAD_SIZE + 1
#define OPCODE_INVALID -1
#define OPCODE_ENCODE 0
#define OPCODE_DECODE 1
#define OPCODE_TRANSFER 2
#define OPCODE_ERROR 3
#define OPCODE_ACK 4
#define PHASE_RECVKEYSIZE 0
#define PHASE_RECVKEY 1
#define PHASE_RECVFILESIZE 2
#define PHASE_RECVFILE 3
#define PHASE_SENDFILESIZE 4
#define PHASE_SENDFILE 5

typedef struct SocketInfo {
    WSAOVERLAPPED overlapped;
    SOCKET socket;
    WSABUF dataBuf;
    char buff[BUFF_SIZE];
    int operation;
    int sentBytes;
    int bytesNeedToSend;
} SocketInfo;

typedef struct Session {
    int phase;
    int mode;
    int key;
    int length;
    char serverFilePath[MAX_PATH];
    void *vBuff;
    FILE *pSer;
} Session;

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags);
unsigned __stdcall IoThread(LPVOID lpParameter);
int countDigit(int);
const char *numToStr(int, int);
int strToNum(const char *);
const char *createOpcodeAndLength(int, int);
const char *generateTmpFileName();
int encode(int, int);
int decode(int, int);
void encode(int, void *, int);
void decode(int, void *, int);
void clearSession(Session *);
void closeSession(Session sessions[], int n);

SOCKET acceptSocket;
SocketInfo *clients[MAX_CLIENT];
Session sessions[MAX_CLIENT];
int nClients = 0;
CRITICAL_SECTION criticalSection;

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    for (int i = 0; i < MAX_CLIENT; ++i) {
        sessions[i].phase = PHASE_RECVKEYSIZE;
        sessions[i].vBuff = malloc(BUFF_SIZE);
        sessions[i].pSer = NULL;
    }

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
            printf("Too many clients\n");
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
        sessions[nClients].phase = PHASE_RECVKEYSIZE;
        sessions[nClients].pSer = NULL;

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
    DWORD sendBytes, bytesNeedToSend, flags;

    // Reference the WSAOVERLAPPED structure as a SOCKET_INFORMATION structure
    SocketInfo *client = (SocketInfo *)overlapped;

    if (error != 0) {
        printf("I/O operation failed with error %d\n", error);
    } else if (transferredBytes == 0) {
        printf("Closing socket %d\n\n", client->socket);
    }

    if (error != 0 || transferredBytes == 0) {
        // Find and remove socket
        EnterCriticalSection(&criticalSection);

        int index;
        for (index = 0; index < nClients; index++) {
            if (clients[index]->socket == client->socket) {
                break;
            }
        }

        closesocket(clients[index]->socket);
        GlobalFree(clients[index]);
        closeSession(sessions, index);
        clients[index] = 0;

        for (int i = index; i < nClients - 1; i++) {
            clients[i] = clients[i + 1];
        }
        nClients--;

        LeaveCriticalSection(&criticalSection);

        return;
    }

    EnterCriticalSection(&criticalSection);
    int index;
    for (index = 0; index < nClients; index++) {
        if (clients[index]->socket == client->socket) {
            break;
        }
    }
    LeaveCriticalSection(&criticalSection);

    // Check to see if the bytesNeedToSend field equals zero. If this is so, then
    // this means a WSARecv call just completed so update the bytesNeedToSend field
    // with the transferredBytes value from the completed WSARecv() call
    if (client->operation == RECEIVE) {
        switch (sessions[index].phase) {
            case PHASE_RECVKEYSIZE: {
                client->buff[transferredBytes] = '\0';
                char sMode[OPCODE_SIZE + 1];
                strncpy_s(sMode, OPCODE_SIZE + 1, client->buff, OPCODE_SIZE);
                sessions[index].mode = strToNum(sMode);

                strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                client->bytesNeedToSend = strlen(client->buff);
                sessions[index].phase = PHASE_RECVKEY;
                break;
            }
            case PHASE_RECVKEY: {
                client->buff[transferredBytes] = '\0';
                sessions[index].key = strToNum(client->buff);
                strcpy_s(sessions[index].serverFilePath, BUFF_SIZE, generateTmpFileName());
                fopen_s(&sessions[index].pSer, sessions[index].serverFilePath, "wb");
                sessions[index].vBuff = malloc(BUFF_SIZE);

                strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                client->bytesNeedToSend = strlen(client->buff);
                sessions[index].phase = PHASE_RECVFILESIZE;
                break;
            }
            case PHASE_RECVFILESIZE: {
                client->buff[transferredBytes] = '\0';
                if (strcmp(client->buff, createOpcodeAndLength(OPCODE_TRANSFER, 0)) == 0) {
                    fclose(sessions[index].pSer);
                    fopen_s(&sessions[index].pSer, sessions[index].serverFilePath, "rb");
                    memset(sessions[index].vBuff, 0, BUFF_SIZE);

                    strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                    client->bytesNeedToSend = strlen(client->buff);
                    sessions[index].phase = PHASE_SENDFILESIZE;
                    break;
                }

                sessions[index].length = strToNum(client->buff + 1);
                memset(sessions[index].vBuff, 0, BUFF_SIZE);

                strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                client->bytesNeedToSend = strlen(client->buff);
                sessions[index].phase = PHASE_RECVFILE;
                break;
            }
            case PHASE_RECVFILE: {
                client->buff[transferredBytes] = '\0';
                fwrite(client->buff, 1, sessions[index].length, sessions[index].pSer);

                strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                client->bytesNeedToSend = strlen(client->buff);
                sessions[index].phase = PHASE_RECVFILESIZE;
                break;
            }
            case PHASE_SENDFILESIZE: {
                if (feof(sessions[index].pSer)) {
                    strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, 0));
                    client->bytesNeedToSend = strlen(client->buff);

                    clearSession(&sessions[index]);
                    break;
                }

                memset(sessions[index].vBuff, 0, BUFF_SIZE);
                sessions[index].length = fread(sessions[index].vBuff, 1, PAYLOAD_SIZE, sessions[index].pSer);
                if (sessions[index].mode == OPCODE_ENCODE) {
                    encode(sessions[index].key, sessions[index].vBuff, sessions[index].length);
                } else if (sessions[index].mode == OPCODE_DECODE) {
                    decode(sessions[index].key, sessions[index].vBuff, sessions[index].length);
                }

                strcpy_s(client->buff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, sessions[index].length));
                client->bytesNeedToSend = strlen(client->buff);
                sessions[index].phase = PHASE_SENDFILE;
                break;
            }
            case PHASE_SENDFILE: {
                memcpy_s(client->buff, BUFF_SIZE, sessions[index].vBuff, sessions[index].length);
                client->bytesNeedToSend = sessions[index].length;

                sessions[index].phase = PHASE_SENDFILESIZE;
                break;
            }
            default: {
                clearSession(&sessions[index]);
                break;
            }
        }
        client->sentBytes = 0;
        client->operation = SEND;
    } else {
        client->sentBytes += transferredBytes;
    }

    if (client->bytesNeedToSend > client->sentBytes) {
        // Post another WSASend() request.
        // Since WSASend() is not guaranteed to send all of the bytes requested,
        // continue posting WSASend() calls until all received bytes are sent
        ZeroMemory(&(client->overlapped), sizeof(WSAOVERLAPPED));
        client->dataBuf.buf = client->buff + client->sentBytes;
        client->dataBuf.len = client->bytesNeedToSend - client->sentBytes;
        client->operation = SEND;
        if (WSASend(client->socket, &(client->dataBuf), 1, &sendBytes, 0, &(client->overlapped), workerRoutine) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                printf("WSASend() failed with error %d\n", WSAGetLastError());
                closeSession(sessions, index);
                return;
            }
        }
    } else {
        // Now that there are no more bytes to send post another WSARecv() request
        client->bytesNeedToSend = 0;
        flags = 0;
        ZeroMemory(&(client->overlapped), sizeof(WSAOVERLAPPED));
        client->dataBuf.len = BUFF_SIZE;
        client->dataBuf.buf = client->buff;
        client->operation = RECEIVE;
        if (WSARecv(client->socket, &(client->dataBuf), 1, &bytesNeedToSend, &flags, &(client->overlapped), workerRoutine) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                printf("WSARecv() failed with error %d\n", WSAGetLastError());
                closeSession(sessions, index);
                return;
            }
        }
    }
}

/**
 * Count digits in a non-negative integer
 *
 * @param n integer need to count digits
 */
int countDigit(int n) {
    if (n == 0) {
        return 1;
    }

    int count = 0;
    while (n != 0) {
        n /= 10;
        ++count;
    }

    return count;
}

/**
 * Convert a non-negative number from int to const char *
 *
 * @param num number
 * @param fixedSize fixed size of number. If fixedSize is 0, there is no 0s in front of number
 * @return number in const char *
 */
const char *numToStr(int num, int fixedSize) {
    char *result = (char *)malloc(sizeof(char) * BUFF_SIZE), tmp[BUFF_SIZE];
    strcpy_s(result, BUFF_SIZE, "");

    if (fixedSize > 0) {
        int numLength = countDigit(num);
        for (int i = 0; i < fixedSize - numLength; ++i) {
            strcat_s(result, BUFF_SIZE, "0");
        }
    }

    sprintf_s(tmp, BUFF_SIZE, "%d", num);
    strcat_s(result, BUFF_SIZE, tmp);

    return result;
}

/**
 * Convert a non-negative integer from C string to int
 *
 * @param str pointer to the string that holds integer
 * @return integer in int type
 */
int strToNum(const char *str) {
    int res = 0, digit = 1;
    for (int i = strlen(str) - 1; i >= 0; --i) {
        res += digit * (int)(str[i] - '0');
        digit *= 10;
    }

    return res;
}

/**
 * Create a part of a message include opcode and length
 *
 * @param opcode opcode
 * @param length length of the payload
 * @return pointer to a C string holds message
 */
const char *createOpcodeAndLength(int opcode, int length) {
    char *res = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(res, BUFF_SIZE, "");
    strcat_s(res, BUFF_SIZE, numToStr(opcode, OPCODE_SIZE));
    strcat_s(res, BUFF_SIZE, numToStr(length, LENGTH_SIZE));

    return res;
}

/**
 * Generate a random name for temporary file in format "tmp_xxxxx"
 *
 * @return pointer to a C string holds generated name
 */
const char *generateTmpFileName() {
    char *res = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(res, BUFF_SIZE, "");
    strcat_s(res, BUFF_SIZE, "tmp_");
    strcat_s(res, BUFF_SIZE, numToStr(rand() % 100000, 5));

    return res;
}

/**
 * Encode an integer using shift cipher
 *
 * @param k key in range [0, 255]
 * @param c integer need to encode in range [-128, 127]
 * @return encoded integer in range [-128, 127]
 */
int encode(int k, int c) {
    int res = c + 128;
    res = (res + k) % 256;
    res -= 128;

    return res;
}

/**
 * Decode an integer using shift cipher
 *
 * @param k key in range [0, 255]
 * @param c integer need to decode in range [-128, 127]
 * @return decoded integer in range [-128, 127]
 */
int decode(int k, int c) {
    int res = c + 128;
    res = (res - k + 256) % 256;
    res -= 128;

    return res;
}

/**
 * Encode a data buffer
 *
 * @param k key
 * @param buff data buffer
 * @param length length of data buffer
 */
void encode(int k, void *buff, int length) {
    for (int i = 0; i < length; ++i) {
        ((char *)buff)[i] = encode(k, ((char *)buff)[i]);
    }
}

/**
 * Decode a data buffer
 *
 * @param k key
 * @param buff data buffer
 * @param length length of data buffer
 */
void decode(int k, void *buff, int length) {
    for (int i = 0; i < length; ++i) {
        ((char *)buff)[i] = decode(k, ((char *)buff)[i]);
    }
}

/**
 * Return to phase PHASE_RECVKEYSIZE,close file stream, and remove temporary file
 *
 * @param s pointer to the session need to clear
 */
void clearSession(Session *s) {
    s->phase = PHASE_RECVKEYSIZE;
    if (s->pSer != NULL) {
        fclose(s->pSer);
        s->pSer = NULL;
    }
    remove(s->serverFilePath);
}

/**
 * Remove a session from sessions array
 * @param sessions An array of sessions
 * @param n Index of the removed session
 */
void closeSession(Session sessions[], int n) {
    clearSession(&sessions[n]);

    for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; ++i) {
        sessions[i] = sessions[i + 1];
    }
}
