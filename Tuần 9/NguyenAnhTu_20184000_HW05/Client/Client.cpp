// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048

#define RES_LOGINSUCCESS "10"
#define RES_ACCOUNTLOCKED "11"
#define RES_ACCOUNTNOTEXIST "12"
#define RES_LOGGEDINFROMANOTHERPLACE "13"
#define RES_ALREADYLOGGEDIN "14"
#define RES_POSTSUCCESS "20"
#define RES_NOTLOGGEDIN "21"
#define RES_LOGOUTSUCESS "30"
#define RES_REQUESTNOTFOUND "99"

#pragma comment(lib, "ws2_32.lib")

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

/**
 * Ask the user to enter a request anh create a request message based on the given protocol.
 *
 * @return C string of user request
 */
char *createRequest() {
    char *request = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(request, BUFF_SIZE, "");

    int mode = 0;
    while (mode == 0) {
        printf("Choose mode:\t[1] Log in\t[2] Post\t[3] Log out\n");
        mode = enterLong();
        switch (mode) {
            default:
                mode = 0;
                break;
            case 1:  // Log in
                strcpy_s(request, BUFF_SIZE, "USER ");
                while (strlen(request) == strlen("USER ")) {
                    printf("Enter username: ");
                    gets_s(request + strlen(request), BUFF_SIZE - strlen(request));
                }
                break;
            case 2:  // Post
                strcpy_s(request, BUFF_SIZE, "POST ");
                while (strlen(request) == strlen("POST ")) {
                    printf("Enter article: ");
                    gets_s(request + strlen(request), BUFF_SIZE - strlen(request));
                }
                break;
            case 3:  // Log out
                strcpy_s(request, BUFF_SIZE, "BYE");
                break;
        }
    }

    return request;
}

/**
 * Print out a notification corresponding to the response code.
 *
 * @param responseCode server response code
 * @return void
 */
void explainResponseCode(char *responseCode) {
    if (strcmp(responseCode, RES_LOGINSUCCESS) == 0) {
        printf("Log in success\n");
    } else if (strcmp(responseCode, RES_ACCOUNTLOCKED) == 0) {
        printf("Error %s: account is locked\n", responseCode);
    } else if (strcmp(responseCode, RES_ACCOUNTNOTEXIST) == 0) {
        printf("Error %s: account does not exist\n", responseCode);
    } else if (strcmp(responseCode, RES_LOGGEDINFROMANOTHERPLACE) == 0) {
        printf("Error %s: logged in from another place\n", responseCode);
    } else if (strcmp(responseCode, RES_ALREADYLOGGEDIN) == 0) {
        printf("Error %s: already logged in\n", responseCode);
    } else if (strcmp(responseCode, RES_POSTSUCCESS) == 0) {
        printf("Post success\n");
    } else if (strcmp(responseCode, RES_NOTLOGGEDIN) == 0) {
        printf("Error %s: not logged in\n", responseCode);
    } else if (strcmp(responseCode, RES_LOGOUTSUCESS) == 0) {
        printf("Log out success\n");
    } else if (strcmp(responseCode, RES_REQUESTNOTFOUND) == 0) {
        printf("Error %s: request not found\n", responseCode);
    } else {
        printf("Error: unknown response code %s\n", responseCode);
    }
}

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
    char buff[BUFF_SIZE];
    while (1) {
        // Send request to server
        strcpy_s(buff, BUFF_SIZE, createRequest());

        int ret = send(client, buff, strlen(buff), 0);
        if (ret == SOCKET_ERROR) {
            printf("Error %d: cannot send message to server\n", WSAGetLastError());
        }

        // Receive response from server
        ret = recv(client, buff, BUFF_SIZE, 0);
        if (ret == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                printf("Error %d: time-out\n", WSAGetLastError());
            } else {
                printf("Error %d: cannot receive message from server\n", WSAGetLastError());
            }
        } else if (strlen(buff) > 0) {
            buff[ret] = '\0';
            explainResponseCode(buff);
        }
    }  // end while

    // Step 6: Close socket
    closesocket(client);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
