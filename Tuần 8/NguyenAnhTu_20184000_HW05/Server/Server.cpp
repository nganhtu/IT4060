// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define BUFF_SIZE 2048

#define ACC_ACTIVE '0'
#define ACC_INACTIVE '1'
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

typedef struct Session {
    SOCKET connSock;
    char *clientIP;
    int clientPort;
    char *username;
    bool isLoggedIn;
} Session;

/**
 * Log in with the given username in the given session.
 *
 * @param s the session to log in
 * @param username the username to log in with
 * @return 1 if the log in was successful, 0 otherwise
 */
int logIn(Session *s, const char *username) {
    if (!s->isLoggedIn) {
        s->isLoggedIn = true;
        s->username = (char *)malloc(sizeof(char) * BUFF_SIZE);
        strcpy_s(s->username, BUFF_SIZE, username);

        return 1;
    }

    return 0;
}

/**
 * Log out the given session.
 *
 * @param s the session to log out
 * @return 1 if the log out was successful, 0 otherwise
 */
int logOut(Session *s) {
    if (s->isLoggedIn) {
        free(s->username);
        s->isLoggedIn = false;

        return 1;
    }

    return 0;
}

/**
 * Solve the request from client and create server response code based on the given protocol.
 *
 * @param request request from client
 * @param s reference to the client session
 * @return server response code
 */
char *solveRequest(char *request, Session *s) {
    char *response = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(response, BUFF_SIZE, "");

    // Solve log in request
    if (strncmp(request, "USER ", strlen("USER ")) == 0) {
        if (s->isLoggedIn) {
            strcpy_s(response, BUFF_SIZE, RES_ALREADYLOGGEDIN);
        } else {
            FILE *accPtr;
            errno_t err = fopen_s(&accPtr, "account.txt", "r");
            if (accPtr == NULL) {
                printf("Error %d: cannot open account file\n", errno);
            } else {
                char line[BUFF_SIZE];
                bool accExist = false;
                while (fgets(line, BUFF_SIZE, accPtr) != NULL) {
                    size_t wsPos = 0;
                    while (wsPos < strlen(line)) {
                        if (line[wsPos] != ' ') {
                            wsPos++;
                        } else {
                            break;
                        }
                    }
                    char accState = line[wsPos + 1], _username[BUFF_SIZE] = "";
                    strncpy_s(_username, BUFF_SIZE, line, wsPos);
                    if (strcmp(_username, request + strlen("USER ")) == 0) {
                        accExist = true;
                        if (accState == ACC_INACTIVE) {
                            strcpy_s(response, BUFF_SIZE, RES_ACCOUNTLOCKED);
                        } else if (accState == ACC_ACTIVE) {
                            logIn(s, _username);
                            strcpy_s(response, BUFF_SIZE, RES_LOGINSUCCESS);
                        }
                        break;
                    }
                }
                if (!s->isLoggedIn && !accExist) {
                    strcpy_s(response, BUFF_SIZE, RES_ACCOUNTNOTEXIST);
                }
            }
            fclose(accPtr);
        }
    }

    // Solve post request
    else if (strncmp(request, "POST ", strlen("POST ")) == 0) {
        if (!s->isLoggedIn) {
            strcpy_s(response, BUFF_SIZE, RES_NOTLOGGEDIN);
        } else {
            strcpy_s(response, BUFF_SIZE, RES_POSTSUCCESS);
        }
    }

    // Solve log out request
    else if (strcmp(request, "BYE") == 0) {
        if (!s->isLoggedIn) {
            strcpy_s(response, BUFF_SIZE, RES_NOTLOGGEDIN);
        } else {
            logOut(s);
            strcpy_s(response, BUFF_SIZE, RES_LOGOUTSUCESS);
        }
    }

    // Solve unknown request
    else {
        strcpy_s(response, BUFF_SIZE, RES_REQUESTNOTFOUND);
    }

    return response;
}

int main(int argc, char *argv[]) {
    DWORD nEvents = 0, index;
    Session sessions[WSA_MAXIMUM_WAIT_EVENTS];
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

    sessions[0].connSock = listenSock;
    events[0] = WSACreateEvent();
    nEvents++;

    WSAEventSelect(sessions[0].connSock, events[0], FD_ACCEPT | FD_CLOSE);

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
        sessions[i].connSock = 0;
    }
    while (1) {
        // wait for network events on all socket
        index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
        if (index == WSA_WAIT_FAILED) {
            printf("Error %d: WSAWaitForMultipleEvents() failed\n", WSAGetLastError());
            break;
        }

        index = index - WSA_WAIT_EVENT_0;
        WSAEnumNetworkEvents(sessions[index].connSock, events[index], &sockEvent);

        if (sockEvent.lNetworkEvents & FD_ACCEPT) {
            if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
                printf("FD_ACCEPT failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
                break;
            }

            if ((connSock = accept(sessions[index].connSock, (sockaddr *)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
                printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
                break;
            }

            int i;
            if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
                printf("Too many clients\n");
                closesocket(connSock);
            } else {
                for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++)
                    if (sessions[i].connSock == 0) {
                        char clientIP[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
                        int clientPort = ntohs(clientAddr.sin_port);
                        printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);

                        sessions[i] = {connSock, clientIP, clientPort, NULL, false};
                        events[i] = WSACreateEvent();
                        WSAEventSelect(sessions[i].connSock, events[i], FD_READ | FD_CLOSE);
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
            ret = recv(sessions[index].connSock, recvBuff, BUFF_SIZE, 0);
            if (ret == SOCKET_ERROR || ret == 0) {
                printf("Error %d: cannot receive data from client\n", WSAGetLastError());
                closesocket(sessions[index].connSock);
                sessions[index] = {0, NULL, 0, NULL, false};
                WSACloseEvent(events[index]);
                nEvents--;
            } else {
                recvBuff[ret] = '\0';
                printf("Receive from client [%s:%d]: %s\n", sessions[index].clientIP, sessions[index].clientPort, recvBuff);

                // Solve request
                strcpy_s(sendBuff, BUFF_SIZE, solveRequest(recvBuff, &sessions[index]));

                // Send response
                ret = send(sessions[index].connSock, sendBuff, ret, 0);
                if (ret == SOCKET_ERROR) {
                    printf("Error %d: cannot send data\n", WSAGetLastError());
                }

                WSAResetEvent(events[index]);
            }
        }

        if (sockEvent.lNetworkEvents & FD_CLOSE) {
            if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
                if (sockEvent.iErrorCode[FD_CLOSE_BIT] == WSAECONNABORTED) {
                    printf("Connection from client [%s:%d] reset by peer\n", sessions[index].clientIP, sessions[index].clientPort);
                } else {
                    printf("FD_CLOSE failed with error %d\n", sockEvent.iErrorCode[FD_CLOSE_BIT]);
                    break;
                }
            }
            // Release socket and event
            sockEvent.iErrorCode[FD_CLOSE_BIT] = 0;
            closesocket(sessions[index].connSock);
            sessions[index] = {0, NULL, 0, NULL, false};
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
