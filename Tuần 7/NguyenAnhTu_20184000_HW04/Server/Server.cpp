// Server.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Server.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define WM_SOCKET WM_USER + 1
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

#pragma comment(lib, "ws2_32.lib")

typedef struct Session {
    char sBuff[BUFF_SIZE];
    void *vBuff;
    int phase;
    int mode;
    int key;
    int length;
    char serverFilePath[MAX_PATH];
    FILE *pSer;
} Session;

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

// Global Variables:
SOCKET client[MAX_CLIENT];
SOCKET listenSock;
Session session[MAX_CLIENT];

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    MyRegisterClass(hInstance);

    // Perform application initialization:
    HWND serverWindow;
    if ((serverWindow = InitInstance(hInstance, nCmdShow)) == NULL) {
        return FALSE;
    }

    // Step 1: Initiate WinSock
    WSADATA wsaData;
    WORD wVersion = MAKEWORD(2, 2);
    if (WSAStartup(wVersion, &wsaData)) {
        MessageBox(serverWindow, L"Winsock 2.2 is not supported", L"Error", MB_OK);
    }

    // Step 2: Construct socket
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // requests Windows message-based notification of network events for listenSock
    WSAAsyncSelect(listenSock, serverWindow, WM_SOCKET, FD_ACCEPT | FD_CLOSE | FD_READ);

    // Step 3: Bind address to socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);
    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
        MessageBox(serverWindow, L"Cannot associate a local address with server socket", L"Error", MB_OK);
    }

    // Step 4: Listen requests from clients
    if (listen(listenSock, MAX_CLIENT)) {
        MessageBox(serverWindow, L"Cannot place server socket in state LISTEN", L"Error", MB_OK);
    }

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SERVER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"WindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow) {
    for (int i = 0; i < MAX_CLIENT; ++i) {
        client[i] = 0;
        session[i].phase = PHASE_RECVKEYSIZE;
        session[i].vBuff = malloc(BUFF_SIZE);
    }

    HWND hWnd = CreateWindowW(L"WindowClass", L"Server", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SOCKET connSock;
    sockaddr_in clientAddr;
    int i, ret, clientAddrLen = sizeof(clientAddr);

    switch (message) {
        case WM_SOCKET:
            if (WSAGETSELECTERROR(lParam)) {
                for (int i = 0; i < MAX_CLIENT; ++i) {
                    if (client[i] == (SOCKET)wParam) {
                        closesocket(client[i]);
                        client[i] = 0;
                        clearSession(&session[i]);
                    }
                }
            }

            switch (WSAGETSELECTEVENT(lParam)) {
                case FD_ACCEPT: {
                    // Step 5: Communicate with client
                    connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
                    if (connSock == INVALID_SOCKET) {
                        MessageBox(hWnd, L"Cannot accept client connection", L"Error", MB_OK);
                        break;
                    }
                    for (i = 0; i < MAX_CLIENT; ++i) {
                        if (client[i] == 0) {
                            client[i] = connSock;
                            // requests Windows message-based notification of network events for connSock
                            WSAAsyncSelect(client[i], hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
                            break;
                        }
                    }
                    if (i == MAX_CLIENT) {
                        MessageBox(hWnd, L"Too many clients", L"Info", MB_OK);
                    }
                    break;
                }

                case FD_READ: {
                    for (i = 0; i < MAX_CLIENT; ++i) {
                        if (client[i] == (SOCKET)wParam) {
                            break;
                        }
                    }

                    switch (session[i].phase) {
                        case PHASE_RECVKEYSIZE: {
                            ret = recv(client[i], session[i].sBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }
                            session[i].sBuff[ret] = '\0';

                            char sMode[OPCODE_SIZE + 1];
                            strncpy_s(sMode, OPCODE_SIZE + 1, session[i].sBuff, OPCODE_SIZE);
                            session[i].mode = strToNum(sMode);

                            strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                            send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                            session[i].phase = PHASE_RECVKEY;
                            break;
                        }
                        case PHASE_RECVKEY: {
                            ret = recv(client[i], session[i].sBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }
                            session[i].sBuff[ret] = '\0';
                            session[i].key = strToNum(session[i].sBuff);

                            strcpy_s(session[i].serverFilePath, BUFF_SIZE, generateTmpFileName());
                            fopen_s(&session[i].pSer, session[i].serverFilePath, "wb");
                            session[i].vBuff = malloc(BUFF_SIZE);

                            strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                            send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                            session[i].phase = PHASE_RECVFILESIZE;
                            break;
                        }
                        case PHASE_RECVFILESIZE: {
                            ret = recv(client[i], session[i].sBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }
                            session[i].sBuff[ret] = '\0';
                            if (strcmp(session[i].sBuff, createOpcodeAndLength(OPCODE_TRANSFER, 0)) == 0) {
                                fclose(session[i].pSer);
                                fopen_s(&session[i].pSer, session[i].serverFilePath, "rb");
                                memset(session[i].vBuff, 0, BUFF_SIZE);

                                strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                                send(client[i], session[i].sBuff, BUFF_SIZE, 0);

                                session[i].phase = PHASE_SENDFILESIZE;
                                break;
                            }

                            session[i].length = strToNum(session[i].sBuff + 1);
                            memset(session[i].vBuff, 0, BUFF_SIZE);

                            strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                            send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                            session[i].phase = PHASE_RECVFILE;
                            break;
                        }
                        case PHASE_RECVFILE: {
                            ret = recv(client[i], (char *)session[i].vBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }
                            ((char *)session[i].vBuff)[ret] = '\0';

                            fwrite(session[i].vBuff, 1, session[i].length, session[i].pSer);

                            strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_ACK, 0));
                            send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                            session[i].phase = PHASE_RECVFILESIZE;
                            break;
                        }

                        case PHASE_SENDFILESIZE: {
                            ret = recv(client[i], session[i].sBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }

                            if (feof(session[i].pSer)) {
                                strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, 0));
                                send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                                clearSession(&session[i]);
                                break;
                            }

                            memset(session[i].vBuff, 0, BUFF_SIZE);
                            session[i].length = fread(session[i].vBuff, 1, PAYLOAD_SIZE, session[i].pSer);

                            if (session[i].mode == OPCODE_ENCODE) {
                                encode(session[i].key, session[i].vBuff, session[i].length);
                            } else if (session[i].mode == OPCODE_DECODE) {
                                decode(session[i].key, session[i].vBuff, session[i].length);
                            }

                            strcpy_s(session[i].sBuff, BUFF_SIZE, createOpcodeAndLength(OPCODE_TRANSFER, session[i].length));
                            send(client[i], session[i].sBuff, strlen(session[i].sBuff), 0);

                            session[i].phase = PHASE_SENDFILE;
                            break;
                        }
                        case PHASE_SENDFILE: {
                            ret = recv(client[i], session[i].sBuff, BUFF_SIZE, 0);
                            if (ret == 0 || WSAGetLastError() == WSAECONNRESET) {
                                clearSession(&session[i]);
                                break;
                            }

                            send(client[i], (char *)session[i].vBuff, session[i].length, 0);

                            session[i].phase = PHASE_SENDFILESIZE;
                            break;
                        }
                        default: {
                            clearSession(&session[i]);
                            break;
                        }
                    }

                    break;
                }

                case FD_CLOSE: {
                    for (i = 0; i < MAX_CLIENT; ++i) {
                        if (client[i] == (SOCKET)wParam) {
                            closesocket(client[i]);
                            client[i] = 0;
                            clearSession(&session[i]);
                            break;
                        }
                    }
                    break;
                }
            }

            break;  // break for WM_SOCKET

        case WM_DESTROY:
            PostQuitMessage(0);

            // Step 6: Close socket
            shutdown(listenSock, SD_BOTH);
            closesocket(listenSock);

            // Step 7: Terminate WinSock
            WSACleanup();
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
            break;
    }
    return 0;
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

// Generate a random name for temporary file, for example: "tmp_12345"
const char *generateTmpFileName() {
    char *res = (char *)malloc(sizeof(char) * BUFF_SIZE);
    strcpy_s(res, BUFF_SIZE, "");
    strcat_s(res, BUFF_SIZE, "tmp_");
    strcat_s(res, BUFF_SIZE, numToStr(rand() % 100000, 5));

    return res;
}

int encode(int k, int c) {
    int res = c + 128;
    res = (res + k) % 256;
    res -= 128;

    return res;
}

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

// Remove temporary file, close file stream, and return to phase PHASE_RECVKEYSIZE
void clearSession(Session *s) {
    s->phase = PHASE_RECVKEYSIZE;
    if (s->pSer != NULL) {
        fclose(s->pSer);
    }
    remove(s->serverFilePath);
}
