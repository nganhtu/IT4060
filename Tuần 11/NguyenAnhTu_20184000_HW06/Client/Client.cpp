// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6600

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

#pragma comment(lib, "ws2_32.lib")

long enterLong();
int enterMode();
int enterKey();
int countDigit(int);
const char *numToStr(int, int);
int strToNum(const char *);
const char *createOpcodeAndLength(int, int);

int main(int argc, char *argv[]) {
    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET client;
    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) {
        printf("Error %d: cannot create client socket\n", WSAGetLastError());
    }
    printf("Client started!\n");

    // (optional) Set time-out for receiving
    int tv = 10000;  // Time-out interval: 10000ms
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&tv), sizeof(int));

    // Step 3: Specify server address
    char serverAddrStr[BUFF_SIZE] = SERVER_ADDR;
    int serverPort = SERVER_PORT;
    if (argc != 3) {
        printf("Usage: UDP_Client <server_addr> <server_port>\n");
    } else {
        strcpy_s(serverAddrStr, BUFF_SIZE, argv[1]);
        serverPort = atoi(argv[2]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverAddrStr, &serverAddr.sin_addr);

    // Step 4: Connect to server
    if (connect(client, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        printf("Error %d: cannot connect to server\n", WSAGetLastError());
    }
    printf("Connected to server!\n");

    // Step 5: Communicate with server
    char sBuff[BUFF_SIZE];
    void *vBuff = malloc(BUFF_SIZE);
    int ret;
    while (1) {
        int mode = enterMode(), key = enterKey();

        strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(mode, countDigit(key)));
        send(client, sBuff, strlen(sBuff), 0);

        ret = recv(client, sBuff, BUFF_SIZE, 0);

        strcpy_s(sBuff, BUFF_SIZE, numToStr(key, 0));
        send(client, sBuff, strlen(sBuff), 0);

        ret = recv(client, sBuff, BUFF_SIZE, 0);
        sBuff[ret] = '\0';

        char clientFilePath[BUFF_SIZE];
        printf("Enter file path: ");
        gets_s(clientFilePath, BUFF_SIZE);

        FILE *pCli;
        errno_t err = fopen_s(&pCli, clientFilePath, "rb");

        while (!feof(pCli)) {
            memset(vBuff, 0, BUFF_SIZE);
            ret = fread(vBuff, 1, PAYLOAD_SIZE, pCli);

            strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, ret));
            send(client, sBuff, strlen(sBuff), 0);

            recv(client, sBuff, BUFF_SIZE, 0);

            send(client, (char *)vBuff, ret, 0);

            recv(client, sBuff, BUFF_SIZE, 0);
        }
        strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, 0));
        send(client, sBuff, strlen(sBuff), 0);

        recv(client, sBuff, BUFF_SIZE, 0);

        strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
        send(client, sBuff, strlen(sBuff), 0);

        fclose(pCli);

        if (mode == OPCODE_ENCODE) {
            strcat_s(clientFilePath, ".enc");
        } else if (mode == OPCODE_DECODE) {
            strcat_s(clientFilePath, ".dec");
        }
        err = fopen_s(&pCli, clientFilePath, "wb");

        while (1) {
            ret = recv(client, sBuff, BUFF_SIZE, 0);
            sBuff[ret] = '\0';

            if (strcmp(sBuff, createOpcodeAndLength(OPCODE_TRANSFER, 0)) == 0) {
                break;
            }

            int length = strToNum(sBuff + OPCODE_SIZE);

            strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
            send(client, sBuff, BUFF_SIZE, 0);

            memset(vBuff, 0, BUFF_SIZE);
            ret = recv(client, (char *)vBuff, BUFF_SIZE, 0);
            ((char *)vBuff)[ret] = '\0';

            strcpy_s(sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
            send(client, sBuff, BUFF_SIZE, 0);

            fwrite(vBuff, 1, length, pCli);
        }

        fclose(pCli);
    }  // end while

    // Step 6: Close socket
    closesocket(client);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}

/**
 * Safe way to get a long number from standard input.
 * This function will not stop until a valid long is entered.
 *
 * @return number from standard input
 */
long enterLong() {
    long n;
    char *p, s[20];

    while (fgets(s, sizeof(s), stdin)) {
        n = strtol(s, &p, 10);
        if (*p == '\n' && p != s) {
            break;
        }
    }

    return n;
}

// Safe way to ask user to enter mode
int enterMode() {
    int mode = OPCODE_INVALID;
    while (mode == OPCODE_INVALID) {
        printf("Choose mode:\t[%d] Encode\t[%d] Decode\n", OPCODE_ENCODE, OPCODE_DECODE);
        mode = enterLong();
        if (mode != OPCODE_ENCODE && mode != OPCODE_DECODE) {
            mode = OPCODE_INVALID;
        }
    }

    return mode;
}

// Safe way to ask user to enter key between [0, 255]
int enterKey() {
    int key = -1;
    while (key == -1) {
        printf("Enter key in range [0, 255]: ");
        key = enterLong();
        if (key < 0 || key > 255) {
            key = -1;
        }
    }

    return key;
}

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
 * Convert number from int to const char *
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

int strToNum(const char *str) {
    int res = 0, digit = 1;
    for (int i = strlen(str) - 1; i >= 0; --i) {
        res += digit * (int)(str[i] - '0');
        digit *= 10;
    }

    return res;
}

// Create opcode and length of a message, for example: "20000"
const char *createOpcodeAndLength(int opcode, int length) {
    char *res = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(res, BUFF_SIZE, "");
    strcat_s(res, BUFF_SIZE, numToStr(opcode, OPCODE_SIZE));
    strcat_s(res, BUFF_SIZE, numToStr(length, LENGTH_SIZE));

    return res;
}
