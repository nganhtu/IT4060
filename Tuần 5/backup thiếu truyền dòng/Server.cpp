// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "process.h"
#include <set>
#include <string>

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

std::set<std::string> loggedInUsers;

typedef struct Session
{
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
int logIn(Session *s, const char *username)
{
    if (!s->isLoggedIn)
    {
        s->isLoggedIn = true;
        s->username = (char *)malloc(sizeof(char) * BUFF_SIZE);
        strcpy_s(s->username, BUFF_SIZE, username);
        std::string s_username = username;
        loggedInUsers.insert(s_username);

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
int logOut(Session *s)
{
    if (s->isLoggedIn)
    {
        std::string s_username = s->username;
        loggedInUsers.erase(s_username);
        free(s->username);
        s->isLoggedIn = false;

        return 1;
    }

    return 0;
}

/**
 * Check if the given username is already logged in.
 *
 * @param username the username to check
 * @return 1 if the username is already logged in, 0 otherwise
 */
int loggedInFromAnotherPlace(const char *username)
{
    std::string s_username = username;

    return loggedInUsers.count(s_username);
}

/**
 * Solve the request from client and create server response code based on the given protocol.
 *
 * @param request request from client
 * @param s reference to the client session
 * @return server response code
 */
char *solveRequest(char *request, Session *s)
{
    char *response = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(response, BUFF_SIZE, "");

    // Solve log in request
    if (strncmp(request, "USER ", strlen("USER ")) == 0)
    {
        if (s->isLoggedIn)
        {
            strcpy_s(response, BUFF_SIZE, RES_ALREADYLOGGEDIN);
        }
        else
        {
            FILE *accPtr;
            errno_t err = fopen_s(&accPtr, "account.txt", "r");
            if (accPtr == NULL)
            {
                printf("Error %d: cannot open account file\n", errno);
            }
            else
            {
                char line[BUFF_SIZE];
                bool accExist = false;
                while (fgets(line, BUFF_SIZE, accPtr) != NULL)
                {
                    size_t wsPos = 0;
                    while (wsPos < strlen(line))
                    {
                        if (line[wsPos] != ' ')
                        {
                            wsPos++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    char accState = line[wsPos + 1], _username[BUFF_SIZE] = "";
                    strncpy_s(_username, BUFF_SIZE, line, wsPos);
                    if (strcmp(_username, request + strlen("USER ")) == 0)
                    {
                        accExist = true;
                        if (accState == ACC_INACTIVE)
                        {
                            strcpy_s(response, BUFF_SIZE, RES_ACCOUNTLOCKED);
                        }
                        else if (accState == ACC_ACTIVE)
                        {
                            if (loggedInFromAnotherPlace(_username))
                            {
                                strcpy_s(response, BUFF_SIZE, RES_LOGGEDINFROMANOTHERPLACE);
                            }
                            else
                            {
                                logIn(s, _username);
                                strcpy_s(response, BUFF_SIZE, RES_LOGINSUCCESS);
                            }
                        }
                        break;
                    }
                }
                if (!s->isLoggedIn && !accExist)
                {
                    strcpy_s(response, BUFF_SIZE, RES_ACCOUNTNOTEXIST);
                }
            }
            fclose(accPtr);
        }
    }

    // Solve post request
    else if (strncmp(request, "POST ", strlen("POST ")) == 0)
    {
        if (!s->isLoggedIn)
        {
            strcpy_s(response, BUFF_SIZE, RES_NOTLOGGEDIN);
        }
        else
        {
            strcpy_s(response, BUFF_SIZE, RES_POSTSUCCESS);
        }
    }

    // Solve log out request
    else if (strcmp(request, "BYE") == 0)
    {
        if (!s->isLoggedIn)
        {
            strcpy_s(response, BUFF_SIZE, RES_NOTLOGGEDIN);
        }
        else
        {
            logOut(s);
            strcpy_s(response, BUFF_SIZE, RES_LOGOUTSUCESS);
        }
    }

    // Solve unknown request
    else
    {
        strcpy_s(response, BUFF_SIZE, RES_REQUESTNOTFOUND);
    }

    return response;
}

/**
 * Thread to receive request from client, solve, and respond to client.
 *
 * @param param pointer to session that contains informations about client session
 * @return 0
 */
unsigned __stdcall commThread(void *param)
{
    Session s = *(Session *)param;
    char buff[BUFF_SIZE];
    int ret;

    while (1)
    {
        // Receive request from client
        ret = recv(s.connSock, buff, BUFF_SIZE, 0);
        if (ret == SOCKET_ERROR && WSAGetLastError() != WSAECONNRESET)
        {
            printf("Error %d: cannot receive data\n", WSAGetLastError());
            break;
        }
        else if (ret == 0 || WSAGetLastError() == WSAECONNRESET)
        {
            buff[0] = '\0';
            break;
        }
        else
        {
            buff[ret] = '\0';
            printf("Receive from client [%s:%d]: %s\n", s.clientIP, s.clientPort, buff);

            // Solve request
            strcpy_s(buff, BUFF_SIZE, solveRequest(buff, &s));

            // Send response to client
            ret = send(s.connSock, buff, strlen(buff), 0);
            if (ret == SOCKET_ERROR)
            {
                printf("Error %d: cannot send data\n", WSAGetLastError());
            }
        }
    } // end communicating

    logOut(&s);
    printf("Client [%s:%d] disconnected!\n", s.clientIP, s.clientPort);
    closesocket(s.connSock);

    return 0;
}

int main(int argc, char *argv[])
{
    // Step 1: Initiate Winsock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData))
    {
        printf("Winsock 2.2 is not supported\n");
    }

    // Step 2: Construct socket
    SOCKET listenSock;
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        printf("Error %d: cannot create server socket\n", WSAGetLastError());
    }

    // Step 3: Bind address to socket
    int serverPort = SERVER_PORT;
    if (argc != 2)
    {
        printf("Usage: UDP_Server <port>\n");
    }
    else
    {
        serverPort = atoi(argv[1]);
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        printf("Error %d: cannot bind this address\n", WSAGetLastError());
    }
    printf("Server started!\n");

    // Step 4: Listen request from client
    if (listen(listenSock, 10))
    {
        printf("Error %d: cannot place server socket in state LISTEN\n", WSAGetLastError());
    }

    // Step 5: Communicate with client
    SOCKET connSock;
    sockaddr_in clientAddr;
    char clientIP[INET_ADDRSTRLEN];
    int clientAddrLen = sizeof(clientAddr), clientPort;
    while (1)
    {
        connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
        if (connSock == SOCKET_ERROR)
        {
            printf("Error %d: cannot permit incoming connection\n", WSAGetLastError());
        }
        else
        {
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
            clientPort = ntohs(clientAddr.sin_port);
            printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);

            Session session = {connSock, clientIP, clientPort, NULL, false};

            _beginthreadex(0, 0, commThread, (void *)&session, 0, 0);
        }
    }

    // Step 6: Close socket
    closesocket(listenSock);

    // Step 7: Terminate Winsock
    WSACleanup();

    return 0;
}
