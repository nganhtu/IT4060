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

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6600
#define RECEIVE 0
#define SEND 1

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

typedef struct Session {
    int phase;
    int mode;
    int key;
    int length;
    char serverFilePath[MAX_PATH];
    void *vBuff;
    FILE *pSer;
} Session;

void freeSocketInfo(SocketInfo *siArray[], int n);
void closeEventInArray(WSAEVENT eventArr[], int n);
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

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

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
    Session sessions[WSA_MAXIMUM_WAIT_EVENTS];
    for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; ++i) {
        socks[i] = 0;
        events[i] = 0;
        sessions[i].phase = PHASE_RECVKEYSIZE;
        sessions[i].vBuff = malloc(BUFF_SIZE);
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
                sessions[i].phase = PHASE_RECVKEYSIZE;
                sessions[i].pSer = NULL;

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
                closeSession(sessions, index);
                client = 0;
                nEvents--;
                continue;
            }

            // Check to see if a WSARecv() call just completed
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
                        closeSession(sessions, index);
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
                        closeSession(sessions, index);
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
