// Server.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Server.h"
#include "winsock2.h"
#include "ws2tcpip.h"

#define WM_SOCKET WM_USER + 1
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define MAX_CLIENT 1024
#define BUFF_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

// Global Variables:
SOCKET client[MAX_CLIENT];
SOCKET listenSock;

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
    char recvBuff[BUFF_SIZE], sendBuff[BUFF_SIZE];

    switch (message) {
        case WM_SOCKET:
            if (WSAGETSELECTERROR(lParam)) {
                for (int i = 0; i < MAX_CLIENT; ++i) {
                    if (client[i] == (SOCKET)wParam) {
                        closesocket(client[i]);
                        client[i] = 0;
                    }
                }
            }

            switch (WSAGETSELECTEVENT(lParam)) {
                case FD_ACCEPT:
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

                case FD_READ:
                    for (i = 0; i < MAX_CLIENT; ++i) {
                        if (client[i] == (SOCKET)wParam) {
                            break;
                        }
                    }
                    // Receive request
                    ret = recv(client[i], recvBuff, BUFF_SIZE, 0);

                    if (ret > 0) {
                        // Solve request
                        memcpy(sendBuff, recvBuff, ret);
                        // strcpy_s(sendBuff, BUFF_SIZE, recvBuff);

                        // Send response
                        send(client[i], sendBuff, ret, 0);
                    }
                    break;

                case FD_CLOSE:
                    for (i = 0; i < MAX_CLIENT; ++i) {
                        if (client[i] == (SOCKET)wParam) {
                            closesocket(client[i]);
                            client[i] = 0;
                            break;
                        }
                    }
                    break;
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
