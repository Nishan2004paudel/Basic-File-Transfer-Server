#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#define PORT 12345
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock initialized.\n");

    SOCKET client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == INVALID_SOCKET) {
        fprintf(stderr, "Error creating client socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("Client socket created.\n");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address or address not supported.\n");
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Error connecting to server: %d\n", WSAGetLastError());
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }
    printf("Connected to server at %s:%d.\n", SERVER_IP, PORT);

    char filename_to_request[BUFFER_SIZE];
    printf("Enter filename to request from server: ");
    if (fgets(filename_to_request, sizeof(filename_to_request), stdin) == NULL) {
        perror("Error reading filename from input");
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }
    filename_to_request[strcspn(filename_to_request, "\n")] = 0;

    if (send(client_sock, filename_to_request, strlen(filename_to_request), 0) == SOCKET_ERROR) {
        fprintf(stderr, "Error sending filename request to server: %d\n", WSAGetLastError());
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }
    printf("Requested file: '%s'.\n", filename_to_request);

    __int64 file_size;
    int bytes_received_size = recv(client_sock, (char*)&file_size, sizeof(file_size), 0);
    if (bytes_received_size <= 0) {
        fprintf(stderr, "Error receiving file size from server or server disconnected: %d\n", WSAGetLastError());
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }

    file_size = ntohll(file_size);

    if (file_size == -1) {
        printf("Server reported: File '%s' not found.\n", filename_to_request);
        closesocket(client_sock);
        WSACleanup();
        return 0;
    }
    printf("Expecting file of size: %lld bytes.\n", file_size);

    char output_filename[BUFFER_SIZE];
    snprintf(output_filename, sizeof(output_filename), "received_%s", filename_to_request);

    FILE *output_file = fopen(output_filename, "wb");
    if (output_file == NULL) {
        perror("Error opening local file for writing");
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }
    printf("Local file '%s' opened for writing.\n", output_filename);

    char buffer[BUFFER_SIZE];
    __int64 total_bytes_received = 0;
    int current_bytes_received;

    while (total_bytes_received < file_size) {
        __int64 bytes_to_receive_this_chunk = file_size - total_bytes_received;
        if (bytes_to_receive_this_chunk > BUFFER_SIZE) {
            bytes_to_receive_this_chunk = BUFFER_SIZE;
        }

        current_bytes_received = recv(client_sock, buffer, (int)bytes_to_receive_this_chunk, 0);
        if (current_bytes_received <= 0) {
            fprintf(stderr, "Error receiving file data or server disconnected prematurely: %d\n", WSAGetLastError());
            fclose(output_file);
            closesocket(client_sock);
            WSACleanup();
            return 1;
        }

        fwrite(buffer, 1, current_bytes_received, output_file);
        total_bytes_received += current_bytes_received;

        printf("\rReceived: %lld / %lld bytes", total_bytes_received, file_size);
        fflush(stdout);
    }
    printf("\nFile '%s' received successfully. Total received: %lld bytes.\n", output_filename, total_bytes_received);

    fclose(output_file);
    closesocket(client_sock);
    WSACleanup();
    printf("Client disconnected.\n");
    return 0;
}