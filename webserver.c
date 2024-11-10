// By Eymard Alarcon //

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

int request_count = 0;
size_t total_bytes_received = 0;
size_t total_bytes_sent = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void update_stats(size_t received, size_t sent) {
    pthread_mutex_lock(&stats_mutex);
    request_count++;
    total_bytes_received += received;
    total_bytes_sent += sent;
    pthread_mutex_unlock(&stats_mutex);
}

void serve_static(int client_socket, char *filepath) {
    char full_path[256] = "./static";
    strncat(full_path, filepath, sizeof(full_path) - strlen(full_path) - 1);

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, not_found, strlen(not_found), 0);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    char header[128];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", file_stat.st_size);
    send(client_socket, header, strlen(header), 0);

    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    close(file_fd);
}

void serve_stats(int client_socket) {
    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body>"
             "<h1>Server Statistics</h1>"
             "<p>Requests received: %d</p>"
             "<p>Total bytes received: %zu</p>"
             "<p>Total bytes sent: %zu</p>"
             "</body></html>",
             request_count, total_bytes_received, total_bytes_sent);
    send(client_socket, response, strlen(response), 0);
}

void serve_calc(int client_socket, char *query) {
    int a = 0, b = 0;
    sscanf(query, "a=%d&b=%d", &a, &b);
    int sum = a + b;

    char response[128];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"
             "Sum: %d", sum);
    send(client_socket, response, strlen(response), 0);
}

void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return NULL;
    }
    buffer[bytes_received] = '\0';

    char method[8], path[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, path, protocol);

    if (strcmp(method, "GET") == 0) {
        if (strncmp(path, "/static", 7) == 0) {
            serve_static(client_socket, path + 7);
        } else if (strcmp(path, "/stats") == 0) {
            serve_stats(client_socket);
        } else if (strncmp(path, "/calc?", 6) == 0) {
            serve_calc(client_socket, path + 6);
        } else {
            const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client_socket, not_found, strlen(not_found), 0);
        }
    }

    update_stats(bytes_received, strlen(buffer));
    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = 80;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server listening on port %d\n", port);

    while (1) {
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, NULL, NULL);
        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}
