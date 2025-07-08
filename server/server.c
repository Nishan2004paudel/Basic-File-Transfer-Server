#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <winsock2.h> 
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#define PORT 12345
#define BUFFER_SIZE 4096

typedef struct {
    SOCKET client_sock;
    struct sockaddr_in client_addr;
} client_info_t;

unsigned __stdcall handle_client(void* arg) {
    client_info_t* client_info = (client_info_t*)arg;
    SOCKET client_sock = client_info->client_sock;
    struct sockaddr_in client_addr = client_info->client_addr;
    
    printf("Thread started for client %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char filename_buffer[BUFFER_SIZE];
    int bytes_received = recv(client_sock, filename_buffer, sizeof(filename_buffer) - 1, 0);
    if (bytes_received <= 0) {
        fprintf(stderr, "Error receiving filename or client disconnected prematurely: %d\n", WSAGetLastError());
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    filename_buffer[bytes_received] = '\0';
    printf("Client %s:%d requested file: '%s'\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename_buffer);

    FILE *file_to_send = fopen(filename_buffer, "rb");
    __int64 file_size = -1;
    
    if (file_to_send == NULL) {
        fprintf(stderr, "Error opening file '%s': %s\n", filename_buffer, strerror(errno));
        __int64 network_file_size = htonll(file_size);
        send(client_sock, (char*)&network_file_size, sizeof(network_file_size), 0);
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    
    if (_fseeki64(file_to_send, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error seeking to end of file '%s': %s\n", filename_buffer, strerror(errno));
        __int64 network_file_size = htonll(-1);
        send(client_sock, (char*)&network_file_size, sizeof(network_file_size), 0);
        fclose(file_to_send);
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    
    file_size = _ftelli64(file_to_send);
    
    if (_fseeki64(file_to_send, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to beginning of file '%s': %s\n", filename_buffer, strerror(errno));
        __int64 network_file_size = htonll(-1);
        send(client_sock, (char*)&network_file_size, sizeof(network_file_size), 0);
        fclose(file_to_send);
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    
    if (file_size == -1) {
        fprintf(stderr, "Error getting file size for '%s': %s\n", filename_buffer, strerror(errno));
        __int64 network_file_size = htonll(file_size);
        send(client_sock, (char*)&network_file_size, sizeof(network_file_size), 0);
        fclose(file_to_send);
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    
    printf("Client %s:%d - File '%s' found, size: %lld bytes.\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename_buffer, file_size);
    
    __int64 network_file_size = htonll(file_size);
    if (send(client_sock, (char*)&network_file_size, sizeof(network_file_size), 0) == SOCKET_ERROR) {
        fprintf(stderr, "Error sending file size to client: %d\n", WSAGetLastError());
        fclose(file_to_send);
        closesocket(client_sock);
        free(client_info);
        return 1;
    }
    
    char file_buffer[BUFFER_SIZE];
    size_t bytes_read_from_file;
    __int64 total_bytes_sent = 0;

    while ((bytes_read_from_file = fread(file_buffer, 1, sizeof(file_buffer), file_to_send)) > 0) {
        int bytes_sent_to_client = send(client_sock, file_buffer, bytes_read_from_file, 0);
        if (bytes_sent_to_client == SOCKET_ERROR) {
            fprintf(stderr, "Error sending file data to client %s:%d: %d\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), WSAGetLastError());
            break;
        }
        total_bytes_sent += bytes_sent_to_client;
        printf("\rClient %s:%d - Sent: %lld / %lld bytes", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), total_bytes_sent, file_size);
        fflush(stdout);
    }
    printf("\nClient %s:%d - File '%s' sent successfully. Total sent: %lld bytes.\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename_buffer, total_bytes_sent);

    fclose(file_to_send);
    closesocket(client_sock);
    printf("Client %s:%d disconnected.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    free(client_info);
    return 0;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock initialized.\n");

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "Error creating listening socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("Listening socket created.\n");

    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        fprintf(stderr, "setsockopt failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Error binding socket: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    printf("Socket bound to port %d.\n", PORT);

    if (listen(listen_sock, 5) == SOCKET_ERROR) {
        fprintf(stderr, "Error listening on socket: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    printf("Server listening for incoming connections...\n");
    
    SOCKET client_sock;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "Error accepting connection: %d\n", WSAGetLastError());
            continue;
        }
        printf("Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        client_info_t* client_info = (client_info_t*)malloc(sizeof(client_info_t));
        if (client_info == NULL) {
            fprintf(stderr, "Error allocating memory for client info: %s\n", strerror(errno));
            closesocket(client_sock);
            continue;
        }
        
        client_info->client_sock = client_sock;
        client_info->client_addr = client_addr;

        uintptr_t thread_handle = _beginthreadex(NULL, 0, handle_client, client_info, 0, NULL);
        if (thread_handle == 0) {
            fprintf(stderr, "Error creating thread for client: %s\n", strerror(errno));
            closesocket(client_sock);
            free(client_info);
            continue;
        }
        
        CloseHandle((HANDLE)thread_handle);

        printf("Spawned thread to handle client %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }

    closesocket(listen_sock);
    WSACleanup();
    printf("Server shut down.\n");
    return 0;
}